#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check the stack frames of libqore's hot call path against the budgets of tools/stack-usage-budgets.txt.

A build configured with -DQORE_STACK_USAGE_REPORT=ON compiles libqore with -fstack-usage: the compiler writes the
frame size of every function into a .su file next to each object, and CMake writes the compiler and build type into
stack-usage-config.txt in the build directory.  This script reads both and fails when a function in the budget file
has a larger frame than its budget, or is not found in any .su file (it was renamed, or inlined into its callers,
whose budgets then have to be reviewed).

Every call of interpreted code (and every level of a recursion of it) puts one frame of each of these functions on
the thread's stack, so each byte of growth costs as many bytes as the recursion is deep; deep recursion on Alpine
(musl, 8 MB thread stacks) fails first.  Frame sizes depend on the compiler, its version and the optimization
level, the other compiler flags, the hardening options that the compiler enables by default and the C library's
headers: the budgets are calibrated for the compiler and its major version, the build type and its flags, the other
compiler flags (CMAKE_CXX_FLAGS), the hardening macros that the compiler predefines with them, link-time optimization
and the target (the compiler's -dumpmachine triple, which names the C library) named in the budget file, and are only
checked for a build that matches them all (any other build, such as one made with a multi-config generator, is
reported, not checked, unless --force is given).

The frame of a function is the largest of its records (all overloads; GCC records its .constprop and .isra clones,
which are alternatives to it, under its own name), which is all of its frame as long as GCC did not split it: a
function that partial inlining split into a head and a ".part" clone, which the head calls, has both frames on the
stack, and GCC records the clone in ways that cannot be attributed reliably.  So a budgeted function with a .part
clone in the object file's symbol table (objdump -t; .cold fragments, which share their function's frame, do not
count) fails the check as SPLIT: an unsplit function's frame is fully in its own record, and a split one is never
passed.  A SPLIT function needs a review: a budget for its head and part, or code that GCC does not split.  A function
that is only a branch to another one (identical code folding merged it) fails as FOLDED, as it runs in the other
function's frame, and one with a "dynamic" record that is not "bounded" (alloca(), variable-length arrays) as DYNAMIC.
Overloads of a function are charged the largest of their frames, not the sum: this assumes that an overload does not
call another, which the check verifies for direct calls (OVERLOAD-CALL; a tail call reuses the frame and is allowed).
The check needs the object files (an error without them, unless --allow-no-symbols is given).

