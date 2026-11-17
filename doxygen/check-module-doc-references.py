#!/usr/bin/env python3
"""Fail a bundled module's final documentation pass on a reference to a module that it does not import.

A bundled binary module's final Doxygen pass imports only the tag files of the modules named in its
QORE_DOC_REFERENCE_MODULES_<module> list (CMakeLists.txt, issue #6350).  A reference to a symbol or page of a module
that is not in the list is only a Doxygen warning: the reference renders as plain text.  This script reads the
warnings of the pass (WARN_LOGFILE), prints them, and fails if an unresolved reference names a symbol or page that the
tag file of a module outside the list defines, so that the list is updated with the documentation (issue #6367).

Usage: check-module-doc-references.py --module <name> --log <warning log> --list-variable <variable>
           [--allowed <module>]... [--tag <module>=<tag file>]... [-- <doxygen command>...]

With a Doxygen command after "--", the script runs it first: the warning log is printed whatever its exit code (with
WARN_LOGFILE, Doxygen writes its errors only to the log), and a failed run fails the script with its exit code.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree


# the warnings of an unresolved reference (Doxygen 1.9 and later), with the name of the target
UNRESOLVED = (
    re.compile(r"warning: unable to resolve reference to '([^']+)' for [\\@]ref command"),
    re.compile(r"warning: explicit link request to '([^']+)' could not be resolved"),
    re.compile(r"warning: unable to resolve link to '([^']+)' for [\\@]link command"),
)

# the parameter list of a function reference, with any qualifier after it: "f()", "f(int x)", "f() const"
PARAMETERS = re.compile(r"\s*\(.*\)[\s\w]*$")


def normalize(name):
    """Returns the name of a reference target as the tag files spell it: no parameter list, no leading scope"""
    name = PARAMETERS.sub("", name.strip())
    if name.startswith("::"):
        name = name[2:]
    return name.replace("#", "::")


def tag_names(path):
    """Returns the names that a Doxygen tag file defines: compounds, their members (qualified with the compound only:
    an unqualified member name says nothing about its module, so a reference to one stays a warning), and the anchors
    of their pages and sections"""
    names = set()
    root = ElementTree.parse(path).getroot()
    for compound in root.iter("compound"):
        compound_name = compound.findtext("name")
        if compound_name:
            names.add(compound_name)
        for member in compound.findall("member"):
            member_name = member.findtext("name")
            if member_name and compound_name and compound.get("kind") not in ("page", "file", "dir"):
                names.add(f"{compound_name}::{member_name}")
        for anchor in compound.iter("docanchor"):
            if anchor.text:
                names.add(anchor.text)
    return names


def unresolved_targets(log):
    """Returns the targets of the unresolved references in a warning log, with the lines that report them"""
    targets = {}
    for line in log.splitlines():
        for pattern in UNRESOLVED:
            match = pattern.search(line)
            if match:
                targets.setdefault(normalize(match.group(1)), []).append(line)
                break
    return targets


def check(module, log, allowed, tags, list_variable):
    """Returns the error messages for the references of the module's documentation to modules outside its list

    module: the module whose final documentation pass wrote the warnings
    log: the text of the warnings
    allowed: the modules that the pass imports (QORE_DOC_REFERENCE_MODULES_<module>)
    tags: a dict of every bundled module's name to the names that its tag file defines
    list_variable: the name of the CMake variable that lists the allowed modules
    """
    errors = []
    for target, lines in sorted(unresolved_targets(log).items()):
        # a name that an imported module (or the module itself) defines is not a missing import
        if any(target in tags.get(name, ()) for name in allowed | {module}):
            continue
        owners = sorted(name for name, names in tags.items() if name != module and target in names)
        if not owners:
            continue
        errors.append(
            f"{lines[0]}\nerror: the documentation of module {module} refers to '{target}' of module "
            f"{' or '.join(owners)}, which is not in {list_variable} (CMakeLists.txt); add the module there")
    return errors


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--module", required=True)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--list-variable", required=True)
    parser.add_argument("--allowed", action="append", default=[])
    parser.add_argument("--tag", action="append", default=[], metavar="MODULE=TAGFILE")
    command = []
    if "--" in argv:
        index = argv.index("--")
        argv, command = argv[:index], argv[index + 1:]
    args = parser.parse_args(argv)

    rc = 0
    if command:
        # a log of an earlier run is not this run's
        if args.log.exists():
            args.log.unlink()
        rc = subprocess.run(command).returncode

    log = args.log.read_text(errors="replace") if args.log.exists() else ""
    # the warnings and errors are written to the log instead of the output of the build: show them as Doxygen would
    if log:
        sys.stderr.write(log if log.endswith("\n") else log + "\n")
    if rc:
        print(f"error: {' '.join(command)} failed with exit code {rc}", file=sys.stderr)
        return rc

    tags = {}
    for spec in args.tag:
        name, _, path = spec.partition("=")
        if not name or not path:
            parser.error(f"invalid --tag {spec!r}: expected MODULE=TAGFILE")
        # a tag file that a module's initial pass did not write (its documentation is not built) defines nothing
        if Path(path).exists():
            tags[name] = tag_names(path)

    errors = check(args.module, log, set(args.allowed), tags, args.list_variable)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
