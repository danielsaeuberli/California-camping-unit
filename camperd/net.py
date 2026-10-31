"""Network settings of the unit (expert view -> Netzwerk), SPEC 2.7.

- Ethernet ``eth0`` to the camper router: DHCP or static (address/prefix, gateway, DNS).
- Wi-Fi ``wlan0`` to the Cerbo access point: SSID and password. Never the default route.
- Cerbo address (MQTT host/port) on the access-point network.

camperd validates the settings and stores them in ``/data/network.json`` (the root file
system is read-only). The root service ``camper-network`` applies them with nmcli at boot
and on request (``deploy/network/apply_network.py``); camperd may start only that unit.
The Wi-Fi password is never sent to the UI.
"""

from __future__ import annotations

import asyncio
import contextlib
import copy
import ipaddress
import json
import logging
import os
import shutil
from collections.abc import Callable
from pathlib import Path
from typing import Any

from camperd.profile import Profile

log = logging.getLogger(__name__)

WIRED_CON = "Wired connection 1"
WIFI_CON = "cerbo-ap"
APPLY_UNIT = "camper-network.service"


class NetError(ValueError):
    pass


def defaults(profile: Profile) -> dict[str, Any]:
    return {
        "eth": {"mode": "dhcp", "address": "", "gateway": "", "dns": []},
        "wifi": {"ssid": "", "psk": ""},
        "cerbo": {
            "host": str(profile.get("buses.cerbo.host", "172.24.24.1")),
            "port": int(profile.get("buses.cerbo.port", 1883)),
        },
    }


def validate(cfg: dict[str, Any]) -> dict[str, Any]:
    """Full, normalised configuration; raises NetError with a UI reason key."""
    eth, wifi, cerbo = cfg.get("eth", {}), cfg.get("wifi", {}), cfg.get("cerbo", {})
    mode = eth.get("mode")
    if mode not in ("dhcp", "static"):
        raise NetError("net_eth_mode")
    out_eth: dict[str, Any] = {"mode": mode, "address": "", "gateway": "", "dns": []}
    if mode == "static":
        try:
            iface = ipaddress.IPv4Interface(str(eth.get("address", "")))
        except ValueError as exc:
            raise NetError("net_eth_address") from exc
        net = iface.network
        if not 8 <= net.prefixlen <= 30 or iface.ip in (net.network_address, net.broadcast_address):
            raise NetError("net_eth_address")
        gw_raw = str(eth.get("gateway", "")).strip()
        if gw_raw:
            try:
                gw = ipaddress.IPv4Address(gw_raw)
            except ValueError as exc:
                raise NetError("net_eth_gateway") from exc
            if gw not in iface.network or gw == iface.ip:
                raise NetError("net_eth_gateway")
        dns = [d for d in (str(x).strip() for x in eth.get("dns", [])) if d]
        if len(dns) > 3:
            raise NetError("net_eth_dns")
        try:
            dns = [str(ipaddress.IPv4Address(d)) for d in dns]
        except ValueError as exc:
            raise NetError("net_eth_dns") from exc
        out_eth.update(address=str(iface), gateway=gw_raw, dns=dns)

    ssid = str(wifi.get("ssid", ""))
    if not 1 <= len(ssid.encode()) <= 32:
        raise NetError("net_wifi_ssid")
    psk = str(wifi.get("psk", ""))
    if not 8 <= len(psk) <= 63 or not psk.isprintable():
        raise NetError("net_wifi_psk")

    try:
        host = str(ipaddress.IPv4Address(str(cerbo.get("host", "")).strip()))
    except ValueError as exc:
        raise NetError("net_cerbo_host") from exc
    port = int(cerbo.get("port", 1883))
    if not 1 <= port <= 65535:
        raise NetError("net_cerbo_port")
    if mode == "static" and ipaddress.IPv4Address(host) in ipaddress.IPv4Interface(out_eth["address"]).network:
        raise NetError("net_cerbo_on_eth")  # the Cerbo is reached over Wi-Fi, never via the router
    return {"eth": out_eth, "wifi": {"ssid": ssid, "psk": psk}, "cerbo": {"host": host, "port": port}}


