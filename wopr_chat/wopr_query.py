#!/usr/bin/env python3

import json
import sys
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

ENDPOINT = "http://127.0.0.1:8765/api/query"

def main():
    prompt = sys.stdin.read().strip()
    if not prompt:
        return 0

    payload = json.dumps({"prompt": prompt}).encode("utf-8")
    request = Request(
        ENDPOINT,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urlopen(request, timeout=620) as response:
            data = json.loads(response.read().decode("utf-8"))
    except HTTPError as exc:
        print(f"NEURAL RESPONSE MODULE ERROR: HTTP {exc.code}")
        return 1
    except URLError:
        print("NEURAL RESPONSE MODULE UNAVAILABLE.")
        return 1
    except Exception as exc:
        print(f"NEURAL RESPONSE MODULE ERROR: {exc}")
        return 1

    if not data.get("ok"):
        print(f"NEURAL RESPONSE MODULE ERROR: {data.get('error', 'UNKNOWN ERROR')}")
        return 1

    reply = str(data.get("response", "")).strip()
    print(reply if reply else "NO RESPONSE.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
