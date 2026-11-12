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

A function that GCC splits into a head and a ".part" clone (partial inlining) can have both frames on the stack, as
the head calls the clone, and its frame is counted as the head's plus the clone's.  GCC records the clone of a free or
static member function as "0(<params>)" at the head's own file:line:col, and that of a non-static member function or
a template instance under the function's own name at the same location, as it does .constprop and .isra clones, which
are alternatives to the function: the clones are therefore accounted for with the symbol table of the object file
(objdump -t), including those that identical code folding merged with another function's (the same address), and a
.part clone that no record accounts for fails the check as UNACCOUNTED, as its frame is unknown; without the object
file, records that may be a head and its clone are added (conservative).  See check().

usage: check-stack-usage.py [--budgets FILE] [--force] [--verbose] BUILD_DIR
exit status: 0 when every budget is met or the build is not one the budgets are for, 1 when a budget is exceeded,
a function is missing or a .part clone of a function is unaccounted, 2 for a usage or input error
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


def split_params(params):
    """Splits a parameter list at its top-level commas."""
    rv = []
    depth = 0
    cur = ""
    for c in params:
        if c in "(<[":
            depth += 1
        elif c in ")>]":
            depth -= 1
        if c == "," and not depth:
            rv.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        rv.append(cur.strip())
    return [] if rv == ["void"] else rv


def matches(func, signature):
    """True if signature is the frame of func itself; see match_params()."""
    return match_params(func, signature) is not None


def is_part_clone_of(func, signature, part_name):
    """True if part_name, a ".part" clone that nm names, may be a clone of the function that the .su record signature
    records: the same function name, the same number of parameters and the same qualifiers (const, &, &&).  The
    parameter types are not compared: nm and .su records spell them differently ("char const*" and "const char*", a
    std::string with its template arguments, typedefs resolved), so an overload with the same number of parameters
    also counts (conservative); for a template instance, which .su records name by its template parameters
    ("int f(T) [with T = int]") and nm by its template arguments ("int f<int>(int)"), the name only."""
    part = match_signature(func, part_name)
    if part is None:
        return False
    if "[with " in signature:
        return True
    own = match_signature(func, signature)
    return own is not None and len(split_params(part[0])) == len(split_params(own[0])) and part[1] == own[1]


class Symbol:
    """A function defined in an object file: its name as objdump -C writes it (with "(anonymous namespace)" written
    as .su records write it, "{anonymous}"), and the section, address and size of its code, which identical code
    folding gives to every function it merges"""

    def __init__(self, name, section, address, size):
        self.name = name.replace("(anonymous namespace)", "{anonymous}")
        self.key = (section, address, size)
        self.is_part = "[clone .part." in name

    def __repr__(self):
        return self.name


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
                    rv.append(Symbol(m.group(5), m.group(3), int(m.group(1), 16), int(m.group(4), 16)))
        except (OSError, subprocess.CalledProcessError):
            rv = None
    cache[obj] = rv
    return rv


def object_part_clones(su_path, cache):
    """Returns the names of the ".part" clones defined in the object file of a .su file, or None if they cannot be
    read; see object_symbols()."""
    symbols = object_symbols(su_path, cache)
    return None if symbols is None else [sym.name for sym in symbols if sym.is_part]


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
    """Returns a list of (signature, bytes, .su path, location) for every frame record under build_dir."""
    frames = []
    for path in sorted(Path(build_dir).rglob("*.su")):
        try:
            text = path.read_text(errors="replace")
        except OSError as e:
            raise InputError("cannot read %s: %s" % (path, e))
        for line in text.splitlines():
            rec = parse_su_line(line)
            if rec:
                frames.append((rec[0], rec[1], path, rec[2]))
    return frames


def arity_quals(func, signature):
    """Returns (number of parameters, qualifiers) of func in signature, or None; see match_signature()."""
    m = match_signature(func, signature)
    return (len(split_params(m[0])), m[1]) if m else None


