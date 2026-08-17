#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOME_PAGE="${SCRIPT_DIR}/intranet/index.html"

clear

if ! command -v lynx >/dev/null 2>&1; then
    echo "Lynx is not installed."
    if [ "$(uname -s)" = "Darwin" ]; then
        echo "Install with: brew install lynx"
    else
        echo "Install Lynx using your package manager."
    fi
    sleep 3
elif [ ! -f "$HOME_PAGE" ]; then
    echo "Local intranet is not installed:"
    echo "$HOME_PAGE"
    sleep 3
else
    lynx "$HOME_PAGE"
fi

clear
echo "--DISCONNECTED--"
sleep 2
