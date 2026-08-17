#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PID_DIR="$SCRIPT_DIR/.wargames-pids"
mkdir -p "$PID_DIR"

start_service() {
    local name="$1"
    local port="$2"
    local script="$3"
    local pidfile="$PID_DIR/${name}.pid"

    if [ ! -x "$script" ]; then
        echo "Skipping $name: $script not found or not executable."
        return
    fi

    if [ -f "$pidfile" ]; then
        local oldpid
        oldpid="$(cat "$pidfile" 2>/dev/null || true)"
        if [ -n "$oldpid" ] && kill -0 "$oldpid" 2>/dev/null; then
            echo "$name already running on Port $port (PID $oldpid)"
            return
        fi
        rm -f "$pidfile"
    fi

    echo "Starting $name Computer System on Port $port"
    "$script" >"$PID_DIR/${name}.log" 2>&1 &
    echo $! > "$pidfile"
}

start_service "Imsai-8080" "9999" "$SCRIPT_DIR/imsai8080_start.sh"
start_service "School" "9991" "$SCRIPT_DIR/school_start.sh"

# Bank support is retained if bank_start.sh exists in the repository.
start_service "Bank" "9992" "$SCRIPT_DIR/bank_start.sh"

start_service "Pan-AM" "9993" "$SCRIPT_DIR/pan-am_start.sh"
start_service "WOPR" "9994" "$SCRIPT_DIR/wopr_start.sh"

echo "Wargames servers started."
