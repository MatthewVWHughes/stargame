#!/usr/bin/env python3
import json
import sys

from ws_bridge_call import call


def main():
    if len(sys.argv) < 4:
        print("usage: ws_bridge_call_long.py TIMEOUT_SECONDS ACTION JSON_PAYLOAD", file=sys.stderr)
        return 2
    timeout = float(sys.argv[1])
    action = sys.argv[2]
    payload = json.loads(sys.argv[3])
    result = call(action, payload, timeout=timeout)
    print(json.dumps(result, indent=2))
    return 0 if result["response"].get("success", False) else 1


if __name__ == "__main__":
    raise SystemExit(main())
