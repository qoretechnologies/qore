#!/usr/bin/env python3
"""Verify that documentation targets build the binary modules the documentation tools load before running them.

qdx and qjar run with the build tree's qore and load binary modules of the same tree; a docs target that runs them
before those modules are built fails with LOAD-MODULE-ERROR in a fresh build directory.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


def configured_tool_modules():
    """Returns QORE_DOC_TOOL_MODULES as QoreMacros.cmake sets it."""
    text = (ROOT / "cmake" / "QoreMacros.cmake").read_text()
    match = re.search(r"^\s*set\(QORE_DOC_TOOL_MODULES ([^)]*)\)", text, re.M)
    if not match:
        raise AssertionError("cmake/QoreMacros.cmake does not set QORE_DOC_TOOL_MODULES")
    return set(match.group(1).split())


def required_modules(path):
    """Returns the modules a Qore source file requires with %requires, outside of documentation code examples."""
    names = []
    in_code = False
    for line in path.read_text().splitlines():
        stripped = line.strip()
        if re.search(r"@code\b", stripped):
            in_code = True
        if re.search(r"@endcode\b", stripped):
            in_code = False
            continue
        if in_code:
            continue
        match = re.match(r"%requires\s+(\S+)", stripped)
        if match and match.group(1) != "qore":
            names.append(match.group(1))
    return names


def user_module_file(name):
    for candidate in (ROOT / "qlib" / f"{name}.qm", ROOT / "qlib" / name / f"{name}.qm"):
        if candidate.is_file():
            return candidate
    return None


def tool_binary_modules():
    """Returns the in-tree binary modules that qdx and qjar load, following user modules transitively."""
    binary = set()
    seen = set()
    pending = []
    for tool in ("qdx", "qjar"):
        pending.extend(required_modules(ROOT / "doxygen" / tool))
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        if (ROOT / "modules" / name).is_dir():
            binary.add(name)
            continue
        module = user_module_file(name)
        if module is None:
            raise AssertionError(f"documentation tools require {name!r}, which is neither in qlib/ nor modules/")
        pending.extend(required_modules(module))
    return binary


class DocToolDependenciesTest(unittest.TestCase):
    def test_tool_module_list_matches_what_the_tools_load(self):
        # The list drifted before: Util.qm began to require yaml and json while the docs targets depended only on
        # astparser and reflection, so docs-lang failed in a fresh build directory.
        self.assertEqual(tool_binary_modules(), configured_tool_modules())

    def test_lang_docs_targets_depend_on_the_tool_modules(self):
        text = (ROOT / "CMakeLists.txt").read_text()
        for target in ("docs-lang", "docs-lang-final-fast"):
            with self.subTest(target=target):
                pattern = r"add_dependencies\(" + re.escape(target) + r"\s+qore\s+\$\{QORE_DOC_TOOL_MODULES\}\)"
                self.assertIsNotNone(re.search(pattern, text),
                                     f"{target} does not depend on qore and QORE_DOC_TOOL_MODULES in CMakeLists.txt")

    def test_fast_module_docs_build_the_tool_modules_first(self):
        # the modules the tools actually load, so that the check does not depend on the list it verifies
        tools = sorted(tool_binary_modules())
        with tempfile.TemporaryDirectory(prefix="qore-doc-tool-deps-") as directory:
            root = Path(directory)
            source, build, built = root / "source", root / "build", root / "built"
            (source / "qlib").mkdir(parents=True)
            built.mkdir()
            (source / "qlib" / "App.qm").write_text("%modern\n")
            # qdx fails as the real one does when a module it loads has not been built yet
            (source / "qdx.py").write_text(f'''import sys
from pathlib import Path
missing = [name for name in {tools!r} if not (Path({str(built)!r}) / name).exists()]
if missing:
    sys.exit("LOAD-MODULE-ERROR: not built: " + ", ".join(missing))
if "--post" in sys.argv:
    sys.exit(0)
if any(arg.endswith("Doxyfile.cmake.tmpl") for arg in sys.argv):
    index = next(i for i, arg in enumerate(sys.argv) if arg.endswith("Doxyfile.cmake.tmpl"))
    Path(sys.argv[index + 1]).write_text("QUIET = YES\\n")
else:
    output = Path(sys.argv[-1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(Path(sys.argv[-2]).read_text())
''')
            (source / "doxygen.py").write_text("import sys\nsys.exit(0)\n")
            tool_targets = "\n".join(
                f'add_custom_target({name} COMMAND "${{CMAKE_COMMAND}}" -E touch "{(built / name).as_posix()}")'
                for name in tools)
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(DocToolDependencies NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "{Path(sys.executable).as_posix()}" "${{CMAKE_SOURCE_DIR}}/doxygen.py")
set(QORE_QDX_COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_SOURCE_DIR}}/qdx.py")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
{tool_targets}
qore_user_module("qlib/App.qm")
''')
            result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs-fast-App"],
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual(tools, sorted(path.name for path in built.iterdir()))


if __name__ == "__main__":
    unittest.main()
