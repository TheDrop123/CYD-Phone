#!/bin/bash

LOG=/tmp/live-glass.log
GLASS=/tmp/live-glass.png
GLASS_TMP=/tmp/live-glass-tmp.png
DELAY=2
export DISPLAY=:0

echo "[$(date '+%Y-%m-%d %H:%M:%S')] live-glass.sh started" >> "$LOG"

while true; do
    XA=$(ls /tmp/serverauth.* 2>/dev/null | head -1)
    [ -z "$XA" ] && XA=$(find /tmp -maxdepth 1 -name 'serverauth.*' -type f 2>/dev/null | head -1)
    export XAUTHORITY="$XA"

    GEO=$(xwininfo -root -tree 2>/dev/null | grep URxvt | head -1 | grep -oE '[0-9]+x[0-9]+[+-][0-9]+[+-][0-9]+' | head -1)
    if [ -n "$GEO" ]; then
        W=${GEO%x*}
        REST=${GEO#*x}
        H=${REST%%[+-]*}
        X=${REST#*[+-]}
        X=${X%%[+-]*}
        Y=${GEO##*[+-]}

        if convert x:"root[${W}x${H}+${X}+${Y}]" \
            -brightness-contrast -35x0 \
            -fill '#7a9c7a' -colorize 50% \
            -blur 0x1.5 \
            "$GLASS_TMP" 2>/dev/null; then
            mv -f "$GLASS_TMP" "$GLASS"
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] cropped ${W}x${H}+${X}+${Y}" >> "$LOG"
        fi
    else
        convert x:'root' -resize '480x320!' \
            -brightness-contrast -35x0 \
            -fill '#7a9c7a' -colorize 50% \
            -blur 0x1.5 \
            "$GLASS_TMP" 2>/dev/null && mv -f "$GLASS_TMP" "$GLASS"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] fallback" >> "$LOG"
    fi

    pkill -USR1 urxvt 2>/dev/null
    sleep "$DELAY"
done