def nm_commands(cfg: dict[str, Any], wired: str = WIRED_CON, existing_wifi: bool = True) -> list[list[str]]:
    """nmcli argument lists (no shell) that apply a validated configuration."""
    w = cfg["wifi"]
    wifi_opts = [
        "802-11-wireless.ssid", w["ssid"], "wifi-sec.key-mgmt", "wpa-psk", "wifi-sec.psk", w["psk"],
        "ipv4.method", "auto", "ipv4.never-default", "yes", "ipv6.method", "disabled",
        "connection.autoconnect", "yes", "connection.autoconnect-retries", "0", "802-11-wireless.powersave", "2",
    ]
    cmds: list[list[str]] = []
    if existing_wifi:
        cmds.append(["nmcli", "con", "modify", WIFI_CON, *wifi_opts])
    else:
        cmds.append(["nmcli", "con", "add", "type", "wifi", "ifname", "wlan0", "con-name", WIFI_CON, *wifi_opts])
    e = cfg["eth"]
    if e["mode"] == "dhcp":
        eth_opts = ["ipv4.method", "auto", "ipv4.addresses", "", "ipv4.gateway", "", "ipv4.dns", ""]
    else:
        eth_opts = ["ipv4.method", "manual", "ipv4.addresses", e["address"],
                    "ipv4.gateway", e["gateway"], "ipv4.dns", ",".join(e["dns"])]
    eth_opts += ["ipv4.route-metric", "100", "connection.autoconnect", "yes"]
    cmds.append(["nmcli", "con", "modify", wired, *eth_opts])
    cmds.append(["nmcli", "-w", "10", "con", "up", WIFI_CON])
    cmds.append(["nmcli", "-w", "10", "con", "up", wired])
    return cmds


class NetworkSettings:
    def __init__(self, path: Path, profile: Profile, apply: Callable[[], Any] | None = None) -> None:
        self.path = path
        self.cfg = defaults(profile)
        self.apply_fn = apply
        self.on_change: list[Callable[[dict[str, Any]], Any]] = []
        with contextlib.suppress(OSError, ValueError):
            stored = json.loads(path.read_text())
            merged = copy.deepcopy(self.cfg)
            for k in merged:
                merged[k].update(stored.get(k, {}))
            self.cfg = validate(merged) if merged["wifi"]["ssid"] else merged

    @property
    def configured(self) -> bool:
        return bool(self.cfg["wifi"]["ssid"])

    def public(self) -> dict[str, Any]:
        out = copy.deepcopy(self.cfg)
        out["wifi"] = {"ssid": self.cfg["wifi"]["ssid"], "psk_set": bool(self.cfg["wifi"]["psk"])}
        return out

    async def update(self, changes: dict[str, Any]) -> dict[str, Any]:
        """Merge, validate, store, apply. An omitted/empty password keeps the stored one."""
        merged = copy.deepcopy(self.cfg)
        for section in ("eth", "wifi", "cerbo"):
            part = changes.get(section, {})
            if not isinstance(part, dict):
                raise NetError("bad_message")
            unknown = set(part) - set(merged[section])
            if unknown:
                raise NetError("bad_message")
            merged[section].update(part)
        if not changes.get("wifi", {}).get("psk"):
            merged["wifi"]["psk"] = self.cfg["wifi"]["psk"]
        new = validate(merged)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(new, indent=2))
        os.chmod(tmp, 0o600)  # contains the Wi-Fi password
        tmp.replace(self.path)
        old, self.cfg = self.cfg, new
        if self.apply_fn is not None:
            await self.apply_fn()
        if old["cerbo"] != new["cerbo"]:
            for cb in self.on_change:
                res = cb(new)
                if asyncio.iscoroutine(res):
                    await res
        return self.public()


async def apply_hw() -> None:
    """Start the root oneshot service that runs nmcli (polkit allows only this unit)."""
    proc = await asyncio.create_subprocess_exec(
        "systemctl", "start", "--no-block", APPLY_UNIT,
        stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.PIPE,
    )
    _, err = await proc.communicate()
    if proc.returncode != 0:
        log.error("starting %s failed: %s", APPLY_UNIT, err.decode(errors="replace").strip())
        raise NetError("net_apply_failed")
    log.info("network settings handed to %s", APPLY_UNIT)


async def apply_sim() -> None:
    log.info("sim: network settings stored, nothing applied")


async def status() -> dict[str, Any]:
    """Current IPv4 addresses of eth0/wlan0 and the default route (read only, no root)."""
    out: dict[str, Any] = {"eth0": None, "wlan0": None, "default_via": None}
    if not shutil.which("ip"):
        return out
    try:
        proc = await asyncio.create_subprocess_exec("ip", "-j", "-4", "addr", stdout=asyncio.subprocess.PIPE)
        raw, _ = await asyncio.wait_for(proc.communicate(), 2)
        for link in json.loads(raw or b"[]"):
            if link.get("ifname") in ("eth0", "wlan0"):
                info = [a for a in link.get("addr_info", []) if a.get("family") == "inet"]
                addrs = [f"{a['local']}/{a['prefixlen']}" for a in info]
                out[link["ifname"]] = addrs[0] if addrs else None
        proc = await asyncio.create_subprocess_exec(
            "ip", "-j", "-4", "route", "show", "default", stdout=asyncio.subprocess.PIPE
        )
        raw, _ = await asyncio.wait_for(proc.communicate(), 2)
        routes = json.loads(raw or b"[]")
        if routes:
            out["default_via"] = f"{routes[0].get('gateway', '?')} ({routes[0].get('dev', '?')})"
    except (OSError, ValueError, TimeoutError):
        pass
    return out
