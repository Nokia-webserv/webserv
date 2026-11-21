"""CGI components: real pipes/processes and deterministic syscall failures."""
import os
import re
import sys


def extended_components(report, invoke, parse_response, require, root, build):
    def state(mode, scenario, data=b"", query="mode=echo"):
        output = invoke(mode, data, scenario, query, query)
        line, _, wire = output.partition(b"\n")
        print("After:    " + line.decode("utf8", "replace"), flush=True)
        values = dict(item.split("=", 1) for item in line.decode().split())
        return {key: int(value) for key, value in values.items()}, wire

    def safe_output(data, body):
        meta, _, wire = invoke("output", data).partition(b"\n")
        if meta == b"0 0":
            print("After:    unusable CGI response rejected", flush=True)
            return
        require(meta == b"1 1", "Inconsistent output readiness: " + repr(meta))
        parse_response(wire, exact=body)

    for name, data, body in [
        ("Wrong Content-Length, shorter than body", b"Content-Type: text/plain\nContent-Length: 1\n\nhello", b"hello"),
        ("Wrong Content-Length, longer than body", b"Content-Type: text/plain\nContent-Length: 99\n\nhello", b"hello"),
        ("Negative Content-Length", b"Content-Type: text/plain\nContent-Length: -1\n\nhello", b"hello"),
        ("Non-numeric Content-Length", b"Content-Type: text/plain\nContent-Length: nope\n\nhello", b"hello"),
        ("Invalid HTTP status, below range", b"Status: 99 Invalid\nContent-Type: text/plain\n\nhello", b"hello"),
        ("Invalid HTTP status, above range", b"Status: 600 Invalid\nContent-Type: text/plain\n\nhello", b"hello"),
        ("CRLF bytes in binary response body", b"Content-Type: application/octet-stream\r\n\r\n\x00\r\n\xff", b"\x00\r\n\xff"),
    ]:
        report.case("C1 CGI output", name, "Reject unusable output or produce a correctly framed HTTP response",
                    lambda d=data, b=body: safe_output(d, b))

    for method, body, raw in [("GET", b"", ""), ("POST", b"a\x00b\xff", "a=%2F&b=x+y&c=%2520"),
                              ("DELETE", b"", "id=17")]:
        def environment(m=method, b=body, q=raw):
            entries = invoke("env-no-length", b, m, "decoded value", q).split(b"\0")
            env = dict(entry.decode().split("=", 1) for entry in entries if entry)
            require(env["REQUEST_METHOD"] == m and env["QUERY_STRING"] == q, "Method or raw query changed")
            require(env["CONTENT_LENGTH"] == str(len(b)), "Fallback length does not describe body bytes")
            require("HTTP_CONTENT_TYPE" not in env and "HTTP_CONTENT_LENGTH" not in env,
                    "Content metadata was incorrectly duplicated as HTTP_* headers")
            print("After:    method={}, raw query={!r}, body length={}".format(m, env["QUERY_STRING"], env["CONTENT_LENGTH"]), flush=True)
        report.case("C2 Request environment", method + " metadata without incoming Content-Length",
                    "Exact method/query; length derived from body; no duplicate content meta-variables", environment)

    def argv_check():
        args = [value.decode() for value in invoke("argv").split(b"\0") if value]
        require(args == [sys.executable, "probe.py"], "Wrong interpreter/script argv: " + repr(args))
        print("After:    argv=" + repr(args), flush=True)
    report.case("C2 Request environment", "Interpreter and script arguments", "Interpreter argv[0], relative script name argv[1]", argv_check)
    report.case("C2 Request environment", "Environment pointer lifetime and terminator",
                "Every envp entry points to live string storage; final entry is NULL",
                lambda: require(invoke("env-pointers").strip() == b"pointers=1", "Broken envp storage or terminator"))

    def lifecycle(scenario):
        values, _ = state("lifecycle", scenario)
        require(all(values[key] == 1 for key in ("closed", "cleared", "tracked", "reaped")),
                "Leaked descriptor, stale client pointer or untracked/unreaped child")
        if scenario not in ("0", "completed", "cleanup"):
            require(values["error"] == 500, "Setup failure did not expose HTTP 500")
        if scenario == "completed":
            require(values["ready"] == 1, "Normal CGI did not finish before cleanup")
        if scenario == "cleanup":
            require(values["idempotent"] == 1, "Repeated cleanup closed a reused unrelated descriptor")
    for scenario, label in [("1", "First pipe failure"), ("2", "Second pipe failure"),
                            ("3", "Fork failure"), ("4", "First nonblocking fcntl failure"),
                            ("7", "Second nonblocking fcntl failure"), ("0", "Delete running CGI"),
                            ("completed", "Delete completed CGI"), ("cleanup", "Repeated cleanup")]:
        report.case("C3 Resource lifecycle", label, "All pipe ends closed, client pointer cleared, child reaped or tracked for reaping",
                    lambda s=scenario: lifecycle(s))

    for scenario, payload in [("full", b"a\x00b\xffc"), ("partial", b"abcdefghij\x00\xff")]:
        def input_success(s=scenario, data=payload):
            values, received = state("input", s, data)
            require(received == data, "Partial writes duplicated or lost input bytes")
            require(values["sent"] == len(data) and values["offsets"] == 1, "Incorrect byte offset")
            require(values["open"] == 0 and values["eof"] == 1, "Final input write did not deliver EOF")
            require(values["ungated"] == 0 and values["errno"] == 0, "I/O without readiness or errno inspection")
            if s == "partial":
                require(values["writes"] == (len(data) + 2) // 3, "Partial-write scenario was not exercised")
        report.case("C4 CGI stdin", scenario + " input write", "Exact bytes and offset; pipe closes only after the last byte", input_success)

    for scenario in ("negative", "zero"):
        def input_failure(s=scenario):
            values, _ = state("input", s, b"nonempty body")
            require(values["writes"] == 1, "Injected write did not execute exactly once")
            require(values["sent"] == 0 and values["offsets"] == 1, "Failed write advanced input cursor")
            require(values["errno"] == 0, "CGI consulted errno after write")
            require(500 <= values["error"] <= 599,
                    "write returned " + ("-1" if s == "negative" else "0") + " for nonempty input, but CGI exposed no error")
        report.case("C4 CGI stdin", "write returns " + ("-1" if scenario == "negative" else "0"),
                    "Expose server error without advancing cursor or consulting errno", input_failure)

    response = b"Content-Type: application/octet-stream\r\n\r\na\x00b\xffc"
    for scenario, payload, valid in [("full", response, True), ("partial", response, True),
                                     ("negative", b"unread data", False), ("empty", b"", False),
                                     ("malformed", b"not a header\n\nbody", False)]:
        def read_case(s=scenario, data=payload, ok=valid):
            values, wire = state("read", s, data)
            require(values["premature"] == 0, "Response completed before stdout EOF")
            require(values["one_read"] == 1, "Multiple reads within one handler call")
            require(values["ungated"] == 0 and values["errno"] == 0, "I/O without readiness or errno inspection")
            if ok:
                require(values["error"] == 0 and values["ready"] == 1, "Valid pipe output did not complete")
                parse_response(wire, 200, b"a\x00b\xffc")
            else:
                require(values["error"] == 500 and values["ready"] == 0, "Unusable output did not expose 500")
        report.case("C5 CGI stdout", scenario + " stdout read", "Data preserved until EOF; failures/unusable output expose 500", read_case)

    process_cases = []
    for size in (0, 1, 4095, 4096, 4097, 65536, 1048576):
        data = (bytes(range(256)) * ((size + 255) // 256))[:size]
        process_cases.append(("Binary stdin and EOF, bytes=" + str(size), "echo", "mode=echo", data, data, False))
    process_cases += [
        ("Correct working directory", "cwd", "mode=cwd", b"", b"relative path works\n", False),
        ("Split CGI headers/body", "split", "mode=partial", b"", b"firstsecond\x00third", False),
        ("Large stdout without Content-Length", "large", "mode=large", b"", bytes(range(256)) * 4096, False),
        ("Full stdout pipe before stdin read", "duplex", "mode=duplex", b"I" * 262144, b"O" * 131072 + b"I" * 262144, False),
        ("No CGI output", "empty", "mode=empty", b"", None, True),
        ("Malformed CGI output", "malformed", "mode=malformed", b"", None, True),
        ("Child killed by signal", "crash", "mode=crash", b"", None, True),
        ("chdir failure", "chdir", "mode=cwd", b"", None, True),
        ("execve failure", "exec", "mode=echo", b"", None, True),
        ("stdin dup2 failure", "dup-stdin", "mode=echo", b"", None, True),
        ("stdout dup2 failure", "dup-stdout", "mode=echo", b"", None, True),
        ("Child error-message write fails", "child-write", "mode=echo", b"", None, True),
        ("Child error-message write returns zero", "child-write-zero", "mode=echo", b"", None, True),
        ("Child error-message write is partial", "child-write-short", "mode=echo", b"", None, True),
    ]
    for name, scenario, query, payload, body, failure in process_cases:
        def process_case(s=scenario, q=query, data=payload, expected=body, bad=failure):
            values, wire = state("execute", s, data, q)
            require(values["published"] == 1, "Running child not published in client")
            require(values["nonblocking"] == 1, "Parent pipe descriptors are blocking")
            require(values["ungated"] == 0 and values["errno"] == 0, "Parent pipe I/O without readiness or errno inspection")
            require(values["completed"] == 1, "CGI made no response/error progress before the 2.5s component deadline")
            if not data:
                require(values["empty_eof"] == 1, "Empty request did not immediately close CGI stdin")
            if bad:
                require(500 <= values["error"] <= 599, "Failed CGI did not expose a server error")
            else:
                require(values["ready"] == 1 and values["error"] == 0, "CGI did not produce a response")
                parse_response(wire, 200, expected)
        report.case("C6 Script execution", name, "Bounded server error" if failure else "Real CGI through polled nonblocking pipes; exact body", process_case)

    def child_readiness():
        path = build / "child-readiness.log"
        old = os.environ.get("CGI_TEST_CHILD_AUDIT")
        os.environ["CGI_TEST_CHILD_AUDIT"] = str(path)
        try:
            state("execute", "exec")
        finally:
            if old is None:
                os.environ.pop("CGI_TEST_CHILD_AUDIT", None)
            else:
                os.environ["CGI_TEST_CHILD_AUDIT"] = old
        events = path.read_text().splitlines()
        require(events, "No child error-report write was traced")
        print("After:    " + " | ".join(events), flush=True)
        require(all(line == "child-write ready=1" for line in events), "Child writes error message to pipe without prior POLLOUT")
    report.case("C7 Mandatory I/O rules", "Child error-report pipe readiness", "Pipe write follows poll readiness, including child setup failures", child_readiness)

    def discarded_calls(names):
        source = (root / "source/CGI.cpp").read_text()
        # Mask comments/quoted text while preserving line numbers. This check
        # detects directly discarded results; it is not a complete C++ analysis.
        masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                        lambda m: re.sub(r"[^\n]", " ", m.group()), source, flags=re.S)
        pattern = r"(?:^|[;{}])\s*(?:\(\s*void\s*\)\s*)?(?:::)?(" + "|".join(names) + r")\s*\("
        bad = ["{}:{} ({})".format("source/CGI.cpp", masked.count("\n", 0, match.start(1)) + 1, match.group(1))
               for match in re.finditer(pattern, masked)]
        require(not bad, "Return value discarded: " + ", ".join(bad))
        print("After:    No directly discarded " + "/".join(names) + " return values", flush=True)
    report.case("C7 Mandatory I/O rules", "I/O return values are not discarded", "No standalone read/write/recv/send call discards its result",
                lambda: discarded_calls(("read", "write", "recv", "send")))
    report.case("C7 Mandatory I/O rules", "Working-directory failure is checked", "chdir failure is handled before script execution",
                lambda: discarded_calls(("chdir",)))
