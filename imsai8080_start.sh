#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="9999"
PROGRAM="$SCRIPT_DIR/imsai8080"

if ! command -v socat >/dev/null 2>&1; then
    echo "ERROR: socat is required to run the IMSAI 8080 server." >&2
    exit 1
fi

if [ ! -x "$PROGRAM" ]; then
    echo "ERROR: Program not found or not executable: $PROGRAM" >&2
    exit 1
fi

exec socat \
    TCP-LISTEN:${PORT},reuseaddr,fork \
    EXEC:"$PROGRAM",pty,stderr,setsid,sigint,sane
