#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check QPP's persisted paths and real CMake flag forwarding across build roots."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
QPP = Path(os.environ.get("QORE_QPP_EXECUTABLE", ROOT / "build/qpp")).resolve()
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
PROBE = """//! Path mapping probe
/** Used to check constructor, method and static method reflection locations. */
qclass PrefixProbe [arg=void* priv; ns=Qore];

//! Construct a probe
/** Creates a probe. */
PrefixProbe::constructor() {
}

//! Get a value
/** Returns a value. */
int PrefixProbe::value() {
    return 42;
}

//! Get a static value
/** Returns a static value. */
static int PrefixProbe::staticValue() {
    return 43;
}

/** @defgroup prefix_functions Prefix functions
*/
///@{
//! Get a free function value
/** Returns a free function value. */
int prefixValue() {
    return 44;
}
///@}
"""


class QppPrefixMapsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not QPP.is_file():
            raise unittest.SkipTest("Build qpp or set QORE_QPP_EXECUTABLE to run the QPP integration tests")

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-qpp-prefix-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def invoke(self, *args, success=True):
        result = subprocess.run(list(map(str, args)), text=True, capture_output=True, timeout=60)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        else:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def generate(self, name, maps):
        directory = self.root / name
        directory.mkdir()
        source = directory / "Probe.qpp"
        source.write_text(PROBE)
        self.invoke(QPP, f"--output={directory}/Probe.cpp", f"--dox-output={directory}/Probe.dox.h",
                    f"--metadata={directory}/Probe.json", f"--stub-output={directory}/Probe.stub.qc",
                    *maps, source)
        return directory

    def test_persisted_paths_match_but_line_directives_keep_real_sources(self):
        outputs = []
        for name in ("first source", "second source"):
            directory = self.generate(name, [f"--file-prefix-map={self.root / name}=."])
            metadata = (directory / "Probe.json").read_bytes()
            self.assertEqual("./Probe.qpp", json.loads(metadata)["source_file"])
            cpp = (directory / "Probe.cpp").read_text()
            locations = re.findall(r'QoreBuiltinSrcLocHelper _qpp_src_loc_h\("([^"\n]+)", (\d+)\)', cpp)
            self.assertEqual(4, len(locations))
            self.assertTrue(all(path == "./Probe.qpp" for path, _ in locations))
            self.assertIn(f'"{directory}/Probe.qpp"', cpp, "#line must retain the real file for native debug maps")
            outputs.append((metadata, (directory / "Probe.stub.qc").read_bytes(), locations))
        self.assertEqual(outputs[0], outputs[1])

    def test_last_matching_map_and_unmapped_paths(self):
        directory = self.generate("original", ["--file-prefix-map=/not-this-source=/unused"])
        self.assertEqual(str(directory / "Probe.qpp"),
                         json.loads((directory / "Probe.json").read_text())["source_file"])
        directory = self.generate("mapped", [f"--file-prefix-map={self.root}=/first",
            f"--file-prefix-map={self.root / 'mapped'}=/second"])
        self.assertEqual("/second/Probe.qpp",
                         json.loads((directory / "Probe.json").read_text())["source_file"])
        directory = self.generate("source=checkout", [f"--file-prefix-map={self.root / 'source=checkout'}=/equal"])
        self.assertEqual("/equal/Probe.qpp",
                         json.loads((directory / "Probe.json").read_text())["source_file"])

    def test_malformed_maps_are_rejected(self):
        for mapping in ("missing-separator", "=empty-old"):
            result = self.invoke(QPP, f"--file-prefix-map={mapping}", success=False)
            self.assertIn("nonempty OLD prefix", result.stderr)

    def test_mapped_labels_are_escaped_for_each_output_format(self):
        prefix = '/mapped"\\\x01\n'
        directory = self.generate("escaped", [f"--file-prefix-map={self.root / 'escaped'}={prefix}"])
        self.assertEqual(prefix + "/Probe.qpp",
                         json.loads((directory / "Probe.json").read_text())["source_file"])
        cpp = (directory / "Probe.cpp").read_text()
        self.assertEqual(4, cpp.count(r'_qpp_src_loc_h("/mapped\"\\\001\012/Probe.qpp",'))
        self.assertIn(r'/mapped\"\\\u0001\n/Probe.qpp', (directory / "Probe.stub.qc").read_text().splitlines()[0])

    def test_cmake_forwards_quoted_and_configuration_flags(self):
        source = self.root / "source with spaces"
        build = self.root / "build with spaces"
        source.mkdir()
        (source / "Probe.qpp").write_text(PROBE)
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(QppPrefixMapFixture NONE)
set(QORE_QPP_EXECUTABLE "{QPP}")
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_FLAGS [=[-O2 "-ffile-prefix-map={source}=/base"]=])
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO [=[-g "-ffile-prefix-map={source}=/configuration"]=])
include("{ROOT}/cmake/QoreMacros.cmake")
qore_wrap_qpp_value(CPP METALIST META STUBLIST STUB Probe.qpp)
add_custom_target(generate ALL DEPENDS ${{CPP}} ${{META}} ${{STUB}})
''')
        self.invoke(CMAKE, "-S", source, "-B", build)
        self.invoke(CMAKE, "--build", build)
        self.assertEqual("/configuration/Probe.qpp",
                         json.loads((build / "Probe.meta.json").read_text())["source_file"])


if __name__ == "__main__":
    unittest.main()
