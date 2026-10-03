#!/usr/bin/env python3
"""Verify source-only module resource installs with CMAKE_EXECUTABLE (or cmake).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
import shutil
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class UserModuleResourcesTest(unittest.TestCase):
    def test_build_time_configuration_and_reciprocal_links(self):
        doxygen = shutil.which("doxygen")
        if not doxygen:
            self.skipTest("Doxygen is required for rendered-link verification")
        with tempfile.TemporaryDirectory(prefix="qore-qlib-doc-passes-") as directory:
            root = Path(directory)
            source, build = root / "source with spaces", root / "build with spaces"
            (source / "qlib").mkdir(parents=True)
            for name, peer in (("App", "Reference"), ("Reference", "App")):
                (source / "qlib" / f"{name}.qm").write_text("%modern\n")
                (source / f"{name}.dox").write_text(
                    f'/** @mainpage {name}\n@section {name.lower()}intro Introduction\n'
                    f'@ref {peer.lower()}intro "{peer}"\n*/\n')
            # Model qdx's build-time regeneration of the base Doxyfile. Phase
            # settings must survive both the initial build and a repeated build.
            (source / "qdx.py").write_text(r'''import sys
from pathlib import Path
if any(arg.endswith("Doxyfile.cmake.tmpl") for arg in sys.argv):
    template = next(i for i, arg in enumerate(sys.argv) if arg.endswith("Doxyfile.cmake.tmpl"))
    config = Path(sys.argv[template + 1])
    name = config.name.removeprefix("Doxyfile.")
    build = config.parent.parent
    source = Path(__file__).parent
    config.write_text(f"""PROJECT_NAME = {name}
INPUT = "{source / (name + '.dox')}"
OUTPUT_DIRECTORY = "{build / 'docs/modules' / name}"
GENERATE_TAGFILE = "{build / (name + '.tag')}"
GENERATE_LATEX = NO
QUIET = YES
WARN_IF_DOC_ERROR = YES
WARN_AS_ERROR = YES
""")
''')
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(QlibDocPasses NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "{doxygen}")
set(QORE_QDX_COMMAND "{sys.executable}" "${{CMAKE_SOURCE_DIR}}/qdx.py")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
set(QORE_DOC_EXTRA_MODULES_App Reference)
set(QORE_DOC_EXTRA_MODULES_Reference App)
qore_user_module("qlib/App.qm")
qore_user_module("qlib/Reference.qm")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
''')
            result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertFalse((build / "doxygen/Doxyfile.App").exists())
            for iteration in range(2):
                result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                        capture_output=True, text=True, timeout=60)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("warning:", (result.stdout + result.stderr).lower())
                for name, peer in (("App", "Reference"), ("Reference", "App")):
                    html = (build / f"docs/modules/{name}/html/index.html").read_text()
                    self.assertIn(f'../../{peer}/html/index.html#{peer.lower()}intro', html)
                    tag = build / f"{name}.tag"
                    timestamp = tag.stat().st_mtime_ns
                    result = subprocess.run([doxygen, str(build / f"doxygen/Doxyfile.{name}.final")],
                                            cwd=build, capture_output=True, text=True, timeout=30)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    self.assertEqual(timestamp, tag.stat().st_mtime_ns)
            # Final rendering must still reject genuinely broken references.
            with (source / "App.dox").open("a") as output:
                output.write("/** @page broken Broken\n@ref nonexistentintro\n*/\n")
            result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                    capture_output=True, text=True, timeout=60)
            self.assertNotEqual(0, result.returncode)
            self.assertIn("nonexistentintro", result.stdout + result.stderr)

    def test_same_named_class_files_are_isolated_across_final_passes(self):
        doxygen = shutil.which("doxygen")
        if not doxygen:
            self.skipTest("Doxygen is required for rendered-link verification")
        with tempfile.TemporaryDirectory(prefix="qore-qlib-doc-collision-") as directory:
            root = Path(directory)
            source, build = root / "source with spaces", root / "build with spaces"
            for name, peer in (("App", "Reference"), ("Reference", "App")):
                module = source / "qlib" / name
                module.mkdir(parents=True)
                (module / f"{name}.qm").write_text(
                    f'/** @mainpage {name}\n@section {name.lower()}intro Introduction\n'
                    f'@ref {name}::Shared "Local class" and @ref {peer.lower()}intro "Peer"\n*/\n')
                (module / "Shared.qc").write_text(
                    f'namespace {name} {{ /** The {name} class. */ class Shared {{}}; }}\n')
            # Model qdx's documented output contract: separated .qc files are
            # emitted beside the requested .qm output, retaining their basenames.
            (source / "qdx.py").write_text(r'''import sys
from pathlib import Path
if "--post" in sys.argv:
    sys.exit(0)
if any(arg.endswith("Doxyfile.cmake.tmpl") for arg in sys.argv):
    index = next(i for i, arg in enumerate(sys.argv) if arg.endswith("Doxyfile.cmake.tmpl"))
    config = Path(sys.argv[index + 1])
    name = config.name.removeprefix("Doxyfile.")
    output = Path(next(arg for arg in sys.argv if arg.startswith("-M=")).split(":", 1)[1])
    build = config.parent.parent
    config.write_text(f"""PROJECT_NAME = {name}
