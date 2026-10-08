#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Verify location-independent fingerprints still detect real compiler input changes."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class CompilerFingerprintTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-compiler-fingerprint-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_cmake(self, *args, success=True):
        result = subprocess.run([CMAKE, *map(str, args)], text=True, capture_output=True, timeout=60)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotRegex(result.stdout + result.stderr, r"(?im)\bwarning\b")
        else:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def fixture(self, name, flags="-O2"):
        source = self.root / name / "source [tree]+"
        build = source / "build with spaces"
        build.mkdir(parents=True)
        (source / "compiler.cpp").write_text("compiler input\n")
        config = build / "flags.cmake"
        config.write_text(f'''set(CMAKE_SOURCE_DIR "{source}")
set(CMAKE_BINARY_DIR "{build}")
include("{ROOT}/cmake/QoreRecordedBuildFlags.cmake")
set(original [=[{flags} -I{source}/include -I{build}/include -ffile-prefix-map={source}=.]=])
qore_recorded_build_flags(recorded "${{original}}")
file(WRITE "{build}/config.txt" "${{recorded}}")
''')
        self.run_cmake("-P", config)
        (build / "inputs.txt").write_text(f"{source}/compiler.cpp\n{build}/config.txt\n")
        (build / "labels.txt").write_text("compiler.cpp\n@build-config\n")
        return source, build

    def digest(self, build, labels=True, success=True):
        self.run_cmake(f"-DINPUT_LIST={build}/inputs.txt", f"-DOUTPUT={build}/digest",
                       f"-DSUCCESS_STAMP={build}/success",
                       *([f"-DINPUT_LABELS={build}/labels.txt"] if labels else []),
                       "-P", ROOT / "cmake/QoreWriteContentDigest.cmake", success=success)
        return (build / "digest").read_text() if success else None

    def test_relocated_trees_match_and_real_changes_invalidate(self):
        source, first = self.fixture("first")
        _, second = self.fixture("second")
        original = self.digest(first)
        self.assertEqual(original, self.digest(second))
        self.assertNotIn(str(self.root), original)
        self.assertEqual("-O2 -I<SOURCE>/include -I<BINARY>/include -ffile-prefix-map=<SOURCE>=.",
                         (first / "config.txt").read_text())
        (source / "compiler.cpp").write_text("changed compiler input\n")
        self.assertNotEqual(original, self.digest(first))
        _, changed_flags = self.fixture("optimization", "-O3")
        self.assertNotEqual(original, self.digest(changed_flags))
        (second / "labels.txt").write_text("renamed-compiler.cpp\n@build-config\n")
        self.assertNotEqual(original, self.digest(second))
        (source / "compiler.cpp").unlink()
        self.assertIn("compiler.cpp\tMISSING", self.digest(first))

    def test_default_digests_keep_path_identity(self):
        source, build = self.fixture("local")
        self.assertIn(str(source / "compiler.cpp") + "\t", self.digest(build, labels=False))

    def test_invalid_labels_fail(self):
        _, build = self.fixture("invalid")
        for labels in ("only-one\n", "duplicate\nduplicate\n", "\n@build-config\n"):
            (build / "labels.txt").write_text(labels)
            self.digest(build, success=False)

    def test_blank_input_lines_and_unchanged_digest_preserve_output(self):
        source, build = self.fixture("blank lines")
        expected = self.digest(build)
        (build / "inputs.txt").write_text(f"\n{source}/compiler.cpp\n\n{build}/config.txt\n\n")
        os.utime(build / "digest", ns=(1_000_000_000, 1_000_000_000))
        os.utime(build / "success", ns=(1_000_000_000, 1_000_000_000))
        self.assertEqual(expected, self.digest(build))
        self.assertEqual(1_000_000_000, (build / "digest").stat().st_mtime_ns)
        self.assertGreater((build / "success").stat().st_mtime_ns, 1_000_000_000)

    def test_prefix_siblings_are_not_rewritten(self):
        source, build = self.fixture("boundaries")
        script = build / "boundary.cmake"
        script.write_text(f'''set(CMAKE_SOURCE_DIR "{source}")
set(CMAKE_BINARY_DIR "{build}")
include("{ROOT}/cmake/QoreRecordedBuildFlags.cmake")
set(flags [=[-I{source}-extra/include -DROOT="{source}" -fdebug-prefix-map={source}=/usr/src/qore]=])
qore_recorded_build_flags(recorded "${{flags}}")
file(WRITE "{build}/boundary.txt" "${{recorded}}")
''')
        self.run_cmake("-P", script)
        self.assertEqual(f'-I{source}-extra/include -DROOT="<SOURCE>" -fdebug-prefix-map=<SOURCE>=/usr/src/qore',
                         (build / "boundary.txt").read_text())


if __name__ == "__main__":
    unittest.main()
