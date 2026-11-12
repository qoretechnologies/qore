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

The frame of a function counts the clones that GCC makes of it with the rule of check(): for each location of a
record of the function (per overload group), the sum of the two largest records there (its head, the ".part" clone
of partial inlining, which the head calls and GCC records as "0(...)" or under the function's name at the head's
location, .constprop/.isra alternatives) and of the records of clones that identical code folding merged with its
clones (found by address in the object file's symbol table, objdump -t).  The head and its clone are two of those
records, so the sum can never be less than their frames together: growth cannot be hidden; it is more when the two
largest records are alternatives, which can only fail a check falsely.  A .part clone in the symbol table for which
no record can be found fails the check as UNACCOUNTED.

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
    as .su records write it, "{anonymous}"), and the section and address of its code, which identical code folding
    gives to every function it merges"""

    def __init__(self, name, section, address, size):
        self.name = name.replace("(anonymous namespace)", "{anonymous}")
        self.key = (section, address)
        # a .cold fragment of a function shares its frame and has no record of its own
        self.is_cold = "[clone .cold" in name
        self.is_part = "[clone .part." in name and not self.is_cold

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


def shape_of(func, signature):
    """Returns the overload group of a record or symbol of func: its number of parameters and qualifiers, or
    "template" for a template instance (which .su records name by its template parameters and the symbol table by
    its template arguments, so its instances are grouped by name); None if it is not func"""
    if "[with " in signature:
        return "template" if matches(func, signature) else None
    shape = arity_quals(func, signature)
    if shape is not None and re.search(r"(?<![A-Za-z0-9_:])" + re.escape(func) + r"(\[abi:[^\]]*\])*<", signature):
        return "template"
    return shape


def check(budgets, frames):
    """Checks frames against budgets: returns a list of (func, budget, worst frame or None, signature or None,
    unaccounted .part clone or None).

    frames are (signature, bytes, path, location) records.  The frame of a budgeted function is the largest, over the
    locations of the records of each of its overload groups (number of parameters and qualifiers; template instances
    by name), of the sum of the two largest records that may be the head there or a clone of it (the "pool"):
    - every record at the location: GCC records a .part clone (partial inlining) as "0(...)" at the function's
      location for a free or static member function, and under the function's own name there for a non-static member
      function or a template instance, as it does .constprop and .isra clones;
    - with the object file's symbol table (objdump -t), for each .part clone of the group, the largest record of the
      functions whose .part clone shares its address (identical code folding merged them; GCC records the merged
      clone once, at one of those functions' locations), each address once; .cold fragments are ignored, as they
      share the frame of the function they were split from.
    The head and its clone are two records of the pool, so the sum of its two largest records is never less than
    their sum, nor than any single record: growth cannot be hidden.  It overcharges when the two largest records are
    alternatives (a .constprop or .isra clone and the function, or two variants of a clone): a false failure only.
    A .part clone in the symbol table for which no record can be found (its group's pool has a single record while
    the symbol table also has the function itself, and no folded clone gives one) is UNACCOUNTED and fails the check."""
    by_path = {}
    for signature, size, path, location in frames:
        by_path.setdefault(path, []).append((signature, size, location))
    sym_cache = {}
    results = []
    for func, budget, _ in budgets:
        worst = None
        worst_sig = None
        unaccounted = None
        for path, records in by_path.items():
            symbols = object_symbols(path, sym_cache)

            def pools_of(f, shape):
                # the records of f's group shape, and every record at their locations, by location
                own = [(size, signature, location) for signature, size, location in records
                    if not is_part_clone(signature) and shape_of(f, signature) == shape]
                rv = {}
                for size, signature, location in own:
                    rv.setdefault(location, [])
                for signature, size, location in records:
                    if location in rv:
                        rv[location].append((size, signature, location))
                return own, rv

            shapes = set()
            for signature, size, location in records:
                if not is_part_clone(signature):
                    shape = shape_of(func, signature)
                    if shape is not None:
                        shapes.add(shape)
            parts = [] if symbols is None else [sym for sym in symbols if sym.is_part
                and shape_of(func, sym.name) is not None]
            for shape in shapes | set(shape_of(func, sym.name) for sym in parts):
                own, pools = pools_of(func, shape)
                # the records of the functions whose clones identical code folding merged with this group's clones,
                # each address once; they are added to the pool at every location of the group
                folded = []
                seen = set()
                for part in parts:
                    if shape_of(func, part.name) != shape or part.key in seen:
                        continue
                    seen.add(part.key)
                    candidates = []
                    for other in symbols:
                        if other is part or not other.is_part or other.key != part.key:
                            continue
                        other_func = symbol_function(other.name)
                        other_shape = shape_of(other_func, other.name) if other_func else None
                        if other_shape is None or (other_func == func and other_shape == shape):
                            continue
                        for pool in pools_of(other_func, other_shape)[1].values():
                            candidates += pool
                    if candidates:
                        folded.append(max(candidates))
                    elif not any(len(pool) > 1 for pool in pools.values()) \
                            and (not own or has_head(symbols, func, shape)):
                        # no record can be the clone: each location has at most the head
                        unaccounted = unaccounted or part.name
                for location, pool in (pools.items() if pools else [(None, [])]):
                    pool = sorted(pool + folded, reverse=True)
                    if not pool:
                        continue
                    total = pool[0][0] + (pool[1][0] if len(pool) > 1 else 0)
                    note = "" if len(pool) < 2 else " + %d B (%s at %s)" % (pool[1][0], pool[1][1], pool[1][2])
                    if worst is None or total > worst:
                        worst = total
                        worst_sig = pool[0][1] + note
        results.append((func, budget, worst, worst_sig, unaccounted))
    return results


def has_head(symbols, func, shape):
    """True if the symbol table has func itself (not a clone) with the given overload group"""
    return any("[clone " not in sym.name and shape_of(func, sym.name) == shape for sym in symbols)


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
