#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""RPM dependency generator for native and AOT Qore modules.

Native modules leave Qore symbols unresolved intentionally. elfdeps therefore
cannot infer their libqore dependency. Require the runtime from the SDK used
for compilation, including its SONAME package and epoch/version/release floor.
This runs during binary packaging, never while preparing an SRPM.
"""
import re
import subprocess
import sys


def requirements(sdk_requires, sdk_evr, isa, api):
    if not re.fullmatch(r"[0-9]+:[A-Za-z0-9._+~^]+-[A-Za-z0-9._+~^]+", sdk_evr):
        raise ValueError("Invalid SDK epoch/version/release")
    if not re.fullmatch(r"\([A-Za-z0-9_-]+\)", isa):
        raise ValueError("Missing native RPM ISA")
    if not re.fullmatch(r"[0-9]+\.[0-9]+", api):
        raise ValueError("Invalid Qore module API")
    libraries = []
    for line in sdk_requires.splitlines():
        match = re.fullmatch(r"(libqore[0-9]*" + re.escape(isa) + r") = (\S+)", line)
        if match:
            libraries.append(match[1])
    if len(libraries) != 1:
        raise ValueError("SDK must have exactly one native libqore SONAME package dependency")
    return [f"qore-module(abi){isa} = {api}", f"qore{isa} >= {sdk_evr}",
            f"{libraries[0]} >= {sdk_evr}"]


def query(*args):
    return subprocess.check_output(["rpm", *args], text=True).strip()


def main():
    files = [line.strip() for line in sys.stdin if line.strip()]
    if not files:
        return
    if any(not path.endswith(".qmod") for path in files):
        raise ValueError("Unexpected file passed to Qore dependency generator")
    isa = query("--eval", "%{?_isa}")
    sdk = "qore-devel" + isa
    sdk_evr = query("-q", "--whatprovides", "--qf", "%{EPOCHNUM}:%{VERSION}-%{RELEASE}", sdk)
    api = subprocess.check_output(["/usr/bin/qore", "--latest-module-api"], text=True).strip()
    print("\n".join(requirements(query("-q", "--whatprovides", "--requires", sdk), sdk_evr, isa, api)))


if __name__ == "__main__":
    main()
