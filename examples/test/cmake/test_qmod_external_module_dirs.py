#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Exercise the core build's module search policy and qcc's real depfile with a small CMake project."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
QCC = Path(os.environ.get("QORE_QCC_EXECUTABLE", ROOT / "build/qcc")).resolve()
QORE = Path(os.environ.get("QORE_BINARY", QCC.parent / "qore")).resolve()
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")

PROBE = """%modern
%try-module json
%define MissingJson
%endtry
%try-module BuildFeature
%endtry
module Probe {
    version = "1.0";
    desc = "Probe the external module path";
    author = "Qore";
    license = "MIT";
}
public string sub probe_value() {
%ifdef MissingJson
    return "in-tree only";
%else
    return BuildFeature::Value;
%endif
}
"""

FEATURE = """%modern
module BuildFeature {
    version = "1.0";
    desc = "External build fixture";
    author = "Qore";
    license = "MIT";
}
public namespace BuildFeature {
    public const Value = "selected";
}
"""


class ExternalModuleDirsTest(unittest.TestCase):
    def setUp(self):
        native = list((QCC.parent / "modules/json").glob("json-api-*.qmod"))
        if not (QCC.is_file() and QORE.is_file() and native):
            self.skipTest("requires the build's qcc, qore and json module")
        temp = tempfile.TemporaryDirectory(prefix="qore-external-dirs-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.build = self.root / "build"
        (self.root / "qlib").mkdir()
        (self.root / "qlib/Probe.qm").write_text(PROBE)
        self.external = self.root / "selected modules"
        self.external.mkdir()
        self.native = self.external / native[0].name
        shutil.copyfile(native[0], self.native)
        self.feature = self.external / "BuildFeature.qm"
        self.feature.write_text(FEATURE)
        self.prefix = self.root / "prefix"
        self.poison = self.root / "ambient modules"
        self.poison.mkdir(parents=True)
        shutil.copyfile(native[0], self.poison / native[0].name)
        (self.poison / "BuildFeature.qm").write_text("malformed external module\n")
        self.env = dict(os.environ, QORE_MODULE_DIR=str(self.poison), QORE_LIBDIR=str(QCC.parent),
                        LD_LIBRARY_PATH=str(QCC.parent), DYLD_LIBRARY_PATH=str(QCC.parent))
        self.env.pop("QORE_MODULE_DIR_ONLY", None)
        (self.root / "format.stamp").touch()
        (self.root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ExternalDirs NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(MODULE_DIR "${{CMAKE_INSTALL_PREFIX}}/lib/qore-modules")
set(USER_MODULE_DIR "${{CMAKE_INSTALL_PREFIX}}/share/qore-modules")
set(MODULE_VER_DIR "${{MODULE_DIR}}/3.0.0")
set(USER_MODULE_VER_DIR "${{USER_MODULE_DIR}}/3.0.0")
include("{ROOT}/cmake/QoreExternalModuleDirs.cmake")
qore_build_module_env(QORE_QM_METADATA_ENV "${{CMAKE_SOURCE_DIR}}/qlib")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_QCC_EXECUTABLE "{QCC}")
set(QCC_FORMAT_STAMP "${{CMAKE_SOURCE_DIR}}/format.stamp")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
qore_user_module("qlib/Probe.qm")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
''')
        self.qmod = self.build / "qlib-qmod/Probe.qmod"

    def invoke(self, *args, env=None, success=True):
        result = subprocess.run(args, env=self.env if env is None else env, text=True,
                                capture_output=True, timeout=120)
        self.assertEqual(success, result.returncode == 0, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def configure(self, *dirs):
        self.invoke(CMAKE, "-S", self.root, "-B", self.build, f"-DCMAKE_INSTALL_PREFIX={self.prefix}", *dirs)

    def compile(self, success=True):
        return self.invoke(CMAKE, "--build", self.build, "--target", "Probe-qmod", success=success)

    def value(self, module_dirs=None):
        record = (self.build / "qore-external-module-dirs.txt").read_text().strip()
        external = record if module_dirs is None else module_dirs
        path = os.pathsep.join(filter(None, (str(self.qmod.parent), str(self.root / "qlib"), external)))
        env = dict(self.env, QORE_MODULE_DIR_ONLY="1", QORE_MODULE_DIR=path)
        return self.invoke(QORE, "-W", "-l", "Probe", "-ne", 'printf("%s", probe_value());', env=env)

    def test_default_uses_optional_modules_in_install_prefix(self):
        lib = self.prefix / "lib/qore-modules"
        share = self.prefix / "share/qore-modules"
        lib.mkdir(parents=True)
        (share / "3.0.0").mkdir(parents=True)
        shutil.copyfile(self.native, lib / self.native.name)
        shutil.copyfile(self.feature, share / "3.0.0/BuildFeature.qm")
        self.configure()
        self.compile()
        # Load with the installed modules visible, independently of the search path recorded by the build.
        installed = os.pathsep.join(map(str, (lib / "3.0.0", share / "3.0.0", lib, share)))
        self.assertEqual("selected", self.value(installed))
        self.assertEqual(installed, (self.build / "qore-external-module-dirs.txt").read_text().strip())
        self.configure("-DQORE_EXTERNAL_MODULE_DIRS=")
        self.compile()
        self.assertEqual("in-tree only", self.value())

    def test_empty_and_disabled_exclude_external_modules(self):
        self.configure()
        self.compile()
        self.assertEqual("in-tree only", self.value())
        self.configure(f"-DQORE_EXTERNAL_MODULE_DIRS={self.poison}")
        self.assertIn("BuildFeature", self.compile(success=False))
        self.configure("-DQORE_EXTERNAL_MODULE_DIRS=")
        self.compile()
        self.assertEqual("in-tree only", self.value())
        self.configure(f"-DQORE_EXTERNAL_MODULE_DIRS={self.external}", "-DQORE_BUILD_WITH_EXTERNAL_MODULES=OFF")
        self.compile()
        self.assertEqual("in-tree only", self.value())

    def test_explicit_dirs_and_incremental_dependencies(self):
        self.configure(f"-DQORE_EXTERNAL_MODULE_DIRS={self.external};{self.poison}")
        self.compile()
        self.assertEqual("selected", self.value())
        self.assertEqual(os.pathsep.join(map(str, (self.external, self.poison))),
                         (self.build / "qore-external-module-dirs.txt").read_text().strip())
        before = self.qmod.stat().st_mtime_ns
        self.compile()
        self.assertEqual(before, self.qmod.stat().st_mtime_ns)
        self.native.touch()
        self.compile()
        self.assertGreater(self.qmod.stat().st_mtime_ns, before)
        self.feature.write_text(FEATURE.replace("selected", "updated"))
        self.compile()
        self.assertEqual("updated", self.value())
        after = self.qmod.stat().st_mtime_ns
        self.compile()
        self.assertEqual(after, self.qmod.stat().st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
