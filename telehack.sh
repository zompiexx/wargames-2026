#!/bin/bash

clear

if command -v telnet >/dev/null 2>&1; then
    telnet telehack.com 1337
elif command -v nc >/dev/null 2>&1; then
    nc telehack.com 1337
else
    echo "No telnet or netcat client available."
    sleep 2
fi

clear
echo "--DISCONNECTED--"
sleep 2
