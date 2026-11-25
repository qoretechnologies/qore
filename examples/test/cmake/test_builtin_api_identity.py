#!/usr/bin/env python3
"""Verify libqore's builtin API identity (cmake/QoreWriteBuiltinApiIdentity.cmake, qore#7183): the digest of the API
metadata of its qpp files, which qcc records in every AOT module, changes only with that metadata, and every qlib AOT
module is compiled again when it changes and none when it does not, with the Makefile generator and with Ninja when
it is installed.

A qmod can have the metadata of the builtin classes, functions and constants that it uses built into its code (the
functional domains that a class inherits from a builtin base class, for example), so the module loader takes a qmod
compiled against other builtin API metadata for out of date.  The identity is only a digest of the API metadata (what
qpp writes with --api-metadata, without documentation and source paths), so a change to the documentation or code of
libqore does not compile the modules again; each qmod depends on the identity header, which is rewritten only when the
identity changes.

The project uses the qlib AOT module rules of the Qore build (qore_user_module()) and its identity script, with
stand-ins for qpp's API metadata files, for libqore (it records the identity in the generated header) and for qcc (it
writes that identity into the module).  Instead of waiting, a test moves every file of the test back in time before it
changes a source, which leaves their order unchanged: make 3.81 compares modification times in whole seconds.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
SCRIPT = ROOT / "cmake" / "QoreWriteBuiltinApiIdentity.cmake"
IDENTITY_RE = re.compile(r'QORE_BUILTIN_API_IDENTITY "([0-9a-f]{64})"')

# the libqore stand-in: compiles the identity of the generated header in, as lib/qore_aot_runtime_identity.cpp does
FAKE_LIBQORE = r'''import re
import sys
from pathlib import Path
header, output = map(Path, sys.argv[1:])
output.write_text(re.search(r'QORE_BUILTIN_API_IDENTITY "([0-9a-f]+)"', header.read_text()).group(1) + "\n")
'''

# the qcc stand-in: writes the identity of the libqore stand-in into the -o output and a depfile naming the source
FAKE_QCC = r'''import sys
from pathlib import Path
args = sys.argv[1:]
output = Path(args[args.index("-o") + 1])
depfile = Path(next(arg for arg in args if arg.startswith("--depfile=")).split("=", 1)[1])
source = Path(args[args.index("-m") + 1])
here = Path(__file__).parent
with open(here / "compiles.log", "a") as log:
    log.write(output.name + "\n")
output.write_text((here.parent / "build" / "libqore-identity.txt").read_text())
depfile.write_text(f"{output}: {source}\n")
'''

MODULES = ("Alpha", "Beta", "Gamma")
API_FILES = ("QC_One.api.json", "QC_Two.api.json")


def age(root, seconds):
    """Moves the modification time of every file below @p root back by @p seconds"""
    for path in [Path(root), *Path(root).rglob("*")]:
        info = path.lstat()
        os.utime(path, ns=(info.st_atime_ns - seconds * 1_000_000_000, info.st_mtime_ns - seconds * 1_000_000_000),
                 follow_symlinks=False)


class ScriptTest(unittest.TestCase):
    """The identity script itself"""

    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        directory = tempfile.TemporaryDirectory(prefix="qore-builtin-api-identity-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def write_files(self, subdir, contents):
        """Writes the API metadata files of @p contents (a dict of name: text) to @p subdir; returns their paths"""
        directory = self.root / subdir
        directory.mkdir(parents=True, exist_ok=True)
        paths = []
        for name, text in contents.items():
            path = directory / name
            path.write_text(text)
            paths.append(path)
        return paths

    def run_script(self, paths, header, expect_ok=True):
        file_list = header.parent / (header.name + ".files.txt")
        file_list.parent.mkdir(parents=True, exist_ok=True)
        file_list.write_text("".join(f"{p}\n" for p in paths))
        result = subprocess.run([CMAKE, f"-DINPUT_LIST={file_list}", f"-DOUTPUT={header}", "-P", str(SCRIPT)],
                                capture_output=True, text=True, timeout=60)
        if expect_ok:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def identity(self, paths, name="h"):
        header = self.root / name / "qore_builtin_api_identity.h"
        self.run_script(paths, header)
        m = IDENTITY_RE.search(header.read_text())
        self.assertIsNotNone(m, header.read_text())
        return m.group(1)

    def test_deterministic(self):
        files = {"QC_A.api.json": '{"classes":[{"name":"A","domains":["NETWORK"]}]}\n',
                 "QC_B.api.json": '{"classes":[{"name":"B","domains":[]}]}\n'}
        first = self.write_files("one", files)
        # the same files in another directory, in another order
        second = list(reversed(self.write_files("two/deeper", files)))
        self.assertEqual(self.identity(first, "h1"), self.identity(second, "h2"))

    def test_changes_with_content(self):
        base = self.identity(self.write_files("one", {"QC_A.api.json": '{"domains":["NETWORK"]}\n',
                                                      "QC_B.api.json": '{"domains":[]}\n'}), "h1")
        changed = self.identity(self.write_files("two", {"QC_A.api.json": '{"domains":["NETWORK","FILESYSTEM"]}\n',
                                                         "QC_B.api.json": '{"domains":[]}\n'}), "h2")
        self.assertNotEqual(base, changed)
        # one file more
        more = self.identity(self.write_files("three", {"QC_A.api.json": '{"domains":["NETWORK"]}\n',
                                                        "QC_B.api.json": '{"domains":[]}\n',
                                                        "QC_C.api.json": '{}\n'}), "h3")
        self.assertNotEqual(base, more)
        # the same content under another file name
        renamed = self.identity(self.write_files("four", {"QC_A.api.json": '{"domains":["NETWORK"]}\n',
                                                          "QC_X.api.json": '{"domains":[]}\n'}), "h4")
        self.assertNotEqual(base, renamed)

    def test_header_rewritten_only_on_change(self):
        paths = self.write_files("one", {"QC_A.api.json": '{"domains":["NETWORK"]}\n'})
        header = self.root / "h" / "qore_builtin_api_identity.h"
        self.run_script(paths, header)
        content = header.read_text()
        age(self.root, 10)
        mtime = header.stat().st_mtime_ns
        self.run_script(paths, header)
        self.assertEqual(mtime, header.stat().st_mtime_ns, "an unchanged identity leaves the header as it was")
        self.assertEqual(content, header.read_text())
        paths[0].write_text('{"domains":[]}\n')
        self.run_script(paths, header)
        self.assertNotEqual(mtime, header.stat().st_mtime_ns, "a new identity rewrites the header")
        self.assertNotEqual(content, header.read_text())

    def test_errors(self):
        header = self.root / "h" / "qore_builtin_api_identity.h"
        result = self.run_script([self.root / "missing.api.json"], header, expect_ok=False)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("API metadata file", result.stdout + result.stderr)
        result = self.run_script([], header, expect_ok=False)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("names no file", result.stdout + result.stderr)
        self.assertFalse(header.exists(), "no header is written")


class RebuildTest(unittest.TestCase):
    """The qmods of a build are compiled again exactly when the identity changes"""

    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        directory = tempfile.TemporaryDirectory(prefix="qore-builtin-api-rebuild-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source, self.build = self.root / "source", self.root / "build"
        (self.source / "qlib").mkdir(parents=True)
        (self.source / "api").mkdir()
        for name in MODULES:
            (self.source / "qlib" / f"{name}.qm").write_text("%modern\n")
        for name in API_FILES:
            (self.source / "api" / name).write_text('{"classes":[{"name":"%s","domains":[]}]}\n' % name)
        (self.source / "runtime.cpp").write_text("// a libqore source that is not API metadata\n")
        (self.source / "codegen.cpp").write_text("// a source of the AOT runtime identity\n")
        (self.source / "libqore.py").write_text(FAKE_LIBQORE)
        (self.source / "qcc.py").write_text(FAKE_QCC)
        qcc = self.source / "qcc"
        qcc.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{self.source / "qcc.py"}" "$@"\n')
        qcc.chmod(0o755)
        modules = "\n".join(f'qore_user_module("qlib/{name}.qm")' for name in MODULES)
        api_names = " ".join(API_FILES)
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14)
project(BuiltinApiIdentityRebuild NONE)
include("{ROOT.as_posix()}/cmake/QoreMacrosIntern.cmake")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_QCC_EXECUTABLE "{qcc.as_posix()}")
# the stand-in qcc does not check the qmods against their dependencies (qore#6598)
set(QORE_QMOD_SOURCE_CHECK OFF)
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
# the AOT runtime identity rules of the Qore build, which a qmod rule of the Qore build uses with these
set(QCC_FORMAT_STAMP "${{CMAKE_BINARY_DIR}}/qcc-format.stamp")
set(_manifest "${{CMAKE_BINARY_DIR}}/qcc-format-sources.txt")
set(_labels "${{CMAKE_BINARY_DIR}}/qcc-format-source-labels.txt")
qore_write_generated_source("${{_manifest}}" "${{CMAKE_SOURCE_DIR}}/codegen.cpp\\n")
qore_write_generated_source("${{_labels}}" "codegen.cpp\\n")
set(QORE_AOT_RUNTIME_IDENTITY_HEADER "${{CMAKE_BINARY_DIR}}/include/qore_aot_runtime_identity.h")
qore_aot_runtime_identity_targets(STAMP "${{QCC_FORMAT_STAMP}}" INPUT_LIST "${{_manifest}}" INPUT_LABELS "${{_labels}}"
    HEADER "${{QORE_AOT_RUNTIME_IDENTITY_HEADER}}")
# the builtin API identity rules of the Qore build (CMakeLists.txt): the API metadata files are written by the
# commands of QPP_GENERATED_FILES (here copies of the files of the source, as qpp writes them from its .qpp files)
set(QPP_API_SRC)
foreach(_api {api_names})
    add_custom_command(OUTPUT "${{CMAKE_BINARY_DIR}}/${{_api}}"
        COMMAND ${{CMAKE_COMMAND}} -E copy "${{CMAKE_SOURCE_DIR}}/api/${{_api}}" "${{CMAKE_BINARY_DIR}}/${{_api}}"
        MAIN_DEPENDENCY "${{CMAKE_SOURCE_DIR}}/api/${{_api}}"
        VERBATIM)
    list(APPEND QPP_API_SRC "${{CMAKE_BINARY_DIR}}/${{_api}}")
endforeach()
add_custom_target(QPP_GENERATED_FILES DEPENDS ${{QPP_API_SRC}})
set(QORE_BUILTIN_API_IDENTITY_HEADER "${{CMAKE_BINARY_DIR}}/include/qore_builtin_api_identity.h")
set(_files "${{CMAKE_BINARY_DIR}}/qore-builtin-api-files.txt")
string(REPLACE ";" "\\n" _files_content "${{QPP_API_SRC}}")
file(WRITE "${{_files}}" "${{_files_content}}\\n")
add_custom_target(qore-builtin-api-identity
    COMMAND ${{CMAKE_COMMAND}} -DINPUT_LIST=${{_files}} -DOUTPUT=${{QORE_BUILTIN_API_IDENTITY_HEADER}}
        -P "{SCRIPT.as_posix()}"
    BYPRODUCTS ${{QORE_BUILTIN_API_IDENTITY_HEADER}}
    VERBATIM)
add_dependencies(qore-builtin-api-identity QPP_GENERATED_FILES)
# libqore depends on the header in its own rule, as the object of lib/qore_aot_runtime_identity.cpp does
add_custom_command(OUTPUT "${{CMAKE_BINARY_DIR}}/libqore-identity.txt"
    COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_SOURCE_DIR}}/libqore.py"
        "${{QORE_BUILTIN_API_IDENTITY_HEADER}}" "${{CMAKE_BINARY_DIR}}/libqore-identity.txt"
    DEPENDS "${{QORE_BUILTIN_API_IDENTITY_HEADER}}" "${{CMAKE_SOURCE_DIR}}/runtime.cpp"
    VERBATIM)
add_custom_target(libqore ALL DEPENDS "${{CMAKE_BINARY_DIR}}/libqore-identity.txt")
add_dependencies(libqore qore-builtin-api-identity qore-aot-runtime-identity)
# qcc is the stand-in script, so each module depends on libqore as the real ones depend on the qcc target
set(QORE_AOT_BINARY_MODULE_TARGETS libqore)
{modules}
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
''')

    def generators(self):
        generators = []
        if shutil.which("make") or shutil.which("gmake"):
            generators.append("Unix Makefiles")
        if shutil.which("ninja"):
            generators.append("Ninja")
        if not generators:
            self.skipTest("make or ninja is required")
        return generators

    def run_cmake(self, *args):
        result = subprocess.run([CMAKE, *map(str, args)], capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertNotRegex(result.stdout + result.stderr, r"(?i)\bwarning\b")
        return result.stdout + result.stderr

    def compiles(self):
        log = self.source / "compiles.log"
        return log.read_text().splitlines() if log.exists() else []

    def build_all(self):
        """Builds and returns the sorted names of the modules that the build compiled"""
        before = len(self.compiles())
        self.run_cmake("--build", self.build)
        return sorted(self.compiles()[before:])

    def assert_current(self):
        """Every module is compiled for the identity that libqore has"""
        header = (self.build / "include" / "qore_builtin_api_identity.h").read_text()
        identity = IDENTITY_RE.search(header).group(1)
        self.assertEqual(identity, (self.build / "libqore-identity.txt").read_text().strip())
        for name in MODULES:
            self.assertEqual(identity, (self.build / "qlib-qmod" / f"{name}.qmod").read_text().strip(), name)
        return identity

    def edit(self, path, text):
        """Changes a file after time has passed since the last build"""
        age(self.root, 5)
        path.write_text(text)

    def run_generator(self, generator):
        all_modules = sorted(f"{name}.qmod" for name in MODULES)
        self.run_cmake("-G", generator, "-S", self.source, "-B", self.build)
        self.assertEqual(all_modules, self.build_all())
        first = self.assert_current()
        # nothing changed: nothing is compiled
        self.assertEqual([], self.build_all())

        # a libqore source that is not API metadata: libqore is built again, no module is compiled
        self.edit(self.source / "runtime.cpp", "// changed\n")
        self.assertEqual([], self.build_all())
        self.assertEqual(first, self.assert_current())

        # an API metadata file rewritten with the same content: the identity does not change
        api = self.source / "api" / API_FILES[0]
        self.edit(api, api.read_text())
        self.assertEqual([], self.build_all())
        self.assertEqual(first, self.assert_current())

        # the API metadata changes (a builtin class has a new functional domain): every module is compiled again
        self.edit(api, api.read_text().replace('"domains":[]', '"domains":["NETWORK"]'))
        self.assertEqual(all_modules, self.build_all())
        second = self.assert_current()
        self.assertNotEqual(first, second)
        self.assertEqual([], self.build_all())

    def test_generators(self):
        for generator in self.generators():
            with self.subTest(generator=generator):
                if self.build.exists():
                    shutil.rmtree(self.build)
                log = self.source / "compiles.log"
                if log.exists():
                    log.unlink()
                for name in API_FILES:
                    (self.source / "api" / name).write_text('{"classes":[{"name":"%s","domains":[]}]}\n' % name)
                self.run_generator(generator)


if __name__ == "__main__":
    unittest.main()
