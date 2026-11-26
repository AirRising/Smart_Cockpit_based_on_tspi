#!/bin/bash
BIN=/home/lckfb/b/smart-cockpit
LOG=/home/lckfb/lab/logs
SENTINEL=/home/lckfb/lab/disable
mkdir -p $LOG
export DISPLAY=:0 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1
[ -x "$BIN" ] || exit 0
while true; do
    if [ -e "$SENTINEL" ]; then sleep 5; continue; fi
    "$BIN" --sim --camera /dev/video0 --log-dir "$LOG" >> /home/lckfb/lab/autostart.log 2>&1
    sleep 2
done
