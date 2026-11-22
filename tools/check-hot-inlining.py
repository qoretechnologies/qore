#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check that the calls on libqore's hot paths listed in tools/hot-inlining.txt are inlined (qore#6494).

libqore is built as a single compilation unit, which is at GCC's unit-wide inline growth limit: which of the inlines
that the limit allows GCC keeps depends on the size of the whole unit, so a change anywhere can make GCC stop
inlining a call on a hot path that the change does not touch.  The calls that the measurements of qore#6494 found to
depend on it are inlined explicitly (QORE_HOT_INLINE, always_inline); this script checks the result in the built
library: it disassembles each caller of the list (also its .constprop and .isra clones, not its .cold fragments,
which hold the code that GCC put out of the hot path), counts its out-of-line calls (and tail jumps) to the callee,
and fails when there are more than the list allows.

A line of the list is "<caller> <callee> [<max>]": caller and callee are qualified names of functions without
template arguments, parameters or return type, which match all of their overloads, template instances and clones;
max is the number of out-of-line calls allowed (default 0: every call is inlined), for a callee that is inlined at
the hot call sites of the caller and called out of line at its others.  "#" starts a comment.

The check needs an unstripped release build of libqore made by GCC (the symbols of its local functions); a library
made by another compiler is reported and not checked, unless --force is given (the output of GNU binutils is read:
--force does not make the tools of another toolchain readable).  The calls are recognized in the code of x86_64
(call, jmp) and aarch64 (bl, b; with their PLT stubs; a call through a linker veneer, which a library larger than
the 128MB range of bl would need, names the veneer and is not recognized); the check fails for a library of another
architecture, as it could not see any call there (qore#7165).  CI runs it in the build-perf job.

Usage: check-hot-inlining.py <build-dir> [--list <file>] [--force]
"""

import argparse
import os
import re
import subprocess
import sys


def base_name(sig):
    """Returns the qualified name of a demangled signature, without template arguments, parameters, return type or
    clone suffix"""
    depth = 0
    out = []
    for ch in sig:
        if ch == "<":
            depth += 1
            continue
        if ch == ">":
            depth -= 1
            continue
        if depth == 0:
            if ch == "(":
                break
            out.append(ch)
    name = "".join(out).strip()
    # "operator()" and the like keep their name
    name = re.sub(r"\boperator ", "operator", name)
    name = name.split(" ")[-1].lstrip("*&")
    return name


def read_list(path):
    pairs = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) not in (2, 3) or (len(parts) == 3 and not parts[2].isdigit()):
                sys.exit("%s:%d: expected \"<caller> <callee> [<max>]\": %r" % (path, n, line))
            pairs.append((parts[0], parts[1], int(parts[2]) if len(parts) == 3 else 0, n))
    return pairs


def find_library(build_dir):
    for name in sorted(os.listdir(build_dir)):
        if re.match(r"libqore\.so(\.\d+)*$", name) and not os.path.islink(os.path.join(build_dir, name)):
            return os.path.join(build_dir, name)
    for name in ("libqore.dylib",):
        if os.path.exists(os.path.join(build_dir, name)):
            return os.path.join(build_dir, name)
    sys.exit("no libqore found in %s" % build_dir)


#! the out-of-line calls and tail jumps of each architecture in the output of objdump -d -C --no-show-raw-insn: the
#! direct ones, which name their target (an indirect call through a register names none)
CALL_PATTERNS = {
    # x86_64: call, jmp (and their suffixed forms)
    "x86-64": re.compile(r"\s(?:call|jmp)[a-z]*\s+[0-9a-f]+ <(.*?)(?:@plt)?(?:\+0x[0-9a-f]+)?>"),
    # aarch64: bl (a call) and b (a tail call); not b.<cond>, cbz and the like, which branch within the function, nor
    # blr and br, which name no target
    "aarch64": re.compile(r"\s(?:bl|b)\s+[0-9a-f]+ <(.*?)(?:@plt)?(?:\+0x[0-9a-f]+)?>"),
}


def library_arch(lib):
    """Returns the architecture of the library: a key of CALL_PATTERNS, or what objdump reports for another one"""
    out = subprocess.run(["objdump", "-f", lib], capture_output=True, text=True, check=True).stdout
    m = re.search(r"architecture:\s*([^,\s]+)", out)
    arch = m.group(1) if m else ""
    if arch in ("i386:x86-64", "x86-64"):
        return "x86-64"
    if arch.startswith("aarch64"):
        return "aarch64"
    return arch


def count_calls(dis, pattern):
    """Returns the number of out-of-line calls to each callee (its base name) in the disassembly; calls of .cold
    fragments are not counted"""
    found = {}
    for line in dis.splitlines():
        m = pattern.search(line)
        if m:
            target = m.group(1)
            if "[clone .cold" in target:
                continue
            t = base_name(target)
            found[t] = found.get(t, 0) + 1
    return found


def compiler_of(lib):
    try:
        out = subprocess.run(["readelf", "-p", ".comment", lib], capture_output=True, text=True).stdout
    except OSError:
        return ""
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("build_dir")
    ap.add_argument("--list", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "hot-inlining.txt"))
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    lib = find_library(args.build_dir)
    comment = compiler_of(lib)
    if "GCC:" not in comment and not args.force:
        print("%s was not made by GCC; the hot inlines are not checked" % lib)
        return 0

    # a library of an architecture whose calls are not recognized cannot be checked: every call would look inlined
    arch = library_arch(lib)
    call = CALL_PATTERNS.get(arch)
    if not call:
        print("ERROR: %s is a library of the architecture %r, whose calls this script does not recognize; the hot "
              "inlines cannot be checked (see CALL_PATTERNS in %s)" % (lib, arch, os.path.basename(__file__)))
        return 2

    pairs = read_list(args.list)
    callers = {c for c, _, _, _ in pairs}

    # the code symbols of the callers: address, size and whether it is a .cold fragment
    nm = subprocess.run(["nm", "-C", "-S", "--defined-only", lib], capture_output=True, text=True, check=True).stdout
    ranges = {}
    for line in nm.splitlines():
        m = re.match(r"([0-9a-f]+) ([0-9a-f]+) [tTwW] (.*)$", line)
        if not m:
            continue
        sig = m.group(3)
        if "[clone .cold" in sig:
            continue
        name = base_name(sig)
        if name in callers:
            ranges.setdefault(name, []).append((int(m.group(1), 16), int(m.group(2), 16), sig))

    failures = 0
    calls_of = {}
    for name, rs in ranges.items():
        found = {}
        for addr, size, sig in rs:
            dis = subprocess.run(["objdump", "-d", "-C", "--no-show-raw-insn", "--start-address=%#x" % addr,
                                  "--stop-address=%#x" % (addr + size), lib], capture_output=True, text=True,
                                 check=True).stdout
            for t, n in count_calls(dis, call).items():
                found[t] = found.get(t, 0) + n
        calls_of[name] = found

    for caller, callee, allowed, n in pairs:
        if caller not in ranges:
            print("MISSING %s (line %d): no such function in %s (renamed, or inlined into its callers?)"
                  % (caller, n, lib))
            failures += 1
            continue
        count = calls_of[caller].get(callee, 0)
        if count > allowed:
            print("NOT-INLINED %s calls %s out of line %d time(s), %d allowed (line %d)"
                  % (caller, callee, count, allowed, n))
            failures += 1

    if failures:
        print("%d of %d hot inlines failed; see tools/hot-inlining.txt and qore#6494" % (failures, len(pairs)))
        return 1
    print("all %d hot inlines of %s (%s) are inlined" % (len(pairs), lib, arch))
    return 0


if __name__ == "__main__":
    sys.exit(main())
