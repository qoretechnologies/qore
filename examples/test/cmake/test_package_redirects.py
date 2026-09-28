#!/usr/bin/env python3
"""Exercise FetchContent discovery across reconfiguration and stale package redirects.

Copyright (C) 2026 Qore Technologies, s.r.o.
SPDX-License-Identifier: MIT
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "cmake/QoreResetPackageRedirects.cmake"
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class PackageRedirectsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        version = subprocess.check_output([CMAKE, "--version"], text=True)
        major, minor = map(int, re.search(r"cmake version (\d+)\.(\d+)", version).groups())
        if (major, minor) < (3, 24):
            raise unittest.SkipTest("FetchContent package redirects require CMake 3.24 or later")

    def run_cmake(self, *args):
        return subprocess.run([CMAKE, *map(str, args)], text=True, capture_output=True, timeout=60)

    def assert_success(self, result):
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def fixture(self, root):
        source = root / "source with spaces"
        build = root / "build with spaces"
        dependency = source / "dependency"
        dependency.mkdir(parents=True)
        (dependency / "fixture.h").write_text("#define FIXTURE_VALUE 42\n")
        (dependency / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.14)
project(QoreRedirectFixture NONE)
add_library(fixture_headers INTERFACE)
add_library(QoreRedirectFixture::headers ALIAS fixture_headers)
target_include_directories(fixture_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
file(WRITE "${CMAKE_BINARY_DIR}/provider.txt" "bundled")
''')
        (source / "consumer.c").write_text(
            '#include <fixture.h>\nint main(void) { return FIXTURE_VALUE != 42; }\n')
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.24)
if(STALE_REDIRECTS)
    # Model redirects surviving CMake's startup cleanup, without requiring a foreign UID.
    # CMake can chmod a directory it owns, so chmod alone cannot reproduce that failure.
    file(COPY "${{STALE_REDIRECTS}}/" DESTINATION "${{CMAKE_FIND_PACKAGE_REDIRECTS_DIR}}")
    file(CHMOD "${{CMAKE_FIND_PACKAGE_REDIRECTS_DIR}}"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
if(ENABLE_REDIRECT_RESET)
    include("{HELPER.as_posix()}")
endif()
project(RedirectConsumer C)
include(FetchContent)
find_package(QoreRedirectFixture QUIET CONFIG NO_DEFAULT_PATH
    PATHS "${{CMAKE_FIND_PACKAGE_REDIRECTS_DIR}}")
if(NOT QoreRedirectFixture_FOUND)
    FetchContent_Declare(qoreredirectfixture
        SOURCE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/dependency" OVERRIDE_FIND_PACKAGE)
    FetchContent_MakeAvailable(qoreredirectfixture)
endif()
if(ENABLE_REDIRECT_RESET)
    # Re-inclusion must retain the redirects created during this configure run.
    include("{HELPER.as_posix()}")
endif()
add_executable(consumer consumer.c)
target_link_libraries(consumer PRIVATE QoreRedirectFixture::headers)
''')
        return source, build

    def make_readonly(self, directory):
        directory.chmod(0o555)
        self.addCleanup(lambda: directory.chmod(0o755) if directory.exists() else None)
        if os.access(directory, os.W_OK):
            self.skipTest("this account bypasses directory write permissions")

    def test_fresh_and_repeated_configuration(self):
        with tempfile.TemporaryDirectory(prefix="qore-package-redirects-") as directory:
            source, build = self.fixture(Path(directory))
            for _ in range(3):
                self.assert_success(self.run_cmake("-S", source, "-B", build,
                                                  "-DENABLE_REDIRECT_RESET=ON"))
                self.assert_success(self.run_cmake("--build", build))
                self.assertEqual("bundled", (build / "provider.txt").read_text())
                self.assertTrue((build / "CMakeFiles/pkgRedirects/qoreredirectfixture-config.cmake").is_file())
                self.assertEqual([], list((build / "CMakeFiles").glob("pkgRedirects.stale-*")))

    def test_stale_redirects_recover_on_every_run(self):
        with tempfile.TemporaryDirectory(prefix="qore-package-redirects-") as directory:
            source, build = self.fixture(Path(directory))
            args = ("-S", source, "-B", build)
            self.assert_success(self.run_cmake(*args, "-DENABLE_REDIRECT_RESET=ON"))
            redirects = build / "CMakeFiles/pkgRedirects"
            original = (redirects / "qoreredirectfixture-config.cmake").read_text()
            saved = Path(directory) / "previous configure redirects"
            shutil.copytree(redirects, saved)
            # Reproduce the actual failure: an old config is found, but its targets do not exist.
            failed = self.run_cmake(*args, "-DENABLE_REDIRECT_RESET=OFF", f"-DSTALE_REDIRECTS={saved}")
            self.assertNotEqual(0, failed.returncode, failed.stdout + failed.stderr)
            self.assertIn("QoreRedirectFixture::headers", failed.stderr)
            self.assertIn("target was not found", failed.stderr)
            for run in range(2):
                # Preserve a cached package hint pointing to the directory, just as Boost_DIR does.
                self.assert_success(self.run_cmake(*args, "-DENABLE_REDIRECT_RESET=ON",
                    f"-DQoreRedirectFixture_DIR={redirects}"))
                self.assert_success(self.run_cmake("--build", build))
                backups = list((build / "CMakeFiles").glob("pkgRedirects.stale-*"))
                self.assertEqual(run + 1, len(backups))
                for backup in backups:
                    self.assertEqual(original, (backup / "qoreredirectfixture-config.cmake").read_text())
                    backup.chmod(0o755)

    def test_system_package_hint_is_preserved(self):
        with tempfile.TemporaryDirectory(prefix="qore-package-redirects-") as directory:
            root = Path(directory)
            source, build = self.fixture(root)
            self.assert_success(self.run_cmake("-S", source, "-B", build,
                                              "-DENABLE_REDIRECT_RESET=ON"))
            redirects = build / "CMakeFiles/pkgRedirects"
            saved = root / "previous configure redirects"
            shutil.copytree(redirects, saved)
            installed = root / "installed package"
            installed.mkdir()
            (installed / "fixture.h").write_text("#define FIXTURE_VALUE 42\n")
            (installed / "QoreRedirectFixtureConfig.cmake").write_text('''
add_library(QoreRedirectFixture::headers INTERFACE IMPORTED)
set_target_properties(QoreRedirectFixture::headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}")
file(WRITE "${CMAKE_BINARY_DIR}/provider.txt" "system")
''')
            self.assert_success(self.run_cmake("-S", source, "-B", build,
                "-DENABLE_REDIRECT_RESET=ON", f"-DSTALE_REDIRECTS={saved}",
                f"-DQoreRedirectFixture_DIR={installed}"))
            self.assert_success(self.run_cmake("--build", build))
            self.assertEqual("system", (build / "provider.txt").read_text())
            for backup in (build / "CMakeFiles").glob("pkgRedirects.stale-*"):
                backup.chmod(0o755)

    def test_unwritable_parent_fails_before_discovery(self):
        with tempfile.TemporaryDirectory(prefix="qore-package-redirects-") as directory:
            parent = Path(directory) / "unwritable parent"
            redirects = parent / "pkgRedirects"
            redirects.mkdir(parents=True)
            (redirects / "stale-config.cmake").write_text("# stale\n")
            self.make_readonly(parent)
            try:
                result = self.run_cmake(f"-DCMAKE_FIND_PACKAGE_REDIRECTS_DIR={redirects}", "-P", HELPER)
                self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertIn("Cannot reset stale CMake package redirects", result.stderr)
                self.assertIn("use a new build directory", " ".join(result.stderr.split()))
                self.assertTrue((redirects / "stale-config.cmake").is_file())
            finally:
                parent.chmod(0o755)

    def test_no_redirect_directory(self):
        self.assert_success(self.run_cmake("-P", HELPER))

    def test_subproject_preserves_parent_redirects(self):
        with tempfile.TemporaryDirectory(prefix="qore-package-redirects-") as directory:
            root = Path(directory)
            source = root / "parent project"
            child = source / "child"
            child.mkdir(parents=True)
            (child / "CMakeLists.txt").write_text(f'include("{HELPER.as_posix()}")\n')
            (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(Parent NONE)
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/parent-config.cmake" "# current run\n")
add_subdirectory(child)
if(NOT EXISTS "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/parent-config.cmake")
    message(FATAL_ERROR "Parent package redirects were discarded")
endif()
''')
            self.assert_success(self.run_cmake("-S", source, "-B", root / "build"))


if __name__ == "__main__":
    unittest.main()
