#!/bin/bash

echo "NOTE: CTRL+C WILL EXIT SESSION"
sleep 2
clear

if command -v vidtex >/dev/null 2>&1; then
    vidtex --host glasstty.com --port 6502
elif [ -x "/usr/local/bin/vidtex" ]; then
    /usr/local/bin/vidtex --host glasstty.com --port 6502
elif [ -x "/opt/homebrew/bin/vidtex" ]; then
    /opt/homebrew/bin/vidtex --host glasstty.com --port 6502
else
    echo "Vidtex is not installed."
    sleep 3
fi

clear
echo "--DISCONNECTED--"
sleep 2
