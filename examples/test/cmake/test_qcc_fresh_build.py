#!/usr/bin/env python3
"""Verify that a qcc source group builds in an empty build directory when its shared parse fails (qore#7727).

QORE_QCC_COMPILE_OBJECTS() first compiles a group of sources in one shared parse (tools/qore-qo-batch-bootstrap);
when that parse fails and no component spans several sources, the helper falls back to compiling each source on its
own and succeeds.  That run published nothing, so it put the group's ordering token, which is also the output of its
recipe, back as it was before; in an empty build directory there was none, so it removed it, and the Makefile
generator then stopped at the next target that depends on it: "No rule to make target
.../.qcc-scc/.qcc-batch-bootstrap.stamp, needed by .../.qcc-incremental-plan.stamp".  A build directory with a stamp
left from an earlier build did not show it.

The two sources of the group here define the same function: each compiles on its own, and their shared parse fails.
The test needs qcc and qore: QORE_QCC_EXECUTABLE and QORE_BINARY, or the build in QORE_TEST_BUILD_DIR, or the ones in
the PATH.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
HELPERS = ("incremental", "source-order", "batch-bootstrap", "incremental-plan", "prune")


def find_tool(env_name, name):
    """Returns the path of a Qore tool: from the environment, the test build, or the PATH."""
    path = os.environ.get(env_name)
    if path:
        return path
    build = os.environ.get("QORE_TEST_BUILD_DIR")
    if build and (Path(build) / name).is_file():
        return str(Path(build) / name)
    return shutil.which(name)


class QccFreshBuildTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        self.qcc = find_tool("QORE_QCC_EXECUTABLE", "qcc")
        self.qore = find_tool("QORE_BINARY", "qore")
        if not self.qcc or not self.qore:
            self.skipTest("qcc and qore are required")
        directory = tempfile.TemporaryDirectory(prefix="qore-qcc-fresh-build-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source = self.root / "source"
        self.source.mkdir()
        # each source compiles on its own; in one shared parse, the second definition of dup() is an error
        for name in ("a", "b"):
            (self.source / f"{name}.q").write_text(f'%modern\nsub dup() {{}}\nprintf("{name}\\n");\n')
        helpers = "\n".join(f'set(QORE_QCC_{helper.upper().replace("-", "_")}_HELPER '
                            f'"{(ROOT / "tools" / ("qore-qo-" + helper)).as_posix()}")' for helper in HELPERS)
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(QccFreshBuild NONE)
include("{(ROOT / "cmake" / "QoreMacros.cmake").as_posix()}")
set(QORE_QCC_EXECUTABLE "{Path(self.qcc).as_posix()}")
set(QORE_EXECUTABLE "{Path(self.qore).as_posix()}")
{helpers}
QORE_QCC_COMPILE_OBJECTS(objects GROUP Probe OUTPUT_DIR ${{CMAKE_BINARY_DIR}}/qo
    SCRIPT_DIR ${{CMAKE_BINARY_DIR}}/qo-scripts SOURCES a.q b.q ORDER_TARGETS_VAR order_targets)
add_custom_target(objects ALL)
add_dependencies(objects ${{order_targets}})
''')

    def build(self, generator):
        """Configures and builds the group twice in an empty build directory with the given generator."""
        build = self.root / ("build-" + generator.split()[0].lower())
        configure = subprocess.run([CMAKE, "-S", str(self.source), "-B", str(build), "-G", generator],
                                   capture_output=True, text=True)
        self.assertEqual(0, configure.returncode, configure.stdout + configure.stderr)
        output = ""
        for attempt in ("first", "second"):
            result = subprocess.run([CMAKE, "--build", str(build)], capture_output=True, text=True)
            output += result.stdout + result.stderr
            self.assertEqual(0, result.returncode, f"{generator}, {attempt} build:\n{output}")
        self.assertIn("batch bootstrap failed; falling back to per-source compilation", output,
                      "the shared parse of the group fails, and the helper falls back")
        self.assertNotIn("No rule to make target", output)
        objects = sorted(path.name for path in (build / "qo").glob("*.qo"))
        self.assertEqual(2, len(objects), f"each source was compiled: {objects}")

    def test_makefiles(self):
        """An empty build directory builds with the Makefile generator, whose next target needs the token."""
        if not shutil.which("make") and not shutil.which("gmake"):
            self.skipTest("make is required for the Makefile generator")
        self.build("Unix Makefiles")

    def test_ninja(self):
        """An empty build directory builds with Ninja too."""
        if not shutil.which("ninja"):
            self.skipTest("ninja is required for the Ninja generator")
        self.build("Ninja")


if __name__ == "__main__":
    unittest.main()
