#!/bin/bash

make re || exit 1

# Start server in the background
./webserv test1.conf &
SERVER_PID=$!

# Make sure the server is stopped when the script exits
cleanup() {
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    make fclean
}

trap cleanup EXIT INT TERM

# Give server time to start
sleep 1

echo "===== CLIENT RESPONSE ====="
curl -v --http1.0 http://localhost:8080/cgi_bin/add.user.py
echo
echo "===== END RESPONSE ====="
