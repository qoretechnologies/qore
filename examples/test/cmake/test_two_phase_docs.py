#!/usr/bin/env python3
"""Exercise the exported documentation helper with CMAKE_EXECUTABLE (or cmake).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class TwoPhaseDocsTest(unittest.TestCase):
    def configure(self, root, enabled=True, missing_target=False, text="@literal@ ${literal}"):
        source = root / "source with spaces"
        build = root / "build~snapshot with spaces"
        source.mkdir(exist_ok=True)
        module_path = f"{source}/qlib:{build}/modules"
        (source / "check-env.py").write_text(f'''import os
from pathlib import Path
assert os.environ["QORE_MODULE_DIR"] == {module_path!r}
assert os.environ["QORE_DOC_DEFINES"] == "QORE_QDX_RUN,Unix"
Path({str(build / "qdx-environment-ok")!r}).touch()
''')
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(DocHelper NONE)
set(QORE_MODULE_DIR_FOR_DOCS "{module_path}")
set(QORE_DOC_DEFINES "QORE_QDX_RUN,Unix")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(DOXYGEN_FOUND {"TRUE" if enabled else "FALSE"})
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E echo)
set(QORE_QDX_COMMAND "{sys.executable}" "${{CMAKE_SOURCE_DIR}}/check-env.py")
add_custom_target(docs)
add_custom_target(docs-module)
add_custom_target(docs-First COMMAND "${{CMAKE_COMMAND}}" -E touch first-built)
{"" if missing_target else 'add_custom_target(docs-Second COMMAND "${CMAKE_COMMAND}" -E touch second-built)'}
file(WRITE "${{CMAKE_BINARY_DIR}}/Doxyfile" [=[{text}
]=])
file(MAKE_DIRECTORY "${{CMAKE_BINARY_DIR}}/doxygen")
file(WRITE "${{CMAKE_BINARY_DIR}}/doxygen/Doxyfile.First" "# First\\n")
file(WRITE "${{CMAKE_BINARY_DIR}}/doxygen/Doxyfile.Second" "# Second\\n")
qore_binary_module_two_phase_docs(xml "First;Second")
''')
        result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                text=True, capture_output=True, timeout=30)
        if not missing_target:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("Warning", result.stderr)
        return build, result

    def test_literal_content_dependencies_and_reconfigure(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            root = Path(directory)
            build, _ = self.configure(root)
            for literal in ("@literal@ ${literal}", "changed @untouched@ ${untouched}"):
                self.configure(root, text=literal)
                final = (build / "Doxyfile.final").read_text()
                self.assertTrue(final.startswith(literal + "\n"), final)
                self.assertEqual(0, final.count("WARN_IF_DOC_ERROR = NO"))
                self.assertEqual(1, final.count("WARN_IF_DOC_ERROR = YES"))
                self.assertIn("GENERATE_TAGFILE =\n", final)
                self.assertIn("WARN_IF_DOC_ERROR = NO", (build / "Doxyfile").read_text())
                for name, peer in (("First", "Second"), ("Second", "First")):
                    user_final = (build / f"doxygen/Doxyfile.{name}.final").read_text()
                    self.assertIn(f"{build}/{peer}.tag=../../{peer}/html", user_final)
                    self.assertIn(f"{build}/xml.tag=../../xml/html", user_final)
                    self.assertNotIn(f"{build}/{name}.tag=", user_final)
                for name in ("First", "Second"):
                    self.assertIn(f"{build}/{name}.tag=../../{name}/html", final)
                result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                        text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
                self.assertTrue((build / "first-built").is_file())
                self.assertTrue((build / "second-built").is_file())
                self.assertTrue((build / "qdx-environment-ok").is_file())

    def test_disabled_documentation(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            build, _ = self.configure(Path(directory), enabled=False)
            self.assertFalse((build / "Doxyfile.final").exists())
            self.assertNotIn("WARN_IF_DOC_ERROR", (build / "Doxyfile").read_text())

    def test_existing_indexes_keep_their_destination_without_duplicates(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-existing-tags-") as directory:
            root = Path(directory)
            build = root / "build~snapshot with spaces"
            first = f'"{build}/First.tag=../custom/First/html"'
            core = '"/sdk with spaces/qore.tag=../language/html"'
            self.configure(root, text=f"TAGFILES = {core} {first}")
            final = (build / "Doxyfile.final").read_text()
            self.assertIn(core, final)
            self.assertIn(first, final)
            self.assertEqual(1, final.count("/First.tag="))
            self.assertIn(f'"{build}/Second.tag=../../Second/html"', final)

    def test_binary_module_java_documentation_environment(self):
        with tempfile.TemporaryDirectory(prefix="qore-qjar-env-") as directory:
            source = Path(directory) / "source~snapshot"
            build = Path(directory) / "build~snapshot"
            (source / "cmake").mkdir(parents=True)
            (source / "cmake/cmake_uninstall.cmake.in").write_text("# fixture\n")
            (source / "fixture.c").write_text("void fixture(void) {}\n")
            (source / "Doxyfile.in").write_text("PROJECT_NAME = Fixture\n")
            (source / "check-env.py").write_text(f'''import os
from pathlib import Path
assert os.environ["QORE_MODULE_DIR"] == {f"{build}/modules/fixture:{source}/qlib"!r}
assert os.environ["QORE_DOC_DEFINES"] == "QORE_QDX_RUN,Unix"
Path({str(build / "qjar-environment-ok")!r}).touch()
''')
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(BinaryDocEnvironment C)
set(QORE_MODULE_DIR_FOR_DOCS "${{CMAKE_SOURCE_DIR}}/qlib")
set(QORE_DOC_DEFINES "QORE_QDX_RUN,Unix")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E true)
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
set(QORE_QJAR_COMMAND "{sys.executable}" "${{CMAKE_SOURCE_DIR}}/check-env.py")
set(QORE_USERMODULE_DOXYGEN_TEMPLATE "${{CMAKE_SOURCE_DIR}}/Doxyfile.in")
set(QORE_MODULES_DIR lib/qore-modules)
set(QORE_API_VERSION 2.0)
add_custom_target(docs)
add_custom_target(docs-lang)
add_library(fixture MODULE fixture.c)
qore_binary_module_intern2(fixture 1.0 "" "1")
''')
            for command in ([CMAKE, "-S", str(source), "-B", str(build)],
                            [CMAKE, "--build", str(build), "--target", "docs"]):
                result = subprocess.run(command, text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertTrue((build / "qjar-environment-ok").is_file())
            (build / "qjar-environment-ok").unlink()
            # HTML generation must remain usable when no Java/JNI output is requested.
            for command in ([CMAKE, "-S", str(source), "-B", str(build),
                             "-DQORE_GENERATE_JAVA_BINDINGS=OFF"],
                            [CMAKE, "--build", str(build), "--target", "docs"]):
                result = subprocess.run(command, text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
            self.assertFalse((build / "qjar-environment-ok").exists())

    def test_reciprocal_links_and_immutable_indexes(self):
        import shutil
        import xml.etree.ElementTree as ET
        doxygen = shutil.which("doxygen")
        if not doxygen:
            self.skipTest("Doxygen is required for the rendered-link integration test")
        with tempfile.TemporaryDirectory(prefix="qore-doc-links-") as directory:
            source = Path(directory) / "source with spaces"
            build = Path(directory) / "build with spaces"
            source.mkdir()
            names = ("native", "First", "Second")
            setup = []
            for name in names:
                refs = "\n".join(f'@ref {peer.lower()}intro "{peer}"' for peer in names if peer != name)
                (source / f"{name}.dox").write_text(
                    f'/** @mainpage {name}\n@section {name.lower()}intro {name}\n{refs}\n*/\n')
                config = build / ("Doxyfile" if name == "native" else f"doxygen/Doxyfile.{name}")
                target = "docs-module" if name == "native" else f"docs-{name}"
                setup.append(f'''file(WRITE "{config}" [=[
PROJECT_NAME = {name}
INPUT = "{source / (name + '.dox')}"
OUTPUT_DIRECTORY = "{build / 'docs' / name}"
GENERATE_TAGFILE = "{build / (name + '.tag')}"
GENERATE_LATEX = NO
QUIET = YES
WARN_AS_ERROR = FAIL_ON_WARNINGS
]=])
add_custom_target({target}
    COMMAND "${{CMAKE_COMMAND}}" -E make_directory "{build / 'docs' / name}"
    COMMAND "{doxygen}" "{config}" VERBATIM)
''')
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ReciprocalDocs NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "{doxygen}")
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
add_custom_target(docs)
file(MAKE_DIRECTORY "${{CMAKE_BINARY_DIR}}/doxygen")
{''.join(setup)}
qore_binary_module_two_phase_docs(native "First;Second")
''')
            for command in ([CMAKE, "-S", str(source), "-B", str(build)],
                            [CMAKE, "--build", str(build), "--target", "docs", "-j4"]):
                result = subprocess.run(command, text=True, capture_output=True, timeout=60)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("warning:", result.stderr.lower())
            for name in names:
                html = (build / f"docs/{name}/html/index.html").read_text()
                for peer in names:
                    if peer != name:
                        self.assertIn(f'../../{peer}/html/index.html#{peer.lower()}intro', html)
                tag = build / f"{name}.tag"
                ET.parse(tag)
                timestamp = tag.stat().st_mtime_ns
                config = build / ("Doxyfile.final" if name == "native" else f"doxygen/Doxyfile.{name}.final")
                result = subprocess.run([doxygen, str(config)], text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual(timestamp, tag.stat().st_mtime_ns)

    def test_missing_dependency_is_an_error(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            _, result = self.configure(Path(directory), missing_target=True)
            self.assertNotEqual(0, result.returncode)
            self.assertIn('"docs-Second"', result.stderr)
            self.assertIn("does not exist", " ".join(result.stderr.split()))

    def test_bundled_modules_use_all_available_indexes(self):
        with tempfile.TemporaryDirectory(prefix="qore bundled docs ") as directory:
            root = Path(directory)
            source, build = root / "source", root / "build"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(BundledDocs NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E echo)
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
add_custom_target(docs)
foreach(name lang lib native peer User)
    add_custom_target(docs-${{name}} COMMAND "${{CMAKE_COMMAND}}" -E touch "${{name}}-built")
endforeach()
foreach(name native peer)
    file(MAKE_DIRECTORY "${{CMAKE_BINARY_DIR}}/modules/${{name}}")
    file(WRITE "${{CMAKE_BINARY_DIR}}/modules/${{name}}/Doxyfile" "# ${{name}}\\n")
endforeach()
qore_bundled_module_two_phase_docs("native;peer;disabled" "User")
''')
            for command in ([CMAKE, "-S", str(source), "-B", str(build)],
                            [CMAKE, "--build", str(build), "--target", "docs", "-j4"]):
                result = subprocess.run(command, text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
            for name, peer in (("native", "peer"), ("peer", "native")):
                config = (build / f"modules/{name}/Doxyfile.final").read_text()
                self.assertIn(f'"{build}/User.tag=../../User/html"', config)
                self.assertIn(f'"{build}/modules/{peer}/{peer}.tag=../../{peer}/html"', config)
                self.assertNotIn(f"{name}.tag=", config)
                self.assertNotIn("disabled.tag", config)
                self.assertIn("GENERATE_TAGFILE =\n", config)
            for name in ("lang", "lib", "native", "peer", "User"):
                self.assertTrue((build / f"{name}-built").exists())


if __name__ == "__main__":
    unittest.main()
