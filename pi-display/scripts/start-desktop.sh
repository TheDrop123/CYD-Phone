#!/bin/bash
# Boot script: waits for IP + display, then starts X

LOG=/tmp/desktop-boot.log
exec > "$LOG" 2>&1

echo "[$(date)] Waiting for network IP..."
for i in $(seq 1 30); do
    IP=$(ip -4 addr show | grep -oP 'inet \K[0-9.]+' | grep -v '^127\.')
    if [ -n "$IP" ]; then
        echo "[$(date)] IP: $IP"
        break
    fi
    sleep 2
done

echo "[$(date)] Waiting for ili9486 DRM device..."
for i in $(seq 1 20); do
    if [ -e /dev/dri/card0 ]; then
        echo "[$(date)] DRM device ready"
        break
    fi
    sleep 1
done

echo "[$(date)] Starting X..."
startx -- -logfile /tmp/xorg.log