usage: check-stack-usage.py [--budgets FILE] [--force] [--verbose] BUILD_DIR
exit status: 0 when every budget is met or the build is not one the budgets are for, 1 when a budget is exceeded or
a function is missing, SPLIT, FOLDED, DYNAMIC or has an OVERLOAD-CALL, 2 for a usage or input error
"""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUDGETS = ROOT / "tools" / "stack-usage-budgets.txt"
CONFIG_NAME = "stack-usage-config.txt"


class InputError(Exception):
    pass


def parse_budgets(text, name="<budgets>"):
    """Parses a budget file.

    Returns (settings, budgets): settings are the "@key value" lines (compiler, build-type, target and the optional
    compiler-version, build-type-flags, cxx-flags, hardening and ipo), budgets a list of
    (function, bytes, line number) in file order.  Blank lines and lines starting with # are ignored.
    """
    settings = {}
    budgets = []
    seen = set()
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("@"):
            parts = line[1:].split(None, 1)
            if len(parts) != 2:
                raise InputError("%s:%d: a setting needs a value: %r" % (name, lineno, raw))
            settings[parts[0]] = parts[1].strip()
            continue
        parts = line.split()
        if len(parts) != 2 or not parts[0].isdigit():
            raise InputError("%s:%d: expected '<bytes> <qualified function name>': %r" % (name, lineno, raw))
        func = parts[1]
        if not re.fullmatch(r"[A-Za-z_~][A-Za-z0-9_:~]*", func):
            raise InputError("%s:%d: not a qualified function name: %r" % (name, lineno, func))
        if func in seen:
            raise InputError("%s:%d: duplicate budget for %s" % (name, lineno, func))
        seen.add(func)
        budgets.append((func, int(parts[0]), lineno))
    for key in ("compiler", "build-type", "target"):
        if key not in settings:
            raise InputError("%s: missing setting @%s" % (name, key))
    return settings, budgets


def parse_config(text):
    """Parses stack-usage-config.txt (key=value lines written by CMake)."""
    config = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        config[key.strip()] = value.strip()
    return config


def parse_su_line(line):
    """Parses one line of a .su file: returns (signature, bytes, location) or None for a line that is not a frame
    record; the location is the "file:line:col" of the function's definition.

    GCC writes "file:line:col:signature<TAB>bytes<TAB>qualifiers", Clang "file:line:col:name<TAB>bytes<TAB>qualifiers".
    """
    fields = line.rstrip("\n").split("\t")
    if len(fields) < 2 or not fields[1].strip().isdigit():
        return None
    # the signature follows the third colon of the location; a signature has colons of its own (A::f)
    match = re.match(r"^(.*?:\d+:\d+):(.*)$", fields[0])
    if match:
        return match.group(2), int(fields[1]), match.group(1)
    return fields[0], int(fields[1]), ""


def su_line_dynamic(line):
    """True if a .su record is "dynamic" without "bounded": the function allocates stack of a size known only at run
    time (alloca(), variable-length arrays), which its record does not include"""
    fields = line.rstrip("\n").split("\t")
    return len(fields) > 2 and "dynamic" in fields[2].split(",") and "bounded" not in fields[2].split(",")


def is_part_clone(signature):
    """True for the record of a ".part" clone made by partial inlining: GCC names it only by a number and its
    parameters ("0(int)"), at the location of the function it was split from."""
    return re.match(r"^\d+\(", signature) is not None


def match_params(func, signature):
    """Returns the parameter list of func in signature, or None; see match_signature()."""
    m = match_signature(func, signature)
    return m[0] if m else None


def match_signature(func, signature):
    """Returns (parameter list, qualifiers) of func in signature, or None if signature is not the frame of func itself.

    func is a qualified name; it must not be preceded by an identifier character or a scope (so that A::f matches
    neither XA::f nor B::A::f), and may be followed by ABI tags and template arguments (as nm names functions:
    "A::f[abi:cxx11](int)", "A::f<int>(int)"), then by its parameter list and nothing but qualifiers, the template
    arguments that GCC's .su records give ("[with T = int]") and clone suffixes (so that a lambda or local class
    inside func, which GCC names "A::f(...)::<lambda()>", is not func).  A record without parameters (as Clang may
    write it) that names the function exactly has the parameter list ""."""
    if signature.strip() == func:
        return "", ""
    pattern = r"(?<![A-Za-z0-9_:])" + re.escape(func) + r"(?=[(<\[])"
    for m in re.finditer(pattern, signature):
        i = m.end()
        # skip ABI tags and template arguments
        ok = True
        while i < len(signature) and signature[i] != "(":
            if signature.startswith("[abi:", i):
                end = signature.find("]", i)
                if end < 0:
                    ok = False
                    break
                i = end + 1
            elif signature[i] == "<":
                depth = 0
                while i < len(signature):
                    if signature[i] == "<":
                        depth += 1
                    elif signature[i] == ">":
                        depth -= 1
                        if not depth:
                            i += 1
                            break
                    i += 1
                if depth:
                    ok = False
                    break
            else:
                ok = False
                break
        if not ok or i >= len(signature):
            continue
        # find the end of the parameter list
        start = i + 1
        depth = 1
        i = start
        while i < len(signature) and depth:
            if signature[i] == "(":
                depth += 1
            elif signature[i] == ")":
                depth -= 1
            i += 1
        if depth:
            continue
        rest = re.sub(r"\[(clone|with) [^\]]*\]", "", signature[i:])
        if re.fullmatch(r"(\s*(const|volatile|noexcept|&&|&))*\s*", rest):
            return " ".join(signature[start:i - 1].split()), " ".join(rest.split())
    return None


def matches(func, signature):
    """True if signature is the frame of func itself, also with the template arguments of its scopes (as .su records
    write the members of class templates: "int S<T>::h(int) [with T = int]") ignored; see match_params()."""
    # most records do not even contain the function's own name
    if func.rsplit("::", 1)[-1] not in signature:
        return False
    return match_params(func, signature) is not None or match_params(func, strip_templates(signature)) is not None


class Symbol:
    """A function defined in an object file: its name as objdump -C writes it, its section, address and size, and
    its object file"""

    def __init__(self, name, section=None, address=None, size=None, obj=None):
        name = re.sub(r"^\.hidden ", "", name)
        self.name = name.replace("(anonymous namespace)", "{anonymous}")
        self.section = section
        self.address = address
        self.size = size
        self.obj = obj
        # a .cold fragment of a function shares its frame and has no record of its own
        self.is_cold = "[clone .cold" in name
        self.is_part = "[clone .part." in name and not self.is_cold

    def __repr__(self):
        return self.name

    def function(self):
        """Returns the qualified name of the function without template arguments and ABI tags, or None"""
        if not hasattr(self, "_function"):
            name = symbol_function(self.name)
            self._function = strip_templates(name) if name is not None else None
        return self._function


def object_functions(su_path, cache):
    """Returns the functions of the object file of a .su file by qualified name (see Symbol.function()), or None"""
    key = ("functions", su_path)
    if key not in cache:
        symbols = object_symbols(su_path, cache)
        rv = None
        if symbols is not None:
            rv = {}
            for sym in symbols:
                rv.setdefault(sym.function(), []).append(sym)
        cache[key] = rv
    return cache[key]


def object_symbols(su_path, cache):
    """Returns the functions (Symbol) defined in the object file of a .su file, or None if they cannot be read (no
    object file, as when a build removed it, or no objdump)."""
    obj = su_path.with_suffix(".o") if su_path else None
    if obj in cache:
        return cache[obj]
    rv = None
    if obj and obj.exists() and shutil.which("objdump"):
        try:
            out = subprocess.run(["objdump", "-t", "-C", str(obj)], stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, universal_newlines=True, check=True).stdout
            rv = []
            for line in out.splitlines():
                m = re.match(r"^([0-9a-fA-F]+) (.{7}) (\S+)\t([0-9a-fA-F]+) +(.*)$", line)
                if m and "F" in m.group(2):
                    rv.append(Symbol(m.group(5), m.group(3), int(m.group(1), 16), int(m.group(4), 16), obj))
        except (OSError, subprocess.CalledProcessError):
            rv = None
    cache[obj] = rv
    return rv


# the largest function that can be a wrapper of identical code folding: a branch to the function it was merged into,
# with an endbr64 (x86_64, 4 bytes, and a jmp of up to 5) or bti (aarch64, 4 bytes, and a b of 4) before it
WRAPPER_MAX_SIZE = 16


def disassemble(sym, cache):
    """Returns the instructions of a function as a list of (mnemonic, operands, branch target or None), the target
    being the demangled name of the function a branch or call goes to (from its relocation, or from objdump's
    "<name>" for one the assembler resolved); or None if the object file cannot be disassembled.  Padding and branch
    target markers (endbr64 on x86_64, bti and paciasp on aarch64) are left out."""
    key = ("disassembly", sym.obj, sym.address)
    if key in cache:
        return cache[key]
    rv = None
    if sym.obj is not None and sym.size and shutil.which("objdump"):
        try:
            out = subprocess.run(["objdump", "-dr", "-C", "--no-show-raw-insn", "-j", sym.section,
                "--start-address=%d" % sym.address, "--stop-address=%d" % (sym.address + sym.size), str(sym.obj)],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True, check=True).stdout
            rv = []
            for line in out.splitlines():
                m = re.match(r"^\s*[0-9a-f]+:\s+R_\S+\s+(.*?)(-0x[0-9a-f]+)?$", line)
                if m:
                    # the relocation of the instruction before it names its target
                    if rv:
                        rv[-1] = (rv[-1][0], rv[-1][1], m.group(1).replace("(anonymous namespace)", "{anonymous}"))
                    continue
                m = re.match(r"^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\s*(.*)$", line)
                if not m or m.group(1) in ("endbr64", "endbr32", "bti", "paciasp", "hint") \
                        or m.group(1).startswith("nop"):
                    continue
                target = re.search(r"<(.*?)(\+0x[0-9a-f]+)?>\s*$", m.group(2))
                rv.append((m.group(1), m.group(2),
                    target.group(1).replace("(anonymous namespace)", "{anonymous}") if target else None))
        except (OSError, subprocess.CalledProcessError):
            rv = None
    cache[key] = rv
    return rv


# x86_64 and aarch64 mnemonics of an unconditional direct branch and of a call
BRANCHES = ("jmp", "jmpq", "b")
CALLS = ("call", "callq", "bl")

# the largest function that can be a wrapper of identical code folding: a branch to the function it was merged into,
# with an endbr64 (x86_64, 4 bytes, and a jmp of up to 5) or bti (aarch64, 4 bytes, and a b of 4) before it
WRAPPER_MAX_SIZE = 16


def wrapper_target(sym, cache):
    """Returns the function that sym only branches to (a wrapper that identical code folding left of a function
    merged into an identical one: "endbr64; jmp f" on x86_64, "bti c; b f" on aarch64), or None"""
    if sym.size is None or sym.size > WRAPPER_MAX_SIZE:
        return None
    insns = disassemble(sym, cache)
    if not insns or len(insns) != 1 or insns[0][0] not in BRANCHES:
        return None
    return insns[0][2] or insns[0][1]


def overload_call(func, symbols, cache):
    """Returns (caller, callee) if one overload of func calls another (not as a tail call, which reuses the caller's
    frame): both frames are then on the stack, while the check charges only the larger; or None.  symbols are the
    functions of func in an object file."""
    overloads = [sym for sym in symbols if not sym.is_cold and "[clone " not in sym.name]
    if len(overloads) < 2:
        return None
    names = set(sym.name for sym in overloads)
    for sym in overloads:
        for mnemonic, _, target in disassemble(sym, cache) or ():
            if mnemonic in CALLS and target in names and target != sym.name:
                return sym.name, target
    return None


def symbol_function(name):
    """Returns the qualified name of the function of a symbol name as objdump -C writes it, without its return type
    (which template instances have), template arguments, ABI tags, parameters, qualifiers and clone suffixes, or
    None if it cannot be determined."""
    name = re.sub(r"\s*\[clone [^\]]*\]", "", name).strip()
    # the parameter list: the last top-level parenthesis
    depth = 0
    end = None
    for i in range(len(name) - 1, -1, -1):
        c = name[i]
        if c == ")":
            if not depth:
                end = i
            depth += 1
        elif c == "(":
            depth -= 1
            if not depth and end is not None:
                head = name[:i]
                break
    else:
        return None
    # ABI tags and template arguments at the end of the name
    while True:
        h = re.sub(r"\[abi:[^\]]*\]$", "", head)
        if h.endswith(">"):
            depth = 0
            for j in range(len(h) - 1, -1, -1):
                if h[j] == ">":
                    depth += 1
                elif h[j] == "<":
                    depth -= 1
                    if not depth:
                        h = h[:j]
                        break
        if h == head:
            break
        head = h
    # a return type before the name (template instances): the last top-level space
    depth = 0
    for j in range(len(head) - 1, -1, -1):
        c = head[j]
        if c in ">)":
            depth += 1
        elif c in "<(":
            depth -= 1
        elif c == " " and not depth:
            return head[j + 1:]
    return head


def collect_frames(build_dir):
    """Returns a list of (signature, bytes, .su path, location, dynamic) for every frame record under build_dir."""
    frames = []
    for path in sorted(Path(build_dir).rglob("*.su")):
        try:
            text = path.read_text(errors="replace")
        except OSError as e:
            raise InputError("cannot read %s: %s" % (path, e))
        for line in text.splitlines():
            rec = parse_su_line(line)
            if rec:
                frames.append((rec[0], rec[1], path, rec[2], su_line_dynamic(line)))
    return frames


def strip_templates(name):
    """Returns a qualified name without the template arguments of any of its scopes and without ABI tags"""
    name = re.sub(r"\[abi:[^\]]*\]", "", name)
    out = ""
    depth = 0
    for i, c in enumerate(name):
        # "operator<" and "operator<<" are names, not template arguments
        if c == "<" and not re.search(r"operator\s*<*$", name[max(0, i - 16):i]):
            depth += 1
        elif c == ">" and depth:
            depth -= 1
        elif not depth:
            out += c
    return out


def split_function(func, symbol):
    """True if symbol, a function of the symbol table (objdump -C), is a .part clone of func (partial inlining), in
    any overload or template instance (template arguments of every scope and ABI tags are ignored); .cold fragments
    are not clones of their own"""
    return symbol.is_part and symbol.function() == func


def check(budgets, frames):
    """Checks frames against budgets: returns a list of (func, budget, frame or None, signature or None, .part clone
    or None, (status, detail) or None).

    frames are (signature, bytes, path, location[, dynamic]) records.  The frame of a budgeted function is the largest
    of its records (all overloads; GCC records its .constprop and .isra clones, which are alternatives to it, under its
    own name).  It fails the check
    - as SPLIT if the object file's symbol table (objdump -t) has a .part clone of the function (partial inlining; not
      a .cold fragment): its head and clone are both on the stack, and GCC records the clone in ways that cannot be
      attributed reliably (as "0(...)", under the function's name, or once for several functions whose clones
      identical code folding merged); the clone is returned;
    - as FOLDED if the function is only a branch to another function (identical code folding merged it into an
      identical one, and it runs in that function's frame, which its own record does not show);
    - as DYNAMIC if one of its records is "dynamic" but not "bounded" (stack of a size known only at run time, which
      the record does not include).
    - as OVERLOAD-CALL if one of its overloads calls another (other than as a tail call): both frames are on the
      stack, while the function is charged only the larger.
    Overloads are charged the largest frame, not the sum, on the assumption that an overload does not call another
    one; OVERLOAD-CALL finds direct calls, not a call through another function (which a recursion of interpreted
    code does not make between overloads of the per-call path)."""
    sym_cache = {}
    paths = sorted(set(f[2] for f in frames), key=str)
    results = []
    for func, budget, _ in budgets:
        worst = None
        worst_sig = None
        split = None
        problem = None
        for f in frames:
            signature, size, path, location = f[:4]
            if is_part_clone(signature) or not matches(func, signature):
                continue
            if len(f) > 4 and f[4] and not problem:
                problem = ("DYNAMIC", "%s allocates stack of a size known only at run time (its record %r is "
                    "\"dynamic\" but not \"bounded\"), which the record does not include" % (func, signature))
            if worst is None or size > worst:
                worst = size
                worst_sig = signature
        for path in paths:
            symbols = (object_functions(path, sym_cache) or {}).get(func, ())
            for sym in symbols:
                if split_function(func, sym):
                    split = split or sym.name
                elif not problem and not sym.is_cold and "[clone " not in sym.name:
                    target = wrapper_target(sym, sym_cache)
                    if target:
                        problem = ("FOLDED", "%s was merged by identical code folding into %s; budget %s or make the "
                            "bodies differ" % (func, target, target))
            call = None if problem else overload_call(func, symbols, sym_cache)
            if call:
                problem = ("OVERLOAD-CALL", "%s calls %s: both frames are on the stack, while the budget is checked "
                    "against the larger; budget the sum or avoid the call" % call)
        results.append((func, budget, worst, worst_sig, split, problem))
    return results


def missing_symbols(budgets, frames):
    """Returns the .su files with records of budgeted functions whose object file's symbol table cannot be read (no
    object file, or no objdump)"""
    cache = {}
    paths = set(f[2] for f in frames if any(matches(func, f[0]) for func, _, _ in budgets))
    return sorted(str(path) for path in paths if object_symbols(path, cache) is None)


def flags_match(calibrated, actual):
    """True if two flag strings have the same flags, in any order."""
    return sorted(calibrated.split()) == sorted(actual.split())


def calibration_mismatch(settings, config):
    """Returns why the budgets do not apply to the build that config describes, or None if they do."""
    compiler = config.get("compiler", "?")
    if compiler.lower() != settings["compiler"].lower():
        return "compiler %s, budgets for %s" % (compiler, settings["compiler"])
    version = config.get("compiler-version", "?")
    calibrated_version = settings.get("compiler-version")
    if calibrated_version and version.split(".")[0] != calibrated_version.split(".")[0]:
        return "compiler version %s, budgets for major version %s (%s)" % (version,
            calibrated_version.split(".")[0], calibrated_version)
    build_type = config.get("build-type", "?")
    if build_type.lower() != settings["build-type"].lower():
        return "build type %s, budgets for %s" % (build_type, settings["build-type"])
    target = config.get("target", "?")
    if target != settings["target"]:
        return "target %s, budgets for %s" % (target, settings["target"])
    for key in ("build-type-flags", "cxx-flags", "hardening"):
        if key in settings and not flags_match(settings[key], config.get(key, "")):
            return "%s %r, budgets for %r" % (key, config.get(key, ""), settings[key])
    if "ipo" in settings and config.get("ipo", "") != settings["ipo"]:
        return "link-time optimization (ipo) %r, budgets for %r" % (config.get("ipo", ""), settings["ipo"])
    return None


def select_budgets(target, directory=None):
    """Returns the budget file of a target: the stack-usage-budgets*.txt file in directory (tools/) whose @target is
    target, or the default file (which the build is then reported to be not calibrated for)."""
    directory = Path(directory) if directory else DEFAULT_BUDGETS.parent
    for path in sorted(directory.glob("stack-usage-budgets*.txt")):
        try:
            settings, _ = parse_budgets(path.read_text(), str(path))
        except (OSError, InputError):
            continue
        if settings.get("target") == target:
            return path
    return DEFAULT_BUDGETS


def main(argv=None):
    parser = argparse.ArgumentParser(description="check libqore hot-path stack frames against budgets")
    parser.add_argument("build_dir", help="a build directory configured with -DQORE_STACK_USAGE_REPORT=ON")
    parser.add_argument("--budgets", help="the budget file (default: the tools/stack-usage-budgets*.txt file of the "
        "build's target, or %s)" % DEFAULT_BUDGETS)
    parser.add_argument("--force", action="store_true",
        help="check the budgets even if the build is not the one they are calibrated for")
    parser.add_argument("--verbose", "-v", action="store_true", help="print the signature that matched each budget")
    parser.add_argument("--allow-no-symbols", action="store_true",
        help="check the records only if the object files or objdump are missing (by default, that is an error)")
    args = parser.parse_args(argv)

    try:
        config_path = Path(args.build_dir) / CONFIG_NAME
        try:
            config = parse_config(config_path.read_text())
        except OSError:
            raise InputError("%s not found: configure the build with -DQORE_STACK_USAGE_REPORT=ON" % config_path)
        budget_path = Path(args.budgets) if args.budgets else select_budgets(config.get("target", ""))
        try:
            settings, budgets = parse_budgets(budget_path.read_text(), str(budget_path))
        except OSError as e:
            raise InputError("cannot read %s: %s" % (budget_path, e))
        args.budgets = str(budget_path)

        compiler = config.get("compiler", "?")
        version = config.get("compiler-version", "?")
        build_type = config.get("build-type", "?")
        target = config.get("target", "?")
        print("stack usage: %s %s, %s build for %s; budgets calibrated for %s, %s build for %s" % (compiler, version,
            build_type, target, settings["compiler"], settings["build-type"], settings["target"]))
        mismatch = calibration_mismatch(settings, config)
        if mismatch:
            if not args.force:
                print("stack usage: SKIPPED: the budgets are not calibrated for this build: %s (use --force to check "
                    "them anyway)" % mismatch)
                return 0
            print("stack usage: note: the budgets are not calibrated for this build: %s" % mismatch)

        frames = collect_frames(args.build_dir)
        if not frames:
            raise InputError("no .su files found under %s: was libqore built with -DQORE_STACK_USAGE_REPORT=ON?" %
                args.build_dir)
        missing = missing_symbols(budgets, frames)
        if missing:
            msg = ("the symbol tables of the object files of %s cannot be read (no object file, or no objdump): the "
                "functions that partial inlining split cannot be found" % ", ".join(missing))
            if not args.allow_no_symbols:
                raise InputError(msg + " (use --allow-no-symbols to check the records only)")
            print("stack usage: note: " + msg)
    except InputError as e:
        print("stack usage: ERROR: %s" % e, file=sys.stderr)
        return 2

    failed = 0
    for func, budget, worst, signature, split, problem in check(budgets, frames):
        if problem:
            print("%-8s %-55s budget %5d B: %s" % (problem[0], func, budget, problem[1]))
            failed += 1
            continue
        if split:
            print("SPLIT    %-55s budget %5d B: %s was split by partial inlining (%s); its frame can't be checked "
                "from .su: review it and add a budget for head+part, or prevent the split (e.g. "
                "__attribute__((noinline)) on the cold part or restructuring)" % (func, budget, func, split))
            failed += 1
            continue
        if worst is None:
            print("MISSING  %-55s budget %5d B: no frame found (renamed, or inlined into its callers)" % (func, budget))
            failed += 1
            continue
        status = "OK" if worst <= budget else "OVER"
        if worst > budget:
            failed += 1
        print("%-8s %-55s %5d B  budget %5d B  (%+d)" % (status, func, worst, budget, worst - budget))
        if args.verbose:
            print("         %s" % signature)
    if failed:
        print("stack usage: FAILED: %d function(s) over budget, missing, split, folded, dynamic or calling an "
            "overload; a frame on the per-call path of interpreted code costs its size at every level of a recursion: "
            "shrink it, or if the growth is justified, raise the budget in %s and explain why in the commit"
            % (failed, args.budgets))
        return 1
    print("stack usage: OK: %d function(s) within budget" % len(budgets))
    return 0


if __name__ == "__main__":
    sys.exit(main())