INPUT = "{output}" "{output.parent / 'Shared.qc.dox.h'}"
OUTPUT_DIRECTORY = "{build / 'docs/modules' / name}"
GENERATE_TAGFILE = "{build / (name + '.tag')}"
GENERATE_LATEX = NO
QUIET = YES
WARN_IF_DOC_ERROR = YES
WARN_AS_ERROR = YES
""")
else:
    source, output = map(Path, sys.argv[-2:])
    if source.is_dir():
        source = source / (source.name + ".qm")
    output.write_text(source.read_text())
    (output.parent / "Shared.qc.dox.h").write_text((source.parent / "Shared.qc").read_text())
''')
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ModuleDocCollision NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "{doxygen}")
set(QORE_QDX_COMMAND "{sys.executable}" "${{CMAKE_SOURCE_DIR}}/qdx.py")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
set(QORE_DOC_EXTRA_MODULES_App Reference)
set(QORE_DOC_EXTRA_MODULES_Reference App)
qore_user_module("qlib/App")
qore_user_module("qlib/Reference")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
''')
            result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            # Serial initial passes reproduce the overwrite deterministically;
            # this must work without relying on a lucky parallel schedule.
            for name in ("App", "Reference"):
                result = subprocess.run([CMAKE, "--build", str(build), "--target", f"docs-{name}"],
                                        capture_output=True, text=True, timeout=60)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            for name in ("App", "Reference"):
                result = subprocess.run([doxygen, str(build / f"doxygen/Doxyfile.{name}.final")],
                                        cwd=build, capture_output=True, text=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual("", result.stderr)
                html = (build / f"docs/modules/{name}/html/index.html").read_text()
                tag = ET.parse(build / f"{name}.tag").getroot()
                filename = next(compound.findtext("filename") for compound in tag.findall("compound")
                                if compound.findtext("name") == f"{name}::Shared")
                self.assertIn(f'href="{filename}"', html)
                self.assertTrue((build / f"docs/modules/{name}/html" / filename).is_file())
                generated = build / f"doxygen/qlib/{name}/Shared.qc.dox.h"
                self.assertEqual((source / f"qlib/{name}/Shared.qc").read_text(), generated.read_text())
            self.assertFalse((build / "doxygen/qlib/Shared.qc.dox.h").exists())
            result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("warning:", (result.stdout + result.stderr).lower())

    def test_documentation_only_dependencies(self):
        for docs_enabled in (False, True):
            with self.subTest(docs_enabled=docs_enabled), tempfile.TemporaryDirectory(
                    prefix="qore-module-doc-only-") as directory:
                root = Path(directory)
                source = root / "source with spaces"
                modules = source / "qlib"
                modules.mkdir(parents=True)
                (modules / "App.qm").write_text("%modern\n%requires Runtime\n")
                (modules / "Runtime.qm").write_text("%modern\n")
                (modules / "Reference.qm").write_text("%modern\n%requires Leaf\n")
                (modules / "Leaf.qm").write_text("%modern\n")
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ModuleDocOnly NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_DOC_EXTRA_MODULES_App Reference Runtime Reference)
# Reciprocal documentation references must not create a build cycle.
set(QORE_DOC_EXTRA_MODULES_Reference App)
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E echo)
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
add_custom_target(docs)
file(MAKE_DIRECTORY "${{CMAKE_BINARY_DIR}}/doxygen")
foreach(mod App Runtime Reference Leaf)
    add_custom_target(${{mod}}-qmod)
    if ({"TRUE" if docs_enabled else "FALSE"})
        add_custom_target(docs-${{mod}}
            COMMAND "${{CMAKE_COMMAND}}" -E touch
                "${{CMAKE_BINARY_DIR}}/doxygen/Doxyfile.${{mod}}" VERBATIM)
    endif()
endforeach()
set_property(GLOBAL PROPERTY QORE_USER_MODULE_TARGETS App Runtime Reference Leaf)
_qore_collect_module_doc_tags(App tags ${{QORE_DOC_EXTRA_MODULES_App}})
string(REPLACE ";" "\\n" lines "${{tags}}")
file(WRITE "${{CMAKE_BINARY_DIR}}/tags.txt" "${{lines}}\\n")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
get_target_property(aot App-qmod MANUALLY_ADDED_DEPENDENCIES)
file(WRITE "${{CMAKE_BINARY_DIR}}/aot.txt" "${{aot}}")
if (TARGET docs-App)
    get_target_property(docs docs-App MANUALLY_ADDED_DEPENDENCIES)
    file(WRITE "${{CMAKE_BINARY_DIR}}/docs.txt" "${{docs}}")
    get_target_property(final docs-App-final MANUALLY_ADDED_DEPENDENCIES)
    file(WRITE "${{CMAKE_BINARY_DIR}}/final.txt" "${{final}}")
endif()
''')
                build = root / "build"
                result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
                self.assertEqual([
                    "Runtime.tag=../../Runtime/html", "Reference.tag=../../Reference/html",
                    "Leaf.tag=../../Leaf/html",
                ], (build / "tags.txt").read_text().splitlines())
                self.assertEqual("Runtime-qmod", (build / "aot.txt").read_text())
                if docs_enabled:
                    self.assertEqual("docs-Runtime", (build / "docs.txt").read_text())
                    self.assertEqual({"docs-App", "docs-Reference", "docs-Runtime"},
                                     set((build / "final.txt").read_text().split(";")))
                    final = (build / "doxygen/Doxyfile.App.final").read_text()
                    self.assertFalse((build / "doxygen/Doxyfile.App").exists())
                    self.assertIn(f'@INCLUDE = "{build}/doxygen/Doxyfile.App"', final)
                    self.assertNotIn("Runtime.tag=", final)
                    self.assertIn("Reference.tag=../../Reference/html", final)
                    self.assertIn("Leaf.tag=../../Leaf/html", final)
                    self.assertIn("GENERATE_TAGFILE =\n", final)
                    result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    self.assertNotIn("Warning", result.stderr)
                else:
                    self.assertFalse((build / "docs.txt").exists())

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
