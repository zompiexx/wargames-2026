#!/usr/bin/env bash
set -e

python3 ./webllm/wopr_chat.py --ensure

echo
echo "Waiting for browser-side WebLLM client..."
echo "When the browser reports SYSTEM READY, press ENTER here."
read -r

echo
echo "Bridge status:"
curl -fsS http://127.0.0.1:8765/api/status
echo
echo

echo "Test query:"
echo "Hello Joshua. Do you remember me?" | python3 ./webllm/webllm_query.py