class ObjectRecords:
    """The frame records of one .su file and the functions of its object file, for the accounting of .part clones"""

    def __init__(self, path, records, symbols):
        self.path = path
        # (signature, bytes, location)
        self.records = records
        # location -> the largest "0(...)" record there
        self.zero = {}
        for signature, size, location in records:
            if location and is_part_clone(signature):
                self.zero[location] = max(size, self.zero.get(location, 0))
        self.symbols = symbols

    def groups(self, func, shape=None):
        """Returns the records of func, optionally only those with the given (number of parameters, qualifiers):
        (location, signature) -> sizes, largest first"""
        rv = {}
        for signature, size, location in self.records:
            if is_part_clone(signature):
                continue
            aq = arity_quals(func, signature)
            if aq is None or (shape is not None and aq != shape):
                continue
            rv.setdefault((location, signature), []).append(size)
        for sizes in rv.values():
            sizes.sort(reverse=True)
        return rv

    def has_head(self, func, shape):
        """True if the object file defines func itself (not a clone) with the given shape"""
        return any(not sym.is_part and "[clone " not in sym.name and arity_quals(func, sym.name) == shape
            for sym in self.symbols)

    def part_records(self, func, shape):
        """Returns the frames that records of func with the given shape give for .part clones: a "0(...)" record at
        the location of a record of the function, the larger of two records of its signature at one location, or its
        only record if the object file does not define the function itself (the head was inlined into all callers)"""
        rv = []
        for (location, signature), sizes in self.groups(func, shape).items():
            if location in self.zero:
                rv.append(self.zero[location])
            elif len(sizes) > 1:
                # the head and the clone: the larger record is charged to the clone (conservative)
                rv.append(sizes[0])
            elif not self.has_head(func, shape):
                rv.append(sizes[0])
        return rv

    def account(self, func):
        """Accounts for the .part clones of the budgeted function func: returns (frames of its clones by shape,
        unaccounted clone or None).

        A clone that identical code folding merged with the clone of another function (the same code) is accounted
        for by that function's records.  The others are accounted for by the records of func with their shape, which
        do not tell overloads with as many parameters apart: they must give a frame for each such clone, or the excess
        clones are unaccounted, and each is charged the largest (conservative)."""
        frames = {}
        unaccounted = None
        own = {}
        for part in self.symbols:
            if not part.is_part or match_signature(func, part.name) is None:
                continue
            shape = arity_quals(func, part.name)
            folded = []
            for other in self.symbols:
                if other is part or not other.is_part or other.key != part.key:
                    continue
                other_func = symbol_function(other.name)
                other_shape = arity_quals(other_func, other.name) if other_func else None
                if other_shape is None or (other_func == func and other_shape == shape):
                    continue
                folded += self.part_records(other_func, other_shape)
            if folded:
                frames.setdefault(shape, []).append((max(folded), part.name, True))
            else:
                own.setdefault(shape, []).append(part)
        for shape, parts in own.items():
            records = self.part_records(func, shape)
            if len(parts) > len(records):
                unaccounted = unaccounted or parts[len(records)].name
            if records:
                for part in parts[:len(records)]:
                    frames.setdefault(shape, []).append((max(records), part.name, False))
        return frames, unaccounted


