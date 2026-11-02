#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check that QoreMacros.cmake links every executable, shared library and module atomically.

QORE_ATOMIC_LINK_RULES() (cmake/QoreMacros.cmake) makes each link write "<target>.partial" and rename it over
"<target>" afterwards, so a make killed with SIGKILL during a link (which cannot apply .DELETE_ON_ERROR) leaves the
previous complete output in place instead of a truncated one newer than its inputs; see
design/atomic-link-outputs.md.
"""

import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
MACROS = (ROOT / "cmake/QoreMacros.cmake").as_posix()

# The toy project's targets: an executable, a versioned shared library and a binary module.
TARGETS = ("atomic_exe", "atomic_lib", "atomic_mod")

# Run from RULE_LAUNCH_LINK in front of every command of a link rule: kills the whole process group of the build
# (as an OOM killer or a CI timeout would) right before the rename of the target named by ATOMIC_LINK_KILL_AT, which
# is after its link has completed.
LAUNCHER = r'''import os
import signal
import sys
command = sys.argv[1:]
kill_at = os.environ.get("ATOMIC_LINK_KILL_AT")
if kill_at and command[1:3] == ["-E", "rename"] and os.path.basename(command[-1]) == kill_at:
    os.killpg(os.getpgrp(), signal.SIGKILL)
os.execvp(command[0], command)
'''


def project_source(macros_include, launcher=None):
    """Returns the CMakeLists.txt of the toy project; the outputs go to a directory with a space in its name."""
    launch = ""
    if launcher:
        launch = (f'set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK '
                  f'"\\"{Path(sys.executable).as_posix()}\\" -I \\"{Path(launcher).as_posix()}\\"")\n')
    return f'''cmake_minimum_required(VERSION 3.14...3.31)
project(AtomicLink C CXX)
{macros_include}
{launch}set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/out dir")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/out dir")
add_subdirectory(sub)
'''


SUB_SOURCE = '''add_executable(atomic_exe main.c)
add_library(atomic_lib SHARED lib.cpp)
set_target_properties(atomic_lib PROPERTIES VERSION 1.2.3 SOVERSION 1)
# a binary module as QORE_BINARY_MODULE_INTERN2() builds it
add_library(atomic_mod MODULE mod.cpp)
set_target_properties(atomic_mod PROPERTIES PREFIX "" SUFFIX "-api-1.0.qmod")
# a POST_BUILD step sees the renamed output
add_custom_command(TARGET atomic_mod POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:atomic_mod>" "$<TARGET_FILE:atomic_mod>.post-build")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/target-files.txt" CONTENT
    "atomic_exe=$<TARGET_FILE:atomic_exe>\\natomic_lib=$<TARGET_FILE:atomic_lib>\\natomic_mod=$<TARGET_FILE:atomic_mod>\\n")
'''


def write_sources(sub, version):
    """Writes the target sources; each output contains the string "atomic-link-<version>"."""
    (sub / "main.c").write_text(f'#include <stdio.h>\nint main(void) {{ puts("atomic-link-{version}"); return 0; }}\n')
    (sub / "lib.cpp").write_text(f'extern "C" const char* atomic_lib() {{ return "atomic-link-{version}"; }}\n')
    (sub / "mod.cpp").write_text(f'extern "C" const char* atomic_mod() {{ return "atomic-link-{version}"; }}\n')


class AtomicLinkTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-atomic-link-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source with spaces"
        self.build = self.root / "build with spaces"

    def run_cmake(self, *args, expect=0, **kwargs):
        result = subprocess.run([CMAKE, *map(str, args)], text=True, capture_output=True, timeout=300, **kwargs)
        if expect is not None:
            self.assertEqual(expect, result.returncode, result.stdout + result.stderr)
        return result

    def make_project(self, macros_include=None, launcher=None, generator="Unix Makefiles", configure=()):
        if macros_include is None:
            # included twice: the second include must not apply the rule again
            macros_include = f'include("{MACROS}")\ninclude("{MACROS}")'
        (self.source / "sub").mkdir(parents=True)
        (self.source / "CMakeLists.txt").write_text(project_source(macros_include, launcher))
        (self.source / "sub/CMakeLists.txt").write_text(SUB_SOURCE)
        write_sources(self.source / "sub", "v1")
        self.run_cmake("-G", generator, "-S", self.source, "-B", self.build, "-DCMAKE_BUILD_TYPE=Release", *configure)
        files = (self.build / "target-files.txt").read_text().splitlines()
        self.target_files = {name: Path(path) for name, path in (line.split("=", 1) for line in files)}
        self.assertEqual(set(TARGETS), set(self.target_files))

    def link_commands(self, target):
        """Returns the commands of the link.txt of the target, each split as cmake_link_script splits it, with the
        paths relative to the directory it runs them in (the target's build directory) made absolute."""
        directory = self.build / "sub"
        link = directory / "CMakeFiles" / f"{target}.dir/link.txt"
        commands = [shlex.split(line) for line in link.read_text().splitlines() if line.strip()]
        return [[os.path.normpath(directory / argument) if argument.startswith("../") else argument
                 for argument in command] for command in commands]

    def assert_atomic_rule(self, target, commands):
        output = str(self.target_files[target])
        self.assertEqual(2, len(commands), commands)
        link, rename = commands
        # the link writes only the .partial file, and the rename moves it over the target
        self.assertEqual(1, link.count("-o"), link)
        self.assertEqual(output + ".partial", link[link.index("-o") + 1])
        self.assertNotIn(output, link)
        self.assertEqual(["-E", "rename", output + ".partial", output], rename[1:])
        self.assertEqual("cmake", Path(rename[0]).stem)

    def partial_files(self):
        return sorted(path.name for path in self.build.rglob("*.partial"))

    def test_rules_write_partial_then_rename(self):
        self.make_project()
        for target in TARGETS:
            with self.subTest(target=target):
                self.assert_atomic_rule(target, self.link_commands(target))
        self.run_cmake("--build", self.build, "-j2")
        self.assertEqual([], self.partial_files())
        for target in TARGETS:
            self.assertIn(b"atomic-link-v1", self.target_files[target].read_bytes())
        exe = self.target_files["atomic_exe"]
        self.assertEqual("atomic-link-v1\n", subprocess.check_output([str(exe)], text=True))
        module = self.target_files["atomic_mod"]
        self.assertEqual(module.read_bytes(), Path(str(module) + ".post-build").read_bytes())
        # the SONAME links of the versioned library point at the renamed file
        lib = self.target_files["atomic_lib"]
        links = [path for path in lib.parent.iterdir() if path.is_symlink()]
        self.assertEqual(2, len(links), links)
        for path in links:
            self.assertEqual(lib.resolve(), path.resolve())

    def test_sigkill_before_rename_keeps_previous_target(self):
        launcher = self.root / "launcher.py"
        launcher.write_text(LAUNCHER)
        self.make_project(launcher=launcher)
        self.run_cmake("--build", self.build, "-j1")
        version = 1
        for target in TARGETS:
            with self.subTest(target=target):
                output = self.target_files[target]
                previous = output.read_bytes()
                version += 1
                write_sources(self.source / "sub", f"v{version}")
                env = dict(os.environ, ATOMIC_LINK_KILL_AT=output.name)
                # the build runs in its own process group, which the launcher kills with SIGKILL
                result = self.run_cmake("--build", self.build, "-j1", expect=None, env=env, start_new_session=True)
                self.assertEqual(-signal.SIGKILL, result.returncode, result.stdout + result.stderr)
                # the previous complete target is untouched, and only the completed link's .partial is left
                self.assertEqual(previous, output.read_bytes())
                self.assertEqual([output.name + ".partial"], self.partial_files())
                partial = Path(str(output) + ".partial")
                self.assertIn(f"atomic-link-v{version}".encode(), partial.read_bytes())
                self.assertGreater(max((self.build / "sub/CMakeFiles" / f"{target}.dir").rglob("*.o"),
                                       key=lambda path: path.stat().st_mtime_ns).stat().st_mtime_ns,
                                   output.stat().st_mtime_ns)
                # the next build relinks the target, which the killed build left out of date
                result = self.run_cmake("--build", self.build, "-j1")
                self.assertIn("Linking", result.stdout)
                self.assertEqual([], self.partial_files())
                for name in TARGETS:
                    self.assertIn(f"atomic-link-v{version}".encode(), self.target_files[name].read_bytes())
                self.assertEqual(f"atomic-link-v{version}\n",
                                 subprocess.check_output([str(self.target_files["atomic_exe"])], text=True))
                # and a build with nothing to do links nothing
                result = self.run_cmake("--build", self.build, "-j1")
                self.assertNotIn("Linking", result.stdout)

    def test_ninja_rules_write_partial_then_rename(self):
        ninja = shutil.which("ninja")
        if not ninja:
            self.skipTest("ninja is not installed")
        self.make_project(generator="Ninja")
        rules = (self.build / "CMakeFiles/rules.ninja").read_text()
        link_rules = [line for line in rules.splitlines() if "-o $TARGET_FILE" in line]
        self.assertEqual(3, len(link_rules), link_rules)
        for line in link_rules:
            self.assertEqual(1, line.count("-o $TARGET_FILE.partial "), line)
            self.assertEqual(1, line.count("-E rename $TARGET_FILE.partial $TARGET_FILE"), line)
        self.run_cmake("--build", self.build)
        self.assertEqual([], self.partial_files())
        self.assertEqual("atomic-link-v1\n", subprocess.check_output([str(self.target_files["atomic_exe"])],
                                                                    text=True))

    def test_linker_depfile_dependencies_relink(self):
        # With a linker that writes a dependency file (CMake 3.27+ with GNU ld 2.35+ or lld), the file names the
        # output the linker wrote, "<target>.partial"; a change to a dependency that only the file records must still
        # relink the target
        ninja = shutil.which("ninja")
        generators = ["Unix Makefiles"] + (["Ninja"] if ninja else [])
        for generator in generators:
            with self.subTest(generator=generator):
                # no spaces here: GNU ld writes a path with a space unescaped into its dependency file, so a target
                # that depends on one is relinked by every build, with or without the atomic rule
                source = self.root / generator.replace(" ", "-") / "source"
                build = self.root / generator.replace(" ", "-") / "build"
                source.mkdir(parents=True)
                (source / "ext.c").write_text("int ext(void) { return 0; }\n")
                (source / "main.c").write_text("int ext(void);\nint main(void) { return ext(); }\n")
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(AtomicLinkDepfile C)
include("{MACROS}")
add_library(ext STATIC ext.c)
# a copy of the archive linked by name: no CMake dependency, only the linker's dependency file, tracks it
add_custom_command(TARGET ext POST_BUILD
    COMMAND "${{CMAKE_COMMAND}}" -E copy "$<TARGET_FILE:ext>" "${{CMAKE_BINARY_DIR}}/libextcopy.a")
add_executable(exe main.c)
add_dependencies(exe ext)
target_link_directories(exe PRIVATE "${{CMAKE_BINARY_DIR}}")
target_link_libraries(exe extcopy)
''')
                self.run_cmake("-G", generator, "-S", source, "-B", build)
                self.run_cmake("--build", build)
                if generator == "Ninja":
                    if "--dependency-file" not in (build / "build.ninja").read_text():
                        self.skipTest("the linker writes no dependency file")
                    # Ninja moves the dependency file into its log, recorded for the declared output
                    recorded = subprocess.check_output([ninja, "-C", str(build), "-t", "deps", "exe"], text=True)
                else:
                    depfile = build / "CMakeFiles/exe.dir/link.d"
                    if not depfile.exists():
                        self.skipTest("the linker writes no dependency file")
                    recorded = depfile.read_text()
                    self.assertTrue(recorded.startswith("exe.partial:"), recorded[:200])
                self.assertIn("libextcopy.a", recorded)
                self.assertNotIn("Linking", self.run_cmake("--build", build).stdout)
                # the modification time is now, after the link
                os.utime(build / "libextcopy.a")
                self.assertIn("Linking C executable", self.run_cmake("--build", build).stdout)
                self.assertNotIn("Linking", self.run_cmake("--build", build).stdout)
                self.assertEqual([], sorted(path.name for path in build.rglob("*.partial")))

    def check_configure_error(self, rules, expected):
        (self.source).mkdir(parents=True)
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(AtomicLinkError C)
{rules}
include("{MACROS}")
''')
        result = self.run_cmake("-S", self.source, "-B", self.build, expect=None)
        self.assertNotEqual(0, result.returncode, result.stdout)
        self.assertIn(expected, result.stderr)

    def test_rule_without_output_operand_fails_configure(self):
        self.check_configure_error('set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <OBJECTS> -o<TARGET>")',
                                   'CMAKE_C_LINK_EXECUTABLE does not write "-o <TARGET>"')

    def test_rule_with_several_commands_fails_configure(self):
        self.check_configure_error('set(CMAKE_C_CREATE_SHARED_MODULE "<CMAKE_C_COMPILER> -o <TARGET>" "strip <TARGET>")',
                                   "CMAKE_C_CREATE_SHARED_MODULE has 2 commands")

    def test_applied_rule_is_kept(self):
        # a rule that already writes "-o <TARGET>.partial" (as Qorus's cmake/QorusLinker.cmake makes it) is not
        # changed, so the two never stack
        self.source.mkdir(parents=True)
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(AtomicLinkApplied C)
set(_rule "<CMAKE_C_COMPILER> <OBJECTS> -o <TARGET>.partial" "\\"${{CMAKE_COMMAND}}\\" -E rename <TARGET>.partial <TARGET>")
set(CMAKE_C_LINK_EXECUTABLE "${{_rule}}")
include("{MACROS}")
if(NOT CMAKE_C_LINK_EXECUTABLE STREQUAL _rule)
    message(FATAL_ERROR "applied rule changed: ${{CMAKE_C_LINK_EXECUTABLE}}")
