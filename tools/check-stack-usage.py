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
the head calls the clone: GCC records the clone as "0(<params>)" at the head's own file:line:col, and the frame of the
function is counted as the head's plus the clone's.  GCC records .constprop and .isra clones under the function's own
name: they are alternatives, of which the largest counts.

usage: check-stack-usage.py [--budgets FILE] [--force] [--verbose] BUILD_DIR
exit status: 0 when every budget is met or the build is not one the budgets are for, 1 when a budget is exceeded
or a function is missing, 2 for a usage or input error
"""

import argparse
from pathlib import Path
import re
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


def matches(func, signature):
    """True if signature is the frame of func itself: the qualified name, not preceded by an identifier character or
    a scope (so that A::f matches neither XA::f nor B::A::f), followed by its parameter list and nothing but
    qualifiers and clone suffixes (so that a lambda or local class inside func, which GCC names
    "A::f(...)::<lambda()>", is not func)."""
    # a record without parameters (as Clang may write it) names the function exactly
    if signature.strip() == func:
        return True
    pattern = r"(?<![A-Za-z0-9_:])" + re.escape(func) + r"\("
    for m in re.finditer(pattern, signature):
        # find the end of the parameter list
        depth = 1
        i = m.end()
        while i < len(signature) and depth:
            if signature[i] == "(":
                depth += 1
            elif signature[i] == ")":
                depth -= 1
            i += 1
        if depth:
            continue
        rest = re.sub(r"\[clone [^\]]*\]", "", signature[i:])
        if re.fullmatch(r"(\s*(const|volatile|noexcept|&&|&))*\s*", rest):
            return True
    return False


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


def check(budgets, frames):
    """Checks frames against budgets: returns a list of (func, budget, worst frame or None, signature or None).

    frames are (signature, bytes, path, location) records.  A function's frame is the largest of its records (GCC
    prints its .constprop and .isra clones under its own name) plus the ".part" clone recorded at the same location
    (see is_part_clone()), which the function calls."""
    parts = {}
    for signature, size, path, location in frames:
        if location and is_part_clone(signature):
            key = (path, location)
            if key not in parts or size > parts[key][0]:
                parts[key] = (size, signature)
    results = []
    for func, budget, _ in budgets:
        worst = None
        worst_sig = None
        for signature, size, path, location in frames:
            if not matches(func, signature):
                continue
            part = parts.get((path, location))
            total = size + (part[0] if part else 0)
            if worst is None or total > worst:
                worst = total
                worst_sig = signature + (" + .part clone %s at %s" % (part[1], location) if part else "")
        results.append((func, budget, worst, worst_sig))
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
    for func, budget, worst, signature in check(budgets, frames):
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
