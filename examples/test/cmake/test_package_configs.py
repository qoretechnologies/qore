#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise the real build-tree and installed SDK configuration generator."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class PackageConfigsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-sdk-config-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.prefix = self.root / "installed"
        for directory in ("bin", "lib", "include", "lib/cmake/Qore"):
            (self.prefix / directory).mkdir(parents=True, exist_ok=True)
        (self.prefix / "lib/libqore.so").touch()
        qore = self.prefix / "bin/qore"
        qore.write_text("#!/bin/sh\nexit 0\n")
        qore.chmod(0o755)
        (self.prefix / "lib/cmake/Qore/QoreMacros.cmake").write_text("set(QORE_MACROS_LOADED TRUE)\n")

    def cmake(self, *args):
        result = subprocess.run([CMAKE, *map(str, args)], text=True, capture_output=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def generate(self, name):
        source = self.root / name / "source with spaces"
        build = self.root / name / "build with spaces"
        (source / "cmake").mkdir(parents=True)
        (source / "cmake/QoreMacros.cmake").write_text("set(QORE_MACROS_LOADED TRUE)\n")
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(QorePackageFixture NONE)
set(CMAKE_SHARED_LIBRARY_PREFIX lib)
set(CMAKE_SHARED_LIBRARY_SUFFIX .so)
set(CMAKE_INSTALL_FULL_BINDIR "{self.prefix}/bin")
set(CMAKE_INSTALL_FULL_LIBDIR "{self.prefix}/lib")
set(CMAKE_INSTALL_FULL_INCLUDEDIR "{self.prefix}/include")
set(CMAKE_INSTALL_FULL_DATADIR "{self.prefix}/share")
set(MODULE_API_MAJOR 1)
set(MODULE_API_MINOR 4)
set(QORE_LLVM_MAJOR_VERSION 21)
set(myprefix "{self.prefix}")
include("{ROOT}/cmake/QoreConfigurePackage.cmake")
qore_configure_package_configs()
''')
        self.cmake("-S", source, "-B", build)
        shutil.copyfile(self.prefix / "bin/qore", build / "qore")
        (build / "qore").chmod(0o755)
        return source, build

    def consume(self, config, name):
        source = self.root / (name + "-consumer")
        source.mkdir()
        (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.20)
project(QoreConsumer NONE)
find_package(Qore CONFIG REQUIRED)
if(NOT QORE_MACROS_LOADED)
    message(FATAL_ERROR "SDK helper macros were not loaded")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/result.txt"
    "${QORE_IN_BUILD_TREE}\\n${QORE_CMAKE_DIR}\\n${QORE_INCLUDE_DIR}\\n${QORE_EXECUTABLE}\\n${QORE_QCC_EXECUTABLE}\\n${QORE_LIBRARY}\\n")
''')
        build = source / "build"
        self.cmake("-S", source, "-B", build, f"-DQore_DIR={config}")
        return (build / "result.txt").read_text().splitlines()

    def test_installed_config_is_independent_of_build_directory(self):
        first_source, first = self.generate("first")
        _, second = self.generate("second")
        config = first / "cmake/install/QoreConfig.cmake"
        self.assertEqual(config.read_bytes(), (second / "cmake/install/QoreConfig.cmake").read_bytes())
        self.assertNotIn(str(first_source), config.read_text())
        self.assertNotIn(str(first), config.read_text())
        installed = self.prefix / "lib/cmake/Qore"
        shutil.copyfile(config, installed / "QoreConfig.cmake")
        # Prove the installed export works with the original source and build gone.
        shutil.rmtree(first.parent)
        report = self.consume(installed, "installed")
        self.assertEqual(["FALSE", str(installed), str(self.prefix / "include"),
                          str(self.prefix / "bin/qore"), str(self.prefix / "bin/qcc"),
                          str(self.prefix / "lib/libqore.so")], report)

    def test_build_tree_and_symlink_keep_build_outputs(self):
        source, build = self.generate("development")
        alias = self.root / "build-alias"
        alias.symlink_to(build, target_is_directory=True)
        for name, directory in (("direct", build), ("symlink", alias)):
            report = self.consume(directory / "cmake", name)
            self.assertEqual(["TRUE", str(source / "cmake"), str(source / "include"),
                              str(build / "qore"), str(build / "qcc"), str(build / "libqore.so")], report)


if __name__ == "__main__":
    unittest.main()
