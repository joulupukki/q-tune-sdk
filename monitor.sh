#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
#
# monitor.sh — watch the Q-Tune's serial console (macOS / Linux). Zero install.
#
# The pedal logs boot and runtime messages over its USB-C port (the ESP32-S3
# native USB Serial/JTAG, 115200 baud). This streams that output using only
# built-in tools (stty + cat) — no ESP-IDF, Python, or Docker required.
#
# It identifies the pedal by USB vendor ID (Espressif = 0x303A) so it picks the
# right port even when other serial devices are present, and it RECONNECTS
# automatically: leave it running, power-cycle or replug the pedal, and it waits
# for the device to come back and resumes — so you can watch the boot logs.
#
# Why not Docker? Docker Desktop can't pass a USB serial device into its Linux VM
# on macOS/Windows, so a container can't see the port. See docs/MONITOR.md.
#
# Usage:
#   ./monitor.sh              # auto-detect the Q-Tune and follow its console
#   ./monitor.sh --list       # list detected serial ports and exit
#   ./monitor.sh /dev/cu.usbmodemXXXX   # pin a specific port (no auto-detect)
#
# Quit with Ctrl-C.

set -uo pipefail
BAUD=115200
POLL=0.2   # seconds between reconnect checks

# Espressif (native USB Serial/JTAG) and common USB-UART bridge vendor IDs, in
# preference order. Decimal, as ioreg/sysfs report them.
ESPRESSIF_VID=12346                 # 0x303A — the Q-Tune's own USB
BRIDGE_VIDS="4292 6790 1027 9025"   # CP210x, CH340, FTDI, Arduino

# Emit one "vid|pid|name|/dev/path" line per USB serial device (no Bluetooth).
gather_ports() {
  case "$(uname -s)" in
    Darwin)
      ioreg -r -c IOUSBHostDevice -l 2>/dev/null | awk '
        /"idVendor"/         { v=$NF+0 }
        /"idProduct"/        { p=$NF+0 }
        /"USB Product Name"/ { n=$0; sub(/.*= "/,"",n); sub(/".*/,"",n) }
        /IOCalloutDevice/    { if (match($0,/\/dev\/[^"]+/))
                                 printf "%d|%d|%s|%s\n", v, p, n, substr($0,RSTART,RLENGTH) }'
      ;;
    *)  # Linux
      local dev vid pid name
      for dev in /dev/ttyACM* /dev/ttyUSB*; do
        [ -e "$dev" ] || continue
        vid=""; pid=""; name=""
        if command -v udevadm >/dev/null 2>&1; then
          eval "$(udevadm info -q property -n "$dev" 2>/dev/null |
                  awk -F= '/^ID_VENDOR_ID=/{print "vid=0x"$2}
                           /^ID_MODEL_ID=/{print "pid=0x"$2}
                           /^ID_MODEL=/{print "name=\""$2"\""}')"
        fi
        printf "%d|%d|%s|%s\n" "$((${vid:-0}))" "$((${pid:-0}))" "${name:-serial}" "$dev"
      done
      ;;
  esac
}

list_ports() {
  local any=0 vid pid name dev
  while IFS='|' read -r vid pid name dev; do
    any=1
    printf "  %-22s vid=0x%04x pid=0x%04x  %s\n" "$dev" "$vid" "$pid" "$name"
  done < <(gather_ports)
  [ "$any" -eq 1 ] || echo "  (no USB serial devices found)"
}

# Pick the best port: Espressif first, then a known bridge, then (if exactly one
# serial device exists) that one. Echoes the path, or nothing if undecided.
detect_port() {
  local ports dev b
  ports="$(gather_ports)"
  [ -n "$ports" ] || return 1
  dev="$(printf '%s\n' "$ports" | awk -F'|' -v v="$ESPRESSIF_VID" '$1==v{print $4; exit}')"
  [ -n "$dev" ] && { printf '%s\n' "$dev"; return 0; }
  for b in $BRIDGE_VIDS; do
    dev="$(printf '%s\n' "$ports" | awk -F'|' -v v="$b" '$1==v{print $4; exit}')"
    [ -n "$dev" ] && { printf '%s\n' "$dev"; return 0; }
  done
  if [ "$(printf '%s\n' "$ports" | grep -c .)" -eq 1 ]; then
    printf '%s\n' "$ports" | awk -F'|' '{print $4}'; return 0
  fi
  return 1
}

# Explicit port if given, else auto-detect (re-runs every reconnect).
resolve_port() {
  if [ -n "$EXPLICIT_PORT" ]; then
    [ -e "$EXPLICIT_PORT" ] && printf '%s\n' "$EXPLICIT_PORT"
  else
    detect_port || true
  fi
}

# --- args --------------------------------------------------------------------
EXPLICIT_PORT=""
case "${1:-}" in
  -l|--list) echo "Detected serial ports:"; list_ports; exit 0 ;;
  -h|--help) sed -n '5,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  "")        : ;;
  *)         EXPLICIT_PORT="$1" ;;
esac

trap 'printf "\nstopped.\n"; exit 0' INT TERM

echo "Q-Tune serial monitor @ ${BAUD} baud — Ctrl-C to quit."
echo "Power-cycle or replug the pedal anytime; it reconnects and shows boot logs."
echo "------------------------------------------------------------"

# Reconnect loop: stream while the device is present; when it disconnects (or
# before it first appears), wait and re-detect. Re-detecting handles the port
# name changing across a re-enumeration.
announced_wait=0
while true; do
  PORT="$(resolve_port)"
  if [ -n "$PORT" ] && [ -e "$PORT" ]; then
    announced_wait=0
    echo "[connected: $PORT]"
    stty -f "$PORT" "$BAUD" 2>/dev/null || stty -F "$PORT" "$BAUD" 2>/dev/null || true
    # Streams until the device disconnects, then returns so we can reconnect.
    cat "$PORT" 2>/dev/null || true
    echo "[disconnected — waiting for the pedal to come back …]"
  else
    if [ "$announced_wait" -eq 0 ]; then
      echo "[waiting for the Q-Tune — connect it or power-cycle …]"
      announced_wait=1
    fi
    sleep "$POLL"
  fi
done
