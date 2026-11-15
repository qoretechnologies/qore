#!/usr/bin/env python3
"""Verify that every qlib AOT module is compiled again when the AOT runtime identity changes, and nothing is when it
does not, with CMAKE_EXECUTABLE (or cmake), with the Makefile generator and with Ninja when it is installed.

A qmod records the AOT runtime identity of the libqore whose qcc compiled it, and libqore refuses a qmod of another
identity.  The identity is derived from the qcc format fingerprint (qcc-format.stamp), and every qmod depends on that
fingerprint.  When the fingerprint was the output of a custom command, the Makefile generators gave each target of
the directory that depends on it (every qmod, libqore) its own copy of the rule (policy CMP0113 is OLD below
cmake_minimum_required(VERSION 3.19), and the Qore build requires 3.14, as the project below does), so a qmod target
could write the fingerprint again, after libqore had been compiled: a fingerprinted source edited while a build was
running made the qmods of that build newer than the fingerprint for the new identity, although qcc had compiled them
for the previous one, and the next build relinked libqore with the new identity and compiled none of them.  And as
the rule touched the fingerprint whenever it ran, a fingerprinted source touched without a change compiled every
module again.  Each module also depends on the identity header, so a build tree that the previous rules left so has
every module compiled again by its next build (qore#6221).

The project uses the identity rules of the Qore build (qore_aot_runtime_identity_targets() in
cmake/QoreMacrosIntern.cmake) and its qlib AOT module rules (qore_user_module()), with stand-ins for libqore (it
records the identity in the generated header) and for qcc (it writes that identity into the module).  Instead of
waiting, a test moves every file of the test back in time before it changes a source, which leaves their order
unchanged: make 3.81 compares modification times in whole seconds.

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

# moves the modification time of every file below a directory back by a number of seconds
AGE = r'''import os
from pathlib import Path
def age(root, seconds):
    for path in [Path(root), *Path(root).rglob("*")]:
        info = path.lstat()
        os.utime(path, ns=(info.st_atime_ns - seconds * 1_000_000_000, info.st_mtime_ns - seconds * 1_000_000_000),
                 follow_symlinks=False)
'''

# the libqore stand-in: compiles the identity of the generated header in, as lib/qore_aot_runtime_identity.cpp does;
# when the file "edit-during-build" exists, the fingerprinted codegen.cpp is changed while it "compiles", after the
# fingerprint was checked by this build
FAKE_LIBQORE = AGE + r'''import re
import sys
header, output, root = map(Path, sys.argv[1:])
trigger = root / "source" / "edit-during-build"
if trigger.exists():
    trigger.unlink()
    age(root, 10)
    codegen = root / "source" / "codegen.cpp"
    codegen.write_text(codegen.read_text() + "// changed while libqore compiles\n")
identity = re.search(r'QORE_AOT_RUNTIME_IDENTITY "([0-9a-f]+)"', header.read_text()).group(1)
output.write_text(identity + "\n")
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


class AotRuntimeIdentityRebuildTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        directory = tempfile.TemporaryDirectory(prefix="qore-aot-identity-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source, self.build = self.root / "source", self.root / "build"
        (self.source / "qlib").mkdir(parents=True)
        for name in MODULES:
            (self.source / "qlib" / f"{name}.qm").write_text("%modern\n")
        (self.source / "codegen.cpp").write_text("// a source that can change what qcc emits\n")
        (self.source / "runtime.cpp").write_text("// a source that cannot\n")
        (self.source / "age.py").write_text(AGE)
        (self.source / "libqore.py").write_text(FAKE_LIBQORE)
        (self.source / "qcc.py").write_text(FAKE_QCC)
        qcc = self.source / "qcc"
        qcc.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{self.source / "qcc.py"}" "$@"\n')
        qcc.chmod(0o755)
        modules = "\n".join(f'qore_user_module("qlib/{name}.qm")' for name in MODULES)
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14)
project(AotRuntimeIdentityRebuild NONE)
include("{ROOT.as_posix()}/cmake/QoreMacrosIntern.cmake")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_QCC_EXECUTABLE "{qcc.as_posix()}")
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
set(QCC_FORMAT_STAMP "${{CMAKE_BINARY_DIR}}/qcc-format.stamp")
set(_manifest "${{CMAKE_BINARY_DIR}}/qcc-format-sources.txt")
set(_labels "${{CMAKE_BINARY_DIR}}/qcc-format-source-labels.txt")
qore_write_generated_source("${{_manifest}}" "${{CMAKE_SOURCE_DIR}}/codegen.cpp\\n")
qore_write_generated_source("${{_labels}}" "codegen.cpp\\n")
set(QORE_AOT_RUNTIME_IDENTITY_HEADER "${{CMAKE_BINARY_DIR}}/include/qore_aot_runtime_identity.h")
qore_aot_runtime_identity_targets(STAMP "${{QCC_FORMAT_STAMP}}" INPUT_LIST "${{_manifest}}" INPUT_LABELS "${{_labels}}"
    HEADER "${{QORE_AOT_RUNTIME_IDENTITY_HEADER}}")
# libqore depends on the header in its own rule, as the object of lib/qore_aot_runtime_identity.cpp does
add_custom_command(OUTPUT "${{CMAKE_BINARY_DIR}}/libqore-identity.txt"
    COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_SOURCE_DIR}}/libqore.py"
        "${{QORE_AOT_RUNTIME_IDENTITY_HEADER}}" "${{CMAKE_BINARY_DIR}}/libqore-identity.txt" "{self.root.as_posix()}"
    DEPENDS "${{QORE_AOT_RUNTIME_IDENTITY_HEADER}}" "${{CMAKE_SOURCE_DIR}}/codegen.cpp" "${{CMAKE_SOURCE_DIR}}/runtime.cpp"
    VERBATIM)
add_custom_target(libqore ALL DEPENDS "${{CMAKE_BINARY_DIR}}/libqore-identity.txt")
add_dependencies(libqore qore-aot-runtime-identity)
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

    def configure(self, generator):
        return self.run_cmake("-G", generator, "-S", self.source, "-B", self.build)

    def compiles(self):
        log = self.source / "compiles.log"
        return log.read_text().splitlines() if log.exists() else []

    def build_all(self, *args):
        """Builds and returns the sorted names of the modules that the build compiled"""
        before = len(self.compiles())
        self.run_cmake("--build", self.build, *args)
        return sorted(self.compiles()[before:])

    def age(self, seconds):
        namespace = {}
        exec(AGE, namespace)
        namespace["age"](self.root, seconds)

    def header_identity(self):
        header = (self.build / "include" / "qore_aot_runtime_identity.h").read_text()
        return re.search(r'QORE_AOT_RUNTIME_IDENTITY "([0-9a-f]+)"', header).group(1)

    def assert_current(self):
        """Every module is compiled for the identity that libqore has, which is the one of the current sources"""
        identity = self.header_identity()
        self.assertEqual(identity, (self.build / "libqore-identity.txt").read_text().strip())
        for name in MODULES:
            self.assertEqual(identity, (self.build / "qlib-qmod" / f"{name}.qmod").read_text().strip(), name)
        return identity

    def edit(self, name, text):
        """Changes a source after time has passed since the last build"""
        self.age(5)
        path = self.source / name
        path.write_text(path.read_text() + text)

    def run_generator(self, generator):
        all_modules = sorted(f"{name}.qmod" for name in MODULES)
        self.configure(generator)
        self.assertEqual(all_modules, self.build_all())
        first = self.assert_current()
        # nothing changed: nothing is compiled, also not after a configure that changes nothing
        self.assertEqual([], self.build_all())
        self.configure(generator)
        self.assertEqual([], self.build_all())
        stamp = self.build / "qcc-format.stamp"
        stamp_mtime = stamp.stat().st_mtime_ns

        # a fingerprinted source whose content does not change: the fingerprint is not rewritten
        self.age(5)
        os.utime(self.source / "codegen.cpp", None)
        self.assertEqual([], self.build_all())
        self.assertEqual(stamp_mtime - 5 * 1_000_000_000, stamp.stat().st_mtime_ns)
        # a source that is not fingerprinted: libqore is rebuilt, no module
        self.edit("runtime.cpp", "// runtime-only change\n")
        self.assertEqual([], self.build_all())
        self.assertEqual(first, self.assert_current())

        # a fingerprinted source changes between builds: every module is compiled for the new identity
        self.edit("codegen.cpp", "// changed between builds\n")
        self.assertEqual(all_modules, self.build_all())
        second = self.assert_current()
        self.assertNotEqual(first, second)
        self.assertEqual([], self.build_all())

        # a fingerprinted source changes while a build is running, after the fingerprint was checked: that build
        # compiles nothing (its modules are current for the libqore it built), the next one compiles every module
        # for the new identity; a runtime-only change makes the build compile libqore
        self.edit("runtime.cpp", "// another runtime-only change\n")
        (self.source / "edit-during-build").write_text("")
        self.assertEqual([], self.build_all())
        self.assertFalse((self.source / "edit-during-build").exists())
        self.assertEqual(second, self.assert_current())
        self.assertEqual(all_modules, self.build_all())
        third = self.assert_current()
        self.assertNotEqual(second, third)
        self.assertEqual([], self.build_all())

        # the same while every module is compiled by the same build
        self.edit("codegen.cpp", "// changed between builds again\n")
        (self.source / "edit-during-build").write_text("")
        self.assertEqual(all_modules, self.build_all())
        fourth = self.assert_current()
        self.assertNotEqual(third, fourth)
        self.assertEqual(all_modules, self.build_all())
        self.assertNotEqual(fourth, self.assert_current())
        self.assertEqual([], self.build_all())

        # a single module target checks the fingerprint too
        self.edit("codegen.cpp", "// changed before a module target is built\n")
        self.assertEqual(["Beta.qmod"], self.build_all("--target", "Beta-qmod"))
        self.assertEqual(self.header_identity(), (self.build / "qlib-qmod" / "Beta.qmod").read_text().strip())
        self.assertEqual(["Alpha.qmod", "Gamma.qmod"], self.build_all())
        self.assert_current()

        # a build tree that the previous rules left with every module compiled for the previous identity after the
        # fingerprint had been rewritten for the new one, and the identity header rewritten after that (qore#6221):
        # the modules are newer than the fingerprint but older than the header, and every one is compiled again.  Only
        # the Makefile generators gave each target a copy of the rules, and Ninja decides from the modification times
        # in its own log, not from those set here
        if generator == "Ninja":
            return
        previous = self.assert_current()
        self.edit("codegen.cpp", "// changed during a build of the previous rules\n")
        stamp, header = self.build / "qcc-format.stamp", self.build / "include" / "qore_aot_runtime_identity.h"
        self.run_cmake(f"-DINPUT_LIST={self.build / 'qcc-format-sources.txt'}",
                       f"-DINPUT_LABELS={self.build / 'qcc-format-source-labels.txt'}", f"-DOUTPUT={stamp}",
                       "-P", ROOT / "cmake" / "QoreWriteContentDigest.cmake")
        self.run_cmake(f"-DINPUT={stamp}", f"-DOUTPUT={header}", "-P",
                       ROOT / "cmake" / "QoreWriteAOTRuntimeIdentity.cmake")
        self.assertNotEqual(previous, self.header_identity())
        now = stamp.stat().st_mtime_ns
        for offset, path in [(-3, stamp), (-1, header)] + [(-2, self.build / "qlib-qmod" / f"{name}.qmod")
                                                           for name in MODULES]:
            os.utime(path, ns=(now + offset * 1_000_000_000, now + offset * 1_000_000_000))
        self.assertEqual(all_modules, self.build_all())
        self.assertNotEqual(previous, self.assert_current())
        self.assertEqual([], self.build_all())

    def test_identity_change_rebuilds_every_module(self):
        for generator in self.generators():
            with self.subTest(generator=generator):
                if self.build.exists():
                    shutil.rmtree(self.build)
                log = self.source / "compiles.log"
                if log.exists():
                    log.unlink()
                self.run_generator(generator)


if __name__ == "__main__":
    unittest.main()
