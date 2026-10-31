"""Victron values from the Cerbo GX (read only), prepared for the UI.

Every value keeps its receive time; a missing value stays ``None`` (never a substitute).
"""

from __future__ import annotations

from collections import deque
from typing import Any

from camperd.clock import Clock

MULTIPLUS_VA = 1600

SYSTEM_PATHS = {
    "soc": "system/0/Dc/Battery/Soc",
    "battery_v": "system/0/Dc/Battery/Voltage",
    "battery_a": "system/0/Dc/Battery/Current",
    "battery_w": "system/0/Dc/Battery/Power",
    # Remaining time in s from the battery monitor (SmartShunt/BMV); absent without one.
    "time_to_go_s": "system/0/Dc/Battery/TimeToGo",
    "pv_w": "system/0/Dc/Pv/Power",
    "charger_w": "system/0/Dc/Charger/Power",
    # Sum of the alternator services (Orion XS 12/12-50A DC-DC charger, VRM instance 277).
    # Confirmed for Venus 3.85 (O21).
    "alternator_w": "system/0/Dc/Alternator/Power",
    # Not in the SPEC 5 list; needed for the "12-V-Verbraucher" row of the prototype.
    "dc_system_w": "system/0/Dc/System/Power",
}
VEBUS_PATHS = {
    "state": "State",
    "mode": "Mode",
    "current_limit": "Ac/In/1/CurrentLimit",
    "current_limit_adjustable": "Ac/In/1/CurrentLimitIsAdjustable",
    "active_input": "Ac/ActiveIn/ActiveInput",
    "in_v": "Ac/ActiveIn/L1/V",
    "in_i": "Ac/ActiveIn/L1/I",
    "in_f": "Ac/ActiveIn/L1/F",
    "in_p": "Ac/ActiveIn/L1/P",
    "out_v": "Ac/Out/L1/V",
    "out_i": "Ac/Out/L1/I",
    "out_f": "Ac/Out/L1/F",
    "out_p": "Ac/Out/L1/P",
    "out_s": "Ac/Out/L1/S",
    "measurement_type": "Ac/PowerMeasurementType",
    "dc_w": "Dc/0/Power",
    "alarm_overload": "Alarms/L1/Overload",
    "alarm_low_battery": "Alarms/LowBattery",
    "alarm_temperature": "Alarms/HighTemperature",
    "alarm_ripple": "Alarms/L1/Ripple",
}
# Venus OS temperature services (``com.victronenergy.temperature``), e.g. a Ruuvi tag the
# Cerbo receives over Bluetooth. Display only: nothing switches on these values.
TEMPERATURE_PATHS = {
    "temp_c": "Temperature",
    "humidity": "Humidity",
    "name": "CustomName",
    "product": "ProductName",
    "type": "TemperatureType",
}


class VictronStore:
    """Holds the latest value per key with its receive time (monotonic)."""

    def __init__(self, clock: Clock, stale_s: float = 10.0) -> None:
        self.clock = clock
        self.stale_s = stale_s
        self._values: dict[str, tuple[Any, float]] = {}
        self.connected = False
        self.portal_id: str | None = None
        self.vebus_instance: str | None = None
        self._by_path = {p: k for k, p in SYSTEM_PATHS.items()}
        # (vebus path, value) waiting to be written by the Cerbo client (see victron_control)
        self.pending_writes: deque[tuple[str, Any]] = deque()
        # temperature services by instance: key -> (value, receive time)
        self.temperatures: dict[str, dict[str, tuple[Any, float]]] = {}

    def ingest_system(self, path: str, value: Any) -> None:
        key = self._by_path.get(path)
        if key is not None:
            self._values[key] = (value, self.clock.now())

    def ingest_vebus(self, path: str, value: Any) -> None:
        for key, p in VEBUS_PATHS.items():
            if p == path:
                self._values["vebus." + key] = (value, self.clock.now())
                return

    def ingest_temperature(self, instance: str, path: str, value: Any) -> None:
        for key, p in TEMPERATURE_PATHS.items():
            if p == path:
                self.temperatures.setdefault(instance, {})[key] = (value, self.clock.now())
                return

    def _temp_get(self, instance: str, key: str) -> Any:
        item = self.temperatures.get(instance, {}).get(key)
        if item is None or self.clock.now() - item[1] > self.stale_s:
            return None
        return item[0]

    def find_temperature(self, custom_name: str | None) -> tuple[str | None, str]:
        """Instance of the interior sensor and a state (ok|missing|ambiguous).

        With ``custom_name`` the service whose CustomName matches (case-insensitive) is used.
        Without one, the only Ruuvi tag the Cerbo reports.
        """
        live = {i for i in self.temperatures if self._temp_get(i, "temp_c") is not None
                or self._temp_get(i, "name") is not None}
        if custom_name:
            hits = [i for i in live if str(self._temp_get(i, "name") or "").strip().lower()
                    == custom_name.strip().lower()]
        else:
            # OPEN: O20 - no sensor name in the profile; fall back to the single Ruuvi tag.
            hits = [i for i in live if "ruuvi" in str(self._temp_get(i, "product") or "").lower()]
        if len(hits) > 1:
            return None, "ambiguous"
        return (hits[0], "ok") if hits else (None, "missing")

    def interior(self, custom_name: str | None) -> dict[str, Any]:
        """Interior temperature for the UI; ``c`` stays None unless a fresh reading exists."""
        out: dict[str, Any] = {"c": None, "rh": None, "name": None, "q": "missing"}
        if not self.connected:
            out["q"] = "offline"
            return out
        inst, state = self.find_temperature(custom_name)
        out["q"] = state
        if inst is None:
            return out
        t, rh = self._temp_get(inst, "temp_c"), self._temp_get(inst, "humidity")
        out["name"] = self._temp_get(inst, "name")
        if not isinstance(t, (int, float)) or isinstance(t, bool):
            out["q"] = "stale"
            return out
        out["c"] = round(float(t), 1)
        if isinstance(rh, (int, float)) and not isinstance(rh, bool):
            out["rh"] = round(float(rh))
        return out

    def get(self, key: str) -> Any:
        """Value if present and fresh, else ``None``."""
        item = self._values.get(key)
        if item is None or self.clock.now() - item[1] > self.stale_s:
            return None
        return item[0]

    def _entry(self, key: str) -> dict[str, Any] | None:
        item = self._values.get(key)
        if item is None:
            return None
        return {"val": item[0], "age_s": round(self.clock.now() - item[1], 1)}

    def snapshot(self) -> dict[str, Any]:
        data: dict[str, Any] = {k: self._entry(k) for k in SYSTEM_PATHS}
        vb = {k: self._entry("vebus." + k) for k in VEBUS_PATHS}
        out_s = self.get("vebus.out_s")
        data["vebus"] = vb
        data["load_pct"] = round(out_s / MULTIPLUS_VA * 100) if isinstance(out_s, (int, float)) else None
        data["connected"] = self.connected
        data["portal_id"] = self.portal_id
        data["stale_s"] = self.stale_s
        return data

    def battery_v(self) -> float | None:
        v = self.get("battery_v")
        return float(v) if isinstance(v, (int, float)) else None