endif()
# a project without C++ has no C++ rule to change
if(DEFINED CMAKE_CXX_LINK_EXECUTABLE)
    message(FATAL_ERROR "CMAKE_CXX_LINK_EXECUTABLE defined without C++")
endif()
''')
        self.run_cmake("-S", self.source, "-B", self.build)

    def installed_config(self):
        """Generates an installed QoreConfig.cmake with the real QoreMacros.cmake beside it in a stand-in prefix."""
        prefix = self.root / "installed prefix"
        for directory in ("bin", "lib", "include", "lib/cmake/Qore"):
            (prefix / directory).mkdir(parents=True, exist_ok=True)
        (prefix / "lib/libqore.so").touch()
        qore = prefix / "bin/qore"
        qore.write_text("#!/bin/sh\nexit 0\n")
        qore.chmod(0o755)
        generator = self.root / "generator"
        generator.mkdir()
        (generator / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(QorePackageFixture NONE)
set(CMAKE_SHARED_LIBRARY_PREFIX lib)
set(CMAKE_SHARED_LIBRARY_SUFFIX .so)
set(CMAKE_INSTALL_FULL_BINDIR "{prefix.as_posix()}/bin")
set(CMAKE_INSTALL_FULL_LIBDIR "{prefix.as_posix()}/lib")
set(CMAKE_INSTALL_FULL_INCLUDEDIR "{prefix.as_posix()}/include")
set(CMAKE_INSTALL_FULL_DATADIR "{prefix.as_posix()}/share")
set(MODULE_API_MAJOR 1)
set(MODULE_API_MINOR 4)
set(QORE_LLVM_MAJOR_VERSION 21)
set(myprefix "{prefix.as_posix()}")
include("{(ROOT / "cmake/QoreConfigurePackage.cmake").as_posix()}")
qore_configure_package_configs()
''')
        self.run_cmake("-S", generator, "-B", generator / "build")
        config = prefix / "lib/cmake/Qore"
        shutil.copyfile(generator / "build/cmake/install/QoreConfig.cmake", config / "QoreConfig.cmake")
        shutil.copyfile(MACROS, config / "QoreMacros.cmake")
        return config

    def test_find_package_applies_rule_once(self):
        config = self.installed_config()
        # an external module: find_package(Qore) after project(), twice, and an explicit include of the macros as well
        self.make_project(f'''find_package(Qore CONFIG REQUIRED)
find_package(Qore CONFIG REQUIRED)
include("${{QORE_CMAKE_DIR}}/QoreMacros.cmake")
if(NOT QORE_CMAKE_DIR STREQUAL "{config.as_posix()}")
    message(FATAL_ERROR "wrong Qore: ${{QORE_CMAKE_DIR}}")
endif()''', configure=[f"-DQore_DIR={config}"])
        for target in TARGETS:
            with self.subTest(target=target):
                self.assert_atomic_rule(target, self.link_commands(target))
        self.run_cmake("--build", self.build, "-j2")
        self.assertEqual([], self.partial_files())

    def test_qore_build_tree_links_atomically(self):
        """Every executable, shared library and module of the Qore build in QORE_TEST_BUILD_DIR links atomically."""
        build = os.environ.get("QORE_TEST_BUILD_DIR")
        if not build:
            self.skipTest("QORE_TEST_BUILD_DIR is not set")
        build = Path(build)
        links = sorted(build.rglob("CMakeFiles/*.dir/link.txt"))
        if not links:
            self.skipTest(f"{build} was not generated for Unix Makefiles")
        checked = set()
        for link in links:
            commands = [shlex.split(line) for line in link.read_text().splitlines() if line.strip()]
            if "-o" not in commands[0]:
                # a static library: archived, not linked
                continue
            with self.subTest(link=str(link.relative_to(build))):
                output = commands[0][commands[0].index("-o") + 1]
                self.assertTrue(output.endswith(".partial"), commands[0])
                renames = [command for command in commands if command[1:3] == ["-E", "rename"]]
                self.assertEqual([["-E", "rename", output, output.removesuffix(".partial")]],
                                 [command[1:] for command in renames])
                self.assertEqual(renames[0], commands[1])
            checked.add(link.parent.name.removesuffix(".dir"))
        # libqore, the qore interpreter, qpp, qcc and the bundled binary modules
        for target in ("libqore", "qore", "qpp", "qcc", "reflection", "astparser"):
            self.assertIn(target, checked)


if __name__ == "__main__":
    unittest.main()
