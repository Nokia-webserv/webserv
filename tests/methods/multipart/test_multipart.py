#!/usr/bin/env python3
"""Real Router -> POST -> multipart -> part -> saveFile, without sockets."""

from collections import Counter
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def field(value=b"My cat"):
    return b'Content-Disposition: form-data; name="title"\r\n\r\n' + value


def file_part(value, mime="text/plain", filename="cat.txt"):
    headers = 'Content-Disposition: form-data; name="file"; filename="' + filename + '"'
    if mime is not None:
        headers += "\r\nContent-Type: " + mime
    return headers.encode("ascii") + b"\r\n\r\n" + value


def multipart(parts, boundary="abc123", ending=b"\r\n"):
    marker = boundary.encode("ascii")
    return b"--" + marker + b"\r\n" + (b"\r\n--" + marker + b"\r\n").join(parts) + b"\r\n--" + marker + b"--" + ending


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def read_response(path, expected):
    raw = path.read_bytes()
    require(b"\r\n\r\n" in raw, "Missing HTTP header/body separator")
    head, body = raw.split(b"\r\n\r\n", 1)
    lines = head.split(b"\r\n")
    version, status, reason = lines[0].decode("ascii").split(" ", 2)
    print("    After: " + lines[0].decode("ascii"), flush=True)
    require(version == "HTTP/1.1" and int(status) == expected,
            "Expected HTTP " + str(expected) + ", received " + str(status))
    reasons = {201: "Created", 400: "Bad Request", 403: "Forbidden", 404: "Not Found", 500: "Internal Server Error"}
    require(reason == reasons[expected], "Wrong reason phrase")
    headers = {}
    for line in lines[1:]:
        name, value = line.split(b":", 1)
        key = name.decode("ascii").lower()
        require(key not in headers, "Duplicate response header")
        headers[key] = value.strip()
    require(headers.get("content-length") == str(len(body)).encode("ascii"), "Incorrect Content-Length")
    require(not body if expected == 201 else bool(body), "Unexpected response body")


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: test_multipart.py COMPONENT_EXECUTABLE")
    executable = str(Path(sys.argv[1]).resolve())
    cases = []

    def add(name, parts, saved=(), status=201, **options):
        boundary = options.pop("boundary", "abc123")
        cases.append(dict(name=name, body=multipart(parts, boundary), saved=list(saved),
                          status=status, content_type="multipart/form-data; boundary=" + boundary, **options))

    text, png = b"hello\n", b"\x89PNG\x00\xff\r\nbytes"
    add("One text file", [file_part(text)], [(".txt", text)])
    add("Field followed by a file", [field(), file_part(text)], [(".txt", text)])
    add("File followed by a field", [file_part(text), field()], [(".txt", text)])
    add("Three files preserve their individual bytes", [file_part(text), file_part(png, "image/png"), file_part(b"{}", "application/json")],
        [(".txt", text), (".png", png), (".json", b"{}")])
    add("Two files with the same extension", [file_part(b"first"), file_part(b"second")], [(".txt", b"first"), (".txt", b"second")])
    add("Only form fields create no files", [field(), field(b"filename=not-a-file.png")])
    add("An empty file is created", [file_part(b"")], [(".txt", b"")])
    binary = bytes(range(256)) * 9
    add("Binary bytes including NUL", [file_part(binary, "application/octet-stream")], [("", binary)])
    payload = b"one\r\n\r\nContent-Type: image/png\r\ntwo\r\n"
    add("Header-like text and trailing CRLF remain in file", [file_part(payload)], [(".txt", payload)])
    large = bytes(range(256)) * 4096
    add("One MiB file", [file_part(large, None)], [("", large)])
    add("Missing part Content-Type", [file_part(text, None)], [("", text)])
    add("Unknown part Content-Type", [file_part(text, "application/x-custom")], [("", text)])
    add("MIME case and charset parameter", [file_part(text, "Text/Plain; charset=utf-8")], [(".txt", text)])
    add("Quoted boundary", [file_part(text)], [(".txt", text)])
    cases[-1]["content_type"] = 'multipart/form-data; boundary="abc123"'
    add("Mixed-case boundary value", [file_part(text)], [(".txt", text)], boundary="AbC123")
    add("Mixed-case multipart type preserves boundary case", [file_part(text)], [(".txt", text)], boundary="AbC123")
    cases[-1]["content_type"] = "Multipart/Form-Data; Boundary=AbC123"
    add("No CRLF after closing delimiter", [file_part(text)], [(".txt", text)])
    cases[-1]["body"] = multipart([file_part(text)], ending=b"")
    add("Epilogue is not uploaded", [file_part(text)], [(".txt", text)])
    cases[-1]["body"] += b"ignored\r\n--abc123\r\n" + file_part(b"must not be saved")
    add("Client filename cannot choose disk path", [file_part(png, "image/png", "../../escape.php")], [(".png", png)])
    add("Existing generated filename is not overwritten", [file_part(text)], [(".txt", text)], existing=True)
    add("Empty upload_store", [file_part(text)], status=403, store_mode="empty")
    add("Missing upload directory", [file_part(text)], status=404, store_mode="missing")
    add("upload_store is a regular file", [file_part(text)], status=404, store_mode="file")
    add("Unwritable upload directory", [file_part(text)], status=403, store_mode="denied")
    add("Missing boundary parameter", [file_part(text)], status=400)
    cases[-1]["content_type"] = "multipart/form-data"
    add("Wrong first boundary", [file_part(text)], status=400)
    cases[-1]["body"] = multipart([file_part(text)], "wrong")
    add("Missing closing delimiter", [file_part(text)], status=400)
    cases[-1]["body"] = b"--abc123\r\n" + file_part(text)
    add("Malformed part stops processing later files", [file_part(text), b"invalid part", file_part(b"later")],
        [(".txt", text)], status=400, partial=True)
    add("Junk attached to closing line returns 400", [file_part(text)], [(".txt", text)], status=400, partial=True)
    cases[-1]["body"] = multipart([file_part(text)], ending=b"junk")
    add("Short writes still save every byte", [file_part(binary)], [(".txt", binary)], chunk=7)
    for name, failure, chunk in (
        ("Unexpected open failure", 1, 0), ("Exhausted filename retries", 2, 0),
        ("Write failure stays 500", 3, 0), ("Zero-byte write stays 500", 4, 0),
        ("Failure after partial write stays 500", 5, 7), ("Close failure stays 500", 6, 0),
    ):
        fault_payload = b"first file must need several writes"
        add(name, [file_part(fault_payload), file_part(b"later")], [(".txt", fault_payload)],
            status=500, failure=failure, chunk=chunk, partial=True)

    passed = failed = skipped = 0
    with tempfile.TemporaryDirectory(prefix="webserv-multipart-fixtures-") as temporary:
        root = Path(temporary)
        for number, case in enumerate(cases, 1):
            print("\n[" + str(number) + "] " + case["name"], flush=True)
            print("    Before: POST /upload; body bytes=" + str(len(case["body"])), flush=True)
            print("    Expected: HTTP " + str(case["status"]) + "; saved file(s)="
                  + str(len(case["saved"])) + (" at most (partial cleanup policy unspecified)" if case.get("partial") else ""), flush=True)
            if case.get("store_mode") == "denied" and os.geteuid() == 0:
                skipped += 1
                print("\033[33m[SKIP]\033[0m root bypasses directory permissions", flush=True)
                continue
            directory = root / str(number)
            directory.mkdir()
            store = directory / "uploads"
            mode = case.get("store_mode", "normal")
            if mode == "file":
                store.write_bytes(b"original")
            elif mode != "missing":
                store.mkdir()
            if case.get("existing"):
                (store / "upload_0.txt").write_bytes(b"existing bytes")
            before = set(store.iterdir()) if store.is_dir() else set()
            if mode == "denied":
                store.chmod(0o500)
            body_file, response_file, metadata_file = [directory / name for name in ("request.bin", "response.bin", "metadata.txt")]
            body_file.write_bytes(case["body"])
            try:
                run = subprocess.run([executable, str(body_file), str(response_file), str(metadata_file),
                                      "" if mode == "empty" else str(store), case["content_type"],
                                      str(case.get("failure", 0)), str(case.get("chunk", 0))],
                                     cwd=directory, capture_output=True, timeout=10)
                require(run.returncode == 0, "Component failed: " + run.stderr.decode("utf-8", "replace"))
                read_response(response_file, case["status"])
                error, code, descriptors, opens, writes, closes = map(int, metadata_file.read_text().split())
                require(error == int(case["status"] >= 400), "Wrong RouteDecision type")
                require(code == (case["status"] if error else 0), "Wrong RouteDecision errorCode")
                require(descriptors == 0, "Upload descriptor leaked")
                entries = set(store.iterdir()) if store.is_dir() else set()
                require(before <= entries, "Existing directory entry disappeared")
                actual = []
                for path in sorted(entries - before):
                    require(path.is_file() and not path.is_symlink(), "Upload is not a regular file")
                    require(re.fullmatch(r"upload_[0-9]+(?:\.[A-Za-z0-9]+)?", path.name), "Unsafe/generated filename mismatch")
                    data = path.read_bytes()
                    actual.append((path.suffix, data))
                    print("    Saved: " + path.name + "; bytes=" + str(len(data))
                          + "; SHA256=" + hashlib.sha256(data).hexdigest(), flush=True)
                if case.get("partial"):
                    require(len(actual) <= len(case["saved"]), "Later file was processed after an error")
                    for suffix, data in actual:
                        incomplete_write = case.get("failure") in (3, 4, 5)
                        require(any(suffix == ext and (full.startswith(data) if incomplete_write else data == full)
                                    for ext, full in case["saved"]),
                                "Unexpected saved bytes after the failed request")
                    if case.get("failure"):
                        require(opens == 1 if case["failure"] != 2 else opens > 1,
                                "Handler continued to another file after an I/O error")
                        if case["failure"] in (1, 2):
                            require(not actual, "File appeared despite open failing")
                else:
                    require(Counter(actual) == Counter(case["saved"]), "Saved file count/extensions/bytes differ")
                if case.get("existing"):
                    require((store / "upload_0.txt").read_bytes() == b"existing bytes", "Existing file overwritten")
                if mode == "file":
                    require(store.read_bytes() == b"original", "upload_store file overwritten")
                require(not (root / "escape.php").exists() and not (directory / "escape.php").exists(), "Client filename escaped upload_store")
                print("    I/O: open=" + str(opens) + ", write=" + str(writes) + ", close=" + str(closes) + "; no descriptor leaks", flush=True)
                passed += 1
                print("\033[32m[PASS]\033[0m " + case["name"], flush=True)
            except Exception as error:
                failed += 1
                print("\033[31m[FAIL]\033[0m " + case["name"] + ": " + str(error), flush=True)
            finally:
                if mode == "denied":
                    store.chmod(0o700)
    print("\nPassed: " + str(passed) + ", failed: " + str(failed) + ", skipped: " + str(skipped), flush=True)
    return int(bool(failed))


if __name__ == "__main__":
    sys.exit(main())
