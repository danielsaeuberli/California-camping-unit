#!/usr/bin/env bash
# Turns a fresh Raspberry Pi OS Lite 64 bit (Bookworm) into the unit image (SPEC 2.10).
# Run on the Pi as root, once after first boot. Idempotent.
#
#   sudo ./build-image.sh [/dev/mmcblk0p3]
#
# The /data partition must exist (create it on the Mac before first boot, e.g. with
# a third ext4 partition of the remaining card space). Its device is the argument.
set -euo pipefail

DATA_DEV="${1:-/dev/mmcblk0p3}"
HERE="$(cd "$(dirname "$0")" && pwd)"
USER_NAME=camper

[[ $EUID -eq 0 ]] || { echo "run as root" >&2; exit 1; }

echo "== packages"
apt-get update
apt-get install -y --no-install-recommends cog python3-venv libgpiod2 curl rsync

echo "== remove / disable services (SPEC 2.4)"
for svc in bluetooth hciuart triggerhappy avahi-daemon ModemManager cups cups-browsed rsyslog \
           dphys-swapfile man-db.timer apt-daily.timer apt-daily-upgrade.timer NetworkManager \
           wpa_supplicant keyboard-setup console-setup; do
  systemctl disable --now "$svc" 2>/dev/null || true
done
apt-get purge -y dphys-swapfile 2>/dev/null || true
swapoff -a || true

echo "== user"
id "$USER_NAME" >/dev/null 2>&1 || useradd -m -s /bin/bash "$USER_NAME"
usermod -aG gpio,i2c,dialout,video,render,input "$USER_NAME"

echo "== /data partition"
mkdir -p /data
if ! grep -q " /data " /etc/fstab; then
  echo "$DATA_DEV /data ext4 defaults,noatime,commit=60 0 2" >> /etc/fstab
fi
mountpoint -q /data || mount /data
mkdir -p /data/releases /data/log /data/measure
chown -R "$USER_NAME:$USER_NAME" /data

echo "== boot config"
CFG=/boot/firmware/config.txt
if ! grep -q "Cali camping unit" "$CFG"; then
  cat "$HERE/config.txt" >> "$CFG"
fi
CMDLINE=/boot/firmware/cmdline.txt
for opt in quiet loglevel=3 logo.nologo consoleblank=0 fsck.repair=yes; do
  grep -qw -- "$opt" "$CMDLINE" || sed -i "1 s/\$/ $opt/" "$CMDLINE"
done

echo "== journald, network"
install -D -m 644 "$HERE/journald.conf" /etc/systemd/journald.conf.d/camper.conf
install -D -m 644 "$HERE/network/10-eth0-cerbo.network" /etc/systemd/network/10-eth0-cerbo.network
systemctl enable systemd-networkd

echo "== services"
install -m 644 "$HERE/systemd/camperd.service" /etc/systemd/system/
install -m 644 "$HERE/systemd/camper-ui.service" /etc/systemd/system/
install -m 644 "$HERE/systemd/camper-service-wifi.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable camperd camper-ui

echo "== ssh: keys only"
sed -i 's/^#\?PasswordAuthentication.*/PasswordAuthentication no/' /etc/ssh/sshd_config

echo "== read-only root (overlay); takes effect after reboot"
raspi-config nonint do_overlayfs 0

echo "done. Deploy a release from the Mac with 'make deploy', then reboot."
