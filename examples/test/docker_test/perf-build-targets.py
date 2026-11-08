#!/usr/bin/env python3
"""Prints the build targets that the given tests need: qore and the module targets of the modules they require.

usage: perf-build-targets.py <build dir> <test>...

build-perf.sh builds only these for the performance tests (run_tests.sh -P), instead of the whole tree with every
user module's qmod (most of the 997 s of the job in pipeline 63444).  A test's modules are those of its %requires and
%try-module lines, and, for a test module required by path (ex: ../../TestCertificates.qm) or an included file,
those of its own lines, recursively.  A module of qlib/ has the target <module>-qmod, whose dependencies build the
qmods of the modules it requires and the binary modules; a binary module of modules/ has its own target; a module
of neither (ex: xml, from the image) has none.  Only the targets that the build tree has are printed (make help).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
# "%requires Util", "%requires(reexport) Mime", "%requires Util >= 1.0", "%try-module process", "%include x.qc"
DIRECTIVE = re.compile(r"(?m)^\s*%(requires|try-module|include)\b(?:\s*\([^)]*\))?\s+(\S+)")


def modules_of(paths):
    """The module names that the files require, following the files they require by path and include"""
    names = set()
    seen = set()
    todo = [Path(p).resolve() for p in paths]
    while todo:
        path = todo.pop()
        if path in seen or not path.is_file():
            continue
        seen.add(path)
        for kind, arg in DIRECTIVE.findall(path.read_text(errors="replace")):
            # a variable in the path, as in "${SCRIPT_DIR}/...": a path, not a module name
            if kind == "include" or "/" in arg or arg.endswith((".qm", ".qc", ".ql", ".q")):
                target = (path.parent / arg.replace("${SCRIPT_DIR}/", "")).resolve()
                # a module of qlib/ required by path (ex: ../../../qlib/DataProvider or ../../../qlib/QUnit.qm)
                try:
                    under_qlib = target.relative_to(ROOT / "qlib")
                except ValueError:
                    under_qlib = None
                if under_qlib is not None and kind != "include":
                    names.add(re.sub(r"\.qm$", "", under_qlib.parts[0]))
                else:
                    todo.append(target)
            else:
                names.add(arg)
    return names


def target_of(name):
    """The build target of a module, or None if this tree does not build it"""
    if (ROOT / "qlib" / (name + ".qm")).is_file() or (ROOT / "qlib" / name / (name + ".qm")).is_file():
        return name + "-qmod"
    if (ROOT / "modules" / name / "CMakeLists.txt").is_file():
        return name
    return None


def build_targets(build_dir):
    """The targets of the build tree, from make help"""
    out = subprocess.run(["make", "-C", str(build_dir), "help"], check=True, capture_output=True, text=True).stdout
    return {line[4:].split()[0] for line in out.splitlines() if line.startswith("... ")}


def main():
    if len(sys.argv) < 3:
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2
    available = build_targets(sys.argv[1])
    targets = ["qore"]
    for name in sorted(modules_of(sys.argv[2:])):
        target = target_of(name)
        if target and target in available:
            targets.append(target)
    missing = [t for t in targets if t not in available]
    if missing:
        print("the build tree has no target %s" % ", ".join(missing), file=sys.stderr)
        return 1
    print(" ".join(targets))
    return 0


if __name__ == "__main__":
    sys.exit(main())
