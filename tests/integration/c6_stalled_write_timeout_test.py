#!/usr/bin/env python3
"""C6: A client that stops reading a large GET response is disconnected.

Usage: python3 tests/integration/c6_stalled_write_timeout_test.py ./webserv
Expected runtime: roughly 36-48 seconds.
"""

import sys
import time

from stalled_write_common import (
    BIG_FILE_SIZE,
    SERVER_TIMEOUT,
    check_stalled_connection_closed,
    request_without_reading,
    running_server,
)


def main(server_path):
    with running_server(server_path) as port:
        print("[INFO] Webserv started", flush=True)
        with request_without_reading(port) as client:
            print(f"[INFO] Requested {BIG_FILE_SIZE // (1024 * 1024)} MiB; not reading response", flush=True)
            # No reads or writes during this interval: this is an outbound stall.
            time.sleep(SERVER_TIMEOUT + 6)
            received, reason = check_stalled_connection_closed(client)
            print(f"[PASS] Timed-out connection closed ({reason}, {received} buffered bytes)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} ./webserv", file=sys.stderr)
        sys.exit(2)
    try:
        main(sys.argv[1])
    except (AssertionError, OSError, RuntimeError) as exc:
        print(f"[FAIL] C6: {exc}", file=sys.stderr)
        sys.exit(1)
