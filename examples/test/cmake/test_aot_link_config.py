#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise exported AOT link recipes with real CMake targets and a linker."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class AotLinkConfigTest(unittest.TestCase):
    def setUp(self):
        # CMake's Makefile generator cannot represent '$()' in its build root;
        # exercise those characters in imported library paths instead.
        temporary = tempfile.TemporaryDirectory(prefix="qore-aot-link- ' ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.build = self.root / "build"
        self.env = {key: value for key, value in os.environ.items() if not key.startswith("RPM_")}
        (self.source / "probe.c").write_text("int answer(void) { return 42; }\n")
        (self.source / "main.c").write_text("int answer(void); int main(void) { return answer() == 42 ? 0 : 1; }\n")

    def run_tool(self, *args, success=True):
        result = subprocess.run(list(map(str, args)), env=self.env, text=True, capture_output=True, timeout=60)
        self.assertEqual(success, result.returncode == 0, result.stdout + result.stderr)
        if success:
            self.assertNotIn("CMake Warning", result.stderr)
        return result

    def configure(self, body, extra="", success=True):
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(QoreAotLinkFixture C)
# These private build flags must never be exported into a consumer's recipe.
set(CMAKE_SHARED_LINKER_FLAGS "-specs=/nonexistent/package-builder.specs")
include("{ROOT}/cmake/QoreAOTLinkConfig.cmake")
{body}
qore_generate_aot_link_config("${{CMAKE_BINARY_DIR}}/aot-link.conf"
    "cxx=cc\\nlibdir=/usr/lib\\nemit_rpath=0\\n" "${{dynamic}}" "${{static}}")
''')
        return self.run_tool("cmake", "-S", self.source, "-B", self.build,
                             "-DCMAKE_BUILD_TYPE=Release", "-DQORE_AOT_LINK_FLAGS=" + extra, success=success)

    def generate(self):
        self.run_tool("cmake", "--build", self.build, "--target", "qore-aot-link-config")
        return dict(line.split("=", 1) for line in (self.build / "aot-link.conf").read_text().splitlines())

    def test_pkgconfig_interfaces_and_configuration_specific_imports_link(self):
        library = self.root / "installed lib ' dollar$();.a"
        obj = self.root / "probe.o"
        self.run_tool("cc", "-c", self.source / "probe.c", "-o", obj)
        self.run_tool("ar", "rcs", library, obj)
        self.configure(f'''add_library(Imported STATIC IMPORTED)
set_target_properties(Imported PROPERTIES IMPORTED_CONFIGURATIONS RELEASE
    IMPORTED_LOCATION_RELEASE [==[{library}]==])
add_library(PkgConfig::Probe INTERFACE IMPORTED)
set_target_properties(PkgConfig::Probe PROPERTIES INTERFACE_LINK_LIBRARIES Imported
    INTERFACE_LINK_OPTIONS "LINKER:--as-needed")
qore_aot_link_arguments(static PkgConfig::Probe)
qore_aot_link_arguments(dynamic m)
''')
        config = self.generate()
        self.assertEqual(["-lm"], shlex.split(config["dynamic_libs"]))
        self.assertEqual(["-Wl,--as-needed", str(library)], shlex.split(config["static_libs"]))
        command = shlex.join(["cc", str(self.source / "main.c"), "-o", str(self.root / "program")])
        self.run_tool("sh", "-ec", command + " " + config["static_libs"])
        self.run_tool(self.root / "program")
        self.assertNotIn("package-builder", (self.build / "aot-link.conf").read_text())

    def test_real_build_target_and_transitive_link_only_dependencies(self):
        self.configure('''add_library(Probe STATIC probe.c)
add_library(Headers INTERFACE)
add_library(Wrapper INTERFACE)
target_link_libraries(Wrapper INTERFACE "$<LINK_ONLY:Probe>" Headers)
qore_aot_link_arguments(static Wrapper)
qore_aot_link_arguments(dynamic)
''')
        self.run_tool("cmake", "--build", self.build, "--target", "Probe")
        config = self.generate()
        self.assertEqual([str(self.build / "libProbe.a")], shlex.split(config["static_libs"]))
        self.run_tool("sh", "-ec", shlex.join(["cc", str(self.source / "main.c"), "-o",
                                              str(self.root / "program")]) + " " + config["static_libs"])
        self.run_tool(self.root / "program")

    def test_explicit_consumer_flags_and_shell_options_are_retained(self):
        self.configure('''add_library(Flags INTERFACE)
target_link_options(Flags INTERFACE "SHELL:-Wl,--as-needed -pthread")
qore_aot_link_arguments(dynamic Flags)
qore_aot_link_arguments(static)
''', extra="-Wl,-z,relro")
        config = self.generate()
        self.assertEqual(["-Wl,--as-needed", "-pthread", "-Wl,-z,relro"], shlex.split(config["dynamic_libs"]))
        self.assertEqual(["-Wl,-z,relro"], shlex.split(config["static_libs"]))

    def test_missing_and_cyclic_targets_fail_configuration(self):
        for body, error in [
            ("qore_aot_link_arguments(static Missing::Library)", "Unresolved AOT link dependency"),
            ("add_library(A INTERFACE)\nadd_library(B INTERFACE)\n"
             "target_link_libraries(A INTERFACE B)\ntarget_link_libraries(B INTERFACE A)\n"
             "qore_aot_link_arguments(static A)", "Cyclic AOT link dependency"),
        ]:
            with self.subTest(error=error):
                result = self.configure(body, success=False)
                self.assertIn(error, result.stderr)

    def test_invalid_argument_preserves_previous_configuration(self):
        self.configure("qore_aot_link_arguments(dynamic m)\nqore_aot_link_arguments(static)")
        self.generate()
        config = self.build / "aot-link.conf"
        previous = config.read_bytes()
        inputs = self.build / "CMakeFiles/qore-aot-link/Release"
        (inputs / "dynamic-0").write_text("first\nsecond")
        result = self.run_tool("cmake", "-DINPUT=" + str(inputs), "-DOUTPUT=" + str(config), "-P",
                               ROOT / "cmake/QoreWriteAOTLinkConfig.cmake", success=False)
        self.assertIn("cannot contain line breaks", result.stderr)
        self.assertEqual(previous, config.read_bytes())
        self.assertFalse(Path(str(config) + ".tmp").exists())

    def test_unchanged_configuration_retains_timestamp(self):
        self.configure("qore_aot_link_arguments(dynamic m)\nqore_aot_link_arguments(static)")
        self.generate()
        config = self.build / "aot-link.conf"
        os.utime(config, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
        self.generate()
        self.assertEqual(1_600_000_000_000_000_000, config.stat().st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
