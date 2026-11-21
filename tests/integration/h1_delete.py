#!/usr/bin/env python3

import os
import shutil
import socket
import sys
import tempfile
from common import start_server, stop_server, wait_for_server, read_response, assert_response

HOST = "127.0.0.1"
PORT = 8080


def send_delete(path):
    client = socket.create_connection((HOST, PORT), timeout=3)
    request = (
        "DELETE " + path + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    )
    client.sendall(request.encode())
    response = read_response(client)
    client.close()
    return response

def test_delete_file(files_dir):
    # Create a temporary file inside the DELETE-enabled directory.
    fd, target = tempfile.mkstemp(
        prefix="delete_test_",
        suffix=".txt",
        dir=files_dir
    )
    os.close(fd)

    filename = os.path.basename(target)

    try:
        response = send_delete("/delete/files/" + filename)
        assert_response(response, 204)

        assert not os.path.exists(target), \
            "File still exists after DELETE"

        print("[PASS] DELETE existing file returns 204 and removes the file")

        # Deleting the same file again should return 404.
        response = send_delete("/delete/files/" + filename)
        assert_response(response, 404)

        print("[PASS] DELETE already deleted file returns 404")

    finally:
        # Cleanup if the server failed to delete the file.
        if os.path.exists(target):
            os.remove(target)


def test_delete_nonexistent():
    response = send_delete("/delete/files/no_such_file.txt")
    assert_response(response, 404)
    print("[PASS] DELETE non-existent file returns 404")

def test_delete_directory(files_dir):
    target = tempfile.mkdtemp(
        prefix="delete_test_dir_",
        dir=files_dir
    )

    try:
        dirname = os.path.basename(target)

        response = send_delete("/delete/files/" + dirname)
        assert_response(response, 403)

        assert os.path.isdir(target), \
            "Directory was unexpectedly deleted"

        print("[PASS] DELETE directory returns 403")

    finally:
        shutil.rmtree(target)

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 h1_delete.py <server_executable> <config-file>")
        sys.exit(1)

    files_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "www", "delete", "files"
    )
    files_dir = os.path.normpath(files_dir)

    server = None
    try:
        server = start_server(sys.argv[1:], "h1_delete")
        wait_for_server(server)

        test_delete_file(files_dir)
        test_delete_nonexistent()
        test_delete_directory(files_dir)

        print("\n[PASS] H1 - DELETE method tests passed")

    except Exception as error:
        print("\n[FAIL] H1 -", error)
        sys.exit(1)

    finally:
        stop_server(server)
        print("[INFO] Test finished")


if __name__ == "__main__":
    main()
