#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$SCRIPT_DIR/.wargames-pids"

stop_service() {
    local name="$1"
    local pidfile="$PID_DIR/${name}.pid"

    if [ ! -f "$pidfile" ]; then
        echo "$name is not running (no PID file)."
        return
    fi

    local pid
    pid="$(cat "$pidfile" 2>/dev/null || true)"

    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        echo "Stopping $name (PID $pid)"
        kill "$pid" 2>/dev/null || true

        for _ in 1 2 3 4 5 6 7 8 9 10; do
            if ! kill -0 "$pid" 2>/dev/null; then
                break
            fi
            sleep 0.1
        done

        if kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    else
        echo "$name is not running."
    fi

    rm -f "$pidfile"
}

stop_service "Imsai-8080"
stop_service "School"
stop_service "Bank"
stop_service "Pan-AM"
stop_service "WOPR"

if [ -f "$SCRIPT_DIR/webllm/wopr_chat.py" ]; then
    python3 "$SCRIPT_DIR/webllm/wopr_chat.py" --stop >/dev/null 2>&1 || true
fi

echo "Wargames servers stopped."
