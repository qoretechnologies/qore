#!/usr/bin/env python3
"""Verify source-only module resource installs with CMAKE_EXECUTABLE (or cmake).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class UserModuleResourcesTest(unittest.TestCase):
    def test_transitive_documentation_tags(self):
        with tempfile.TemporaryDirectory(prefix="qore-module-doc-tags-") as directory:
            root = Path(directory)
            source = root / "source with spaces"
            modules = source / "qlib"
            modules.mkdir(parents=True)
            # Shared/cyclic dependencies must be visited once. Optional and unavailable
            # external modules must not introduce nonexistent tag-file inputs.
            (modules / "App.qm").write_text("%modern\n%requires Shared\n%requires Leaf\n")
            (modules / "Shared.qm").write_text(
                "%modern\n%requires(reexport) native\n%requires Leaf\n%requires external\n"
                "%try-module optional\n%requires unbuilt\n")
            (modules / "Leaf.qm").write_text("%modern\n%requires Shared\n%requires App\n")
            for name in ("native", "unbuilt"):
                (source / "modules" / name).mkdir(parents=True)
            (source / "modules/native/CMakeLists.txt").write_text("add_custom_target(docs-native)\n")
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ModuleDocTags NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
add_subdirectory(modules/native)
_qore_collect_module_doc_tags(App tags)
string(REPLACE ";" "\\n" lines "${{tags}}")
file(WRITE "${{CMAKE_BINARY_DIR}}/tags.txt" "${{lines}}\\n")
''')
            build = root / "build with spaces"
            result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("Warning", result.stderr)
            self.assertEqual([
                "Shared.tag=../../Shared/html", "Leaf.tag=../../Leaf/html",
                f"{build}/modules/native/native.tag=../../native/html",
            ], (build / "tags.txt").read_text().splitlines())

    def test_external_documentation_inputs(self):
        for separated in (False, True):
            with self.subTest(separated=separated), tempfile.TemporaryDirectory(
                    prefix="qore-module-doc-inputs-") as directory:
                root = Path(directory)
                source = root / "source"
                module = source / ("qlib/Fixture" if separated else "qlib")
                module.mkdir(parents=True)
                (module / "Fixture.qm").write_text("%modern\n")
                sources = ["Fixture.qm"]
                if separated:
                    (module / "Part.qc").write_text("# fixture\n")
                    sources.append("Part.qc")
                # Resource names containing Qore suffixes must not become inputs.
                resources = ("logo.svg", "logo.qm.svg", "schema.yaml", "schema.json", "wire.proto")
                for name in resources:
                    (module / name).write_text("resource\n")
                (source / "Doxyfile.in").write_text("INPUT = @_dox_input@\n")
                registration = "qlib/Fixture" if separated else "qlib/Fixture.qm"
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ModuleDocInputs NONE)
set(QORE_MODULE_DIR_FOR_DOCS "${{CMAKE_SOURCE_DIR}}/qlib:${{CMAKE_BINARY_DIR}}/modules")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(DOXYGEN_FOUND TRUE)
set(QORE_USERMODULE_DOXYGEN_TEMPLATE "${{CMAKE_SOURCE_DIR}}/Doxyfile.in")
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E true)
set(QORE_USER_MODULES_DIR share/qore-modules)
add_custom_target(docs)
add_custom_target(docs-module)
qore_external_user_module("{registration}" "")
''')
                build = root / "build~snapshot"
                result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
                inputs = (build / "doxygen/Doxyfile.Fixture").read_text().removeprefix("INPUT = ").split()
                self.assertEqual([str(build / "doxygen/qlib/Fixture" / (name + ".dox.h"))
                                  for name in sources], inputs)
                result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs"],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_directory_resources(self):
        for registration in ('qore_user_module("qlib/Fixture")',
                             'qore_external_user_module("qlib/Fixture" "")',
                             'qore_user_modules("qlib/Fixture")'):
            with self.subTest(registration=registration), tempfile.TemporaryDirectory(
                    prefix="qore-module-resources-") as directory:
                root = Path(directory)
                source = root / "source with spaces"
                module = source / "qlib/Fixture"
                module.mkdir(parents=True)
                resources = {"Fixture.qm": "%modern\n", "Part.qc": "# fixture\n",
                             "schema.json": '{"paths": {}}\n', "schema.yaml": "paths: {}\n",
                             "logo.svg": "<svg/>\n", "wire.proto": 'syntax = "proto3";\n'}
                for name, content in resources.items():
                    (module / name).write_text(content)
                (module / "unpackaged.txt").write_text("not a module resource")
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ModuleResources NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(DOXYGEN_FOUND FALSE)
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen TRUE)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_QM_SOURCE_INSTALL_COMPONENT source-modules)
{registration}
''')
                build = root / "build-debug"
                install = root / "install"
                for command in ([CMAKE, "-S", str(source), "-B", str(build),
                                 f"-DCMAKE_INSTALL_PREFIX={install}"],
                                [CMAKE, "--install", str(build), "--component", "source-modules"]):
                    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    self.assertNotIn("Warning", result.stderr)
                installed = install / "share/qore-modules/Fixture"
                self.assertEqual(set(resources), {path.name for path in installed.iterdir()})
                for name, content in resources.items():
                    self.assertEqual(content, (installed / name).read_text())


if __name__ == "__main__":
    unittest.main()
