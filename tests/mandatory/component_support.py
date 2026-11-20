"""Build the suite's CGI harness without dependencies on other test folders."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


def compile_component(root, work):
    here = Path(__file__).resolve().parent
    build = work / "cgi-component-build"
    build.mkdir()
    headers = build / "include"
    shutil.copytree(root / "include", headers)
    cgi_header = headers / "CGI.hpp"
    source = cgi_header.read_text()
    source, count = re.subn(r"\bclass\s+CGI\s*\{", "class CGI {\n    friend class CGITestPeer;", source, count=1)
    if count != 1:
        raise AssertionError("Cannot grant CGI test peer access")
    cgi_header.write_text(source)
    wrapper = '''#include "CGI.hpp"
#include <fcntl.h>
#include <iostream>
#include <ctime>
#include <poll.h>
int cgiTestPipe(int*);
pid_t cgiTestFork();
int cgiTestFcntl(int, int, int);
ssize_t cgiTestWrite(int, const void*, size_t);
#define pipe cgiTestPipe
#define fork cgiTestFork
#define fcntl cgiTestFcntl
#define write cgiTestWrite
'''
    # Keep every included header in one shadow include tree. This avoids
    # duplicate classes when ClientConnection includes CGI.hpp transitively.
    import json
    (build / "cgi_wrapped.cpp").write_text(wrapper + '#include ' + json.dumps(str(root / "source/CGI.cpp")) + '\n')
    command = shlex.split(os.environ.get("CXX", "c++"))
    command += ["-std=c++98", "-Wall", "-Wextra", "-Werror", "-I" + str(headers)]
    if re.search(r"ClientConnection\s*&\s*client", source):
        command += ["-DMANDATORY_CLIENT_CONTEXT"]
    command += [str(here / "cgi_component.cpp"), str(build / "cgi_wrapped.cpp")]
    command += [str(root / "source" / (name + ".cpp")) for name in (
        "Router", "ClientConnection", "HttpParser", "HttpRequest", "HttpResponse", "ServerConfig", "LocationConfig", "utils")]
    executable = build / "cgi_component"
    command += ["-o", str(executable)]
    print("Building mandatory CGI component harness (C++98)...", flush=True)
    result = subprocess.run(command, capture_output=True, timeout=60)
    (build / "build.log").write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise AssertionError("CGI test harness did not compile:\n" + result.stderr.decode("utf8", "replace")[-3000:])
    return executable