def check(budgets, frames):
    """Checks frames against budgets: returns a list of (func, budget, worst frame or None, signature or None,
    unaccounted .part clone or None).

    frames are (signature, bytes, path, location) records.  A function split by partial inlining has its head and its
    ".part" clone, which the head calls, on the stack: their frames add.  GCC records the clone of a free or static
    member function as "0(<params>)" at the function's file:line:col, and that of a non-static member function or a
    template instance under the function's own name at the same location, as it records .constprop and .isra clones,
    which are alternatives to the function and not frames on top of it.  The clones are therefore accounted from
    the symbols of the object file (objdump -t): every .part clone of a budgeted function (the same name, number of
    parameters and qualifiers; overloads with as many parameters are not told apart) must be accounted for (see
    ObjectRecords.account()) by
    - the records of the clone of another function that identical code folding merged it with (the same section,
      address and size), which GCC records once, at that function's location, or
    - a record of its own: a "0(...)" record at the location of a record of its function, a second record of its
      function's signature at the same location (the larger is charged to the clone), or its function's only record
      if the object file does not define the function itself (the head was inlined into all of its callers); the
      overloads with as many parameters must have as many such records as clones;
    otherwise a clone is UNACCOUNTED and fails the check, as its frame is unknown.  The frame of each group of records
    of the function is its head plus its clone (the largest clone of its shape for a folded one, conservatively), and
    the largest counts.  Template instances, which .su records name by
    their template parameters and the symbol table by their template arguments, are matched by name only and are
    not checked for unaccounted clones.  Without the object file (or objdump), two records of the same signature at
    the same location and a "0(...)" record at the location are added (conservative), and clones cannot be checked."""
    by_path = {}
    for signature, size, path, location in frames:
        by_path.setdefault(path, []).append((signature, size, location))
    sym_cache = {}
    objects = [ObjectRecords(path, records, object_symbols(path, sym_cache)) for path, records in by_path.items()]
    results = []
    for func, budget, _ in budgets:
        worst = None
        worst_sig = None
        unaccounted = None
        for obj in objects:
            groups = obj.groups(func)
            parts = [] if obj.symbols is None else [sym for sym in obj.symbols if sym.is_part
                and match_signature(func, sym.name) is not None]
            # every clone of the function must be accounted for
            part_frames = {}
            if obj.symbols is not None:
                part_frames, obj_unaccounted = obj.account(func)
                unaccounted = unaccounted or obj_unaccounted
            for (location, signature), sizes in groups.items():
                total = sizes[0]
                note = ""
                if location in obj.zero:
                    total += obj.zero[location]
                    note = " + .part clone recorded as 0(...) at %s" % location
                elif "[with " in signature:
                    # a template instance: by name (conservative)
                    if len(sizes) > 1 and (obj.symbols is None or parts):
                        total = sizes[0] + sizes[1]
                        note = " + .part clone recorded under the same name at %s" % location
                elif obj.symbols is None:
                    if len(sizes) > 1:
                        total = sizes[0] + sizes[1]
                        note = (" + .part clone recorded under the same name at %s (assumed: no object file to tell "
                            "it from a .constprop or .isra clone)" % location)
                else:
                    shape = arity_quals(func, signature)
                    clones = part_frames.get(shape, [])
                    if clones:
                        if len(sizes) > 1:
                            # the head and the clone
                            total = sizes[0] + sizes[1]
                            note = " + .part clone recorded under the same name at %s" % location
                        if obj.has_head(func, shape):
                            # a clone recorded elsewhere (folded): the head plus that clone
                            frame, name, folded = max(clones)
                            # the head: the smaller record if the clone of this group is recorded here too
                            head = sizes[-1]
                            if folded and head + frame > total:
                                total = head + frame
                                note = " + .part clone %s (%d B, recorded with a folded clone)" % (name, frame)
                if worst is None or total > worst:
                    worst = total
                    worst_sig = signature + note
        results.append((func, budget, worst, worst_sig, unaccounted))
    return results


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


def main(argv=None):
    parser = argparse.ArgumentParser(description="check libqore hot-path stack frames against budgets")
    parser.add_argument("build_dir", help="a build directory configured with -DQORE_STACK_USAGE_REPORT=ON")
    parser.add_argument("--budgets", default=str(DEFAULT_BUDGETS), help="the budget file (default: %(default)s)")
    parser.add_argument("--force", action="store_true",
        help="check the budgets even if the build is not the one they are calibrated for")
    parser.add_argument("--verbose", "-v", action="store_true", help="print the signature that matched each budget")
    args = parser.parse_args(argv)

    try:
        budget_path = Path(args.budgets)
        try:
            settings, budgets = parse_budgets(budget_path.read_text(), str(budget_path))
        except OSError as e:
            raise InputError("cannot read %s: %s" % (budget_path, e))
        config_path = Path(args.build_dir) / CONFIG_NAME
        try:
            config = parse_config(config_path.read_text())
        except OSError:
            raise InputError("%s not found: configure the build with -DQORE_STACK_USAGE_REPORT=ON" % config_path)

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
    except InputError as e:
        print("stack usage: ERROR: %s" % e, file=sys.stderr)
        return 2

    failed = 0
    for func, budget, worst, signature, unaccounted in check(budgets, frames):
        if unaccounted:
            print("UNACCOUNTED %-52s budget %5d B: the object file has the .part clone %r, which no record accounts "
                "for (as when GCC records it only as \"0(...)\" at a location without a record of the function, or "
                "for overloads with as many parameters); its frame is unknown" % (func, budget, unaccounted))
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
        print("stack usage: FAILED: %d function(s) over budget or missing; a frame on the per-call path of interpreted "
            "code costs its size at every level of a recursion: shrink it, or if the growth is justified, raise the "
            "budget in %s and explain why in the commit" % (failed, args.budgets))
        return 1
    print("stack usage: OK: %d function(s) within budget" % len(budgets))
    return 0


if __name__ == "__main__":
    sys.exit(main())
