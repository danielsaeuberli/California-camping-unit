#!/usr/bin/env bash
# Network setup Pi 5 <-> Victron Cerbo GX (SPEC 2.7, HARDWARE.md 5.3). Run on the Pi as root.
#
#   sudo deploy/network/setup-network.sh "<venus-SSID>" "<password from the Cerbo label>"
#
# - wlan0 -> Cerbo access point (always on). Never the default route: control traffic to the
#   Cerbo must not depend on the camper router.
# - eth0  -> camper router LAN port (router is only on sometimes): DHCP, default route only
#   while the router is there; for SSH / remote access to the Pi.
# The Cerbo cannot bridge or route between its Wi-Fi and Ethernet; nothing here tries to.
# The password is passed in and stored only in NetworkManager's root-only connection file.
set -euo pipefail

SSID="${1:?usage: setup-network.sh <venus-SSID> <password>}"
PSK="${2:?usage: setup-network.sh <venus-SSID> <password>}"
WIRED="${WIRED_CON:-Wired connection 1}"

[[ $EUID -eq 0 ]] || { echo "run as root" >&2; exit 1; }

echo "== Wi-Fi -> Cerbo access point"
nmcli radio wifi on
if nmcli -t -f NAME con show | grep -qx cerbo-ap; then
  nmcli con modify cerbo-ap 802-11-wireless.ssid "$SSID" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$PSK"
else
  nmcli dev wifi rescan ifname wlan0 || true
  nmcli dev wifi connect "$SSID" password "$PSK" ifname wlan0 name cerbo-ap
fi
# powersave 2 = off: avoids AP link drops on the Pi
nmcli con modify cerbo-ap \
  ipv4.never-default yes ipv6.method disabled \
  connection.autoconnect yes connection.autoconnect-retries 0 \
  802-11-wireless.powersave 2

echo "== Ethernet -> camper router (DHCP, default route only when present)"
nmcli con modify "$WIRED" ipv4.method auto ipv4.route-metric 100 connection.autoconnect yes

nmcli con up cerbo-ap || true
nmcli con up "$WIRED" || true

echo "== Result"
ip -4 addr show wlan0 | sed -n 's/ *inet /wlan0 /p'
ip route show dev wlan0
echo "Expected Cerbo address on the AP network: 172.24.24.1. Confirm it from the route above,"
echo "then check MQTT:  python3 tools/cerbo_check.py --host <address>"
