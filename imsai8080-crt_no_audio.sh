#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

export WARGAMES_SILENT=1

stty erase '^H' 2>/dev/null || true

PROFILE="$SCRIPT_DIR/imsai8080.json"
COMMAND="$SCRIPT_DIR/imsai8080_no_audio.sh"

if [ "$(uname -s)" = "Darwin" ]; then
    CRT="/Applications/cool-retro-term.app/Contents/MacOS/cool-retro-term"

    if [ ! -x "$CRT" ]; then
        echo "ERROR: cool-retro-term is not installed in /Applications."
        echo "Run ./install.sh to install it."
        exit 1
    fi
else
    if ! command -v cool-retro-term >/dev/null 2>&1; then
        echo "ERROR: cool-retro-term is not installed."
        echo "Run ./install.sh to install it."
        exit 1
    fi

    CRT="$(command -v cool-retro-term)"
fi

exec "$CRT" \
    -geometry 1175x700 \
    -p "$PROFILE" \
    -e "$COMMAND"
