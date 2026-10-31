#!/usr/bin/env bash
# First network setup without the touchscreen (SPEC 2.7, HARDWARE.md 5.3). Run on the Pi as root.
#
#   sudo deploy/network/setup-network.sh "<venus-SSID>" "<password from the Cerbo label>"
#
# Writes /data/network.json (Ethernet DHCP, Wi-Fi to the Cerbo AP, Cerbo 172.24.24.1) and
# applies it. Later changes: expert view -> Netzwerk on the unit.
set -euo pipefail
SSID="${1:?usage: setup-network.sh <venus-SSID> <password>}"
PSK="${2:?usage: setup-network.sh <venus-SSID> <password>}"
[[ $EUID -eq 0 ]] || { echo "run as root" >&2; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
nmcli radio wifi on
python3 -I "$HERE/apply_network.py" --ssid "$SSID" --psk "$PSK"
ip -4 addr show wlan0 | sed -n 's/ *inet /wlan0 /p'
echo "Check MQTT: python3 tools/cerbo_check.py"
