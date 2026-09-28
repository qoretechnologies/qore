#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise final AOT artifacts across physical roots with real compiler/runtime tools."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
QCC = Path(os.environ.get("QORE_QCC_EXECUTABLE", ROOT / "build/qcc")).resolve()
QORE = Path(os.environ.get("QORE_BINARY", ROOT / "build/qore")).resolve()
HEADER = '''%modern
module ReproProbe {
    version = "1.0";
    desc = "Reproducible deployment artifact test";
    author = "Qore";
    license = "MIT";
}
'''
FUNCTIONS = '''
int sub repro_leaf() {
    throw "MAPPED-LOCATION", "leaf";
}
public string sub repro_location() {
    try {
        repro_leaf();
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s:%d", ex.file, ex.line);
    }
    return "missing";
}
public string sub repro_literal() {
    return "/literal/source/keep";
}
public int sub repro_value(int value) {
    return value + 1;
}
'''


class AotPrefixMapsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not QCC.is_file() or not QORE.is_file():
            raise unittest.SkipTest("Build qcc/qore or set QORE_QCC_EXECUTABLE and QORE_BINARY")

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-aot-prefix-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.env = {k: v for k, v in os.environ.items() if k not in
                    ("QORE_MODULE_DIR", "QORE_INCLUDE_DIR", "QORE_INCLUDE_DIRS", "QORE_LIBDIR")}
        self.env["QORE_MODULE_DIR_ONLY"] = "1"
        if (QCC.parent / "libqore.so").exists():
            self.env["LD_LIBRARY_PATH"] = str(QCC.parent)
            self.env["QORE_LIBDIR"] = str(QCC.parent)

    def run_tool(self, *args, success=True, **kwargs):
        result = subprocess.run(list(map(str, args)), env=self.env, text=True,
                                capture_output=True, timeout=120, **kwargs)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        else:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def compile_module(self, name, split=False, extra=(), mapped_prefix="/usr/src/qore-test"):
        source = self.root / (name + " source")
        if split:
            source /= "ReproProbe"
        source.mkdir(parents=True)
        module = source / "ReproProbe.qm"
        module.write_text(HEADER if split else HEADER + FUNCTIONS)
        body = source / "functions.qc" if split else module
        if split:
            body.write_text(FUNCTIONS)
        # Different mtimes must not enter a mapped deployment artifact.
        timestamp = 1700000000 + len(name)
        os.utime(module, (timestamp, timestamp))
        os.utime(body, (timestamp, timestamp))
        output = self.root / name
        output.mkdir()
        artifact = output / "ReproProbe.qmod"
        self.run_tool(QCC, "-m", "-O3", f"--file-prefix-map={self.root}=/earlier-match",
                      f"--file-prefix-map={source}={mapped_prefix}",
                      "--file-prefix-map=/literal/source=/must-not-change-literals", *extra,
                      f"--depfile={output}/module.d", "-o", artifact, source if split else module)
        self.assertIn(str(module).replace(" ", "\\ "), (output / "module.d").read_text())
        index = json.loads(self.run_tool(QCC, "--dump-index-json", artifact))
        self.assertEqual(mapped_prefix + "/ReproProbe.qm", index["source"])
        self.assertNotIn(str(source), json.dumps(index))
        self.assertNotIn(str(source).encode(), artifact.read_bytes())
        self.env["QORE_MODULE_DIR"] = str(output)
        result = self.run_tool(QORE, "-l", "ReproProbe", "-e",
            'printf("%d\\n%s\\n%s\\n", repro_value(41), repro_literal(), repro_location());')
        line = next(i for i, text in enumerate(body.read_text().splitlines(), 1)
                    if 'throw "MAPPED-LOCATION"' in text)
        self.assertEqual(f"42\n/literal/source/keep\n{mapped_prefix}/{body.name}:{line}\n", result)
        info = self.run_tool(QCC, "--dump-info", artifact)
        self.assertNotIn("source-mtime-ns", info)
        return artifact

    def test_single_module_artifacts_and_inline_locations(self):
        first = self.compile_module("first")
        second = self.compile_module("different-second")
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_split_module_artifacts_and_source_inclusion(self):
        for mode, extra in (("debug", ()), ("stripped", ("--strip-debug-info",)),
                            ("source", ("--include-source",))):
            first = self.compile_module("first-" + mode, split=True, extra=extra)
            second = self.compile_module("second-longer-" + mode, split=True, extra=extra)
            self.assertEqual(first.read_bytes(), second.read_bytes(), mode)

    def test_standalone_executables_and_literal_values(self):
        outputs = []
        for name in ("one", "another"):
            directory = self.root / name
            directory.mkdir()
            source = directory / "main.qr"
            source.write_text('''%modern
try {
    throw "MAPPED", "/literal/source/keep";
} catch (hash<ExceptionInfo> ex) {
    printf("%s:%d:%s\\n", ex.file, ex.line, ex.desc);
}
''')
            output = directory / "program"
            self.run_tool(QCC, f"--file-prefix-map={directory}=/usr/src/qore-test",
                          "--file-prefix-map=/literal/source=/must-not-change-literals",
                          "-o", output, source)
            self.assertEqual("/usr/src/qore-test/main.qr:3:/literal/source/keep\n",
                             self.run_tool(output, cwd=self.root))
            outputs.append(output.read_bytes())
        self.assertEqual(*outputs)

    def test_relative_maps_and_old_prefix_with_equals(self):
        first = self.compile_module("first=checkout", mapped_prefix=".")
        second = self.compile_module("different=checkout", mapped_prefix=".")
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_compiled_dependency_imports_and_relocated_artifacts(self):
        builds = []
        for name in ("first", "second-longer"):
            source = self.root / (name + "-source")
            output = self.root / (name + "-output")
            source.mkdir()
            output.mkdir()
            provider = source / "ReproProvider.qm"
            provider.write_text(HEADER.replace("ReproProbe", "ReproProvider")
                                + 'public int sub provider_answer() { return 41; }\n')
            consumer = source / "ReproConsumer.qm"
            consumer.write_text(HEADER.replace("ReproProbe", "ReproConsumer")
                                + '%requires ReproProvider\n'
                                + 'public int sub consumer_answer() { return provider_answer() + 1; }\n')
            self.env["QORE_MODULE_DIR"] = str(output)
            for module in (provider, consumer):
                self.run_tool(QCC, "-m", f"--file-prefix-map={source}=/usr/src/qore-test",
                              f"--depfile={output}/{module.stem}.d",
                              "-o", output / (module.stem + ".qmod"), module)
            self.assertIn(str(output / "ReproProvider.qmod"),
                          (output / "ReproConsumer.d").read_text())
            source.rename(source.with_suffix(".hidden"))
            deployed = output.with_suffix(".deployed")
            output.rename(deployed)
            self.env["QORE_MODULE_DIR"] = str(deployed)
            self.assertEqual("42\n", self.run_tool(QORE, "-l", "ReproConsumer",
                "-e", 'printf("%d\\n", consumer_answer());', cwd=self.root))
            builds.append([(deployed / (module + ".qmod")).read_bytes()
                           for module in ("ReproProvider", "ReproConsumer")])
        self.assertEqual(*builds)

    def test_mapping_changes_invalidate_compiler_manifest(self):
        source = self.root / "ReproProbe.qm"
        source.write_text(HEADER + FUNCTIONS)
        output = self.root / "ReproProbe.qmod"
        manifest = self.root / "manifest.json"
        args = (QCC, "-m", "--skip-if-manifest-current", f"--write-manifest={manifest}",
                "-o", output, source)
        self.run_tool(*args, f"--file-prefix-map={self.root}=/first")
        original = output.read_bytes()
        mtime = output.stat().st_mtime_ns
        self.run_tool(*args, f"--file-prefix-map={self.root}=/first")
        self.assertEqual(mtime, output.stat().st_mtime_ns)
        self.run_tool(*args, f"--file-prefix-map={self.root}=/second")
        self.assertNotEqual(original, output.read_bytes())
        self.assertEqual("/second/ReproProbe.qm",
                         json.loads(self.run_tool(QCC, "--dump-index-json", output))["source"])

    def test_module_initialization_reads_deployed_resources(self):
        source = self.root / "source"
        output = self.root / "output"
        source.mkdir()
        output.mkdir()
        module = source / "ReproResource.qm"
        module.write_text('''%modern
module ReproResource {
    version = "1.0";
    desc = "Mapped resource initialization test";
    author = "Qore";
    license = "MIT";
}
public namespace ReproResource {
    public const Value = File::readTextFile(get_script_dir() + "/value.txt");
}
''')
        (source / "value.txt").write_text("source value")
        (output / "value.txt").write_text("deployed value")
        self.run_tool(QCC, "-m", f"--file-prefix-map={source}=/usr/src/qore-test",
                      "-o", output / "ReproResource.qmod", module)
        source.rename(source.with_suffix(".hidden"))
        deployed = output.with_suffix(".deployed")
        output.rename(deployed)
        self.env["QORE_MODULE_DIR"] = str(deployed)
        self.assertEqual("deployed value\n", self.run_tool(QORE, "-l", "ReproResource",
            "-e", 'printf("%s\\n", ReproResource::Value);', cwd=self.root))

    def test_cmake_forwards_quoted_and_configuration_maps(self):
        source = self.root / "source with spaces"
        source.mkdir()
        module = source / "ReproProbe.qm"
        module.write_text(HEADER + FUNCTIONS)
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(AotPrefixMapFixture NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_QCC_EXECUTABLE "{QCC}")
set(QORE_QM_METADATA_ENV "QORE_MODULE_DIR_ONLY=1")
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_AOT_MODULES_DIR lib/qore-modules)
set(QORE_QMOD_INSTALL_COMPONENT runtime)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_FLAGS [=[-O2 "-ffile-prefix-map={source}=/base"]=])
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO [=[-g "-ffile-prefix-map={source}=/configuration"]=])
qore_user_module_aot_rules(ReproProbe 0 "{module}" "{module}")
''')
        build = self.root / "build"
        cmake = os.environ.get("CMAKE_EXECUTABLE", "cmake")
        self.run_tool(cmake, "-S", source, "-B", build)
        self.run_tool(cmake, "--build", build)
        artifact = build / "qlib-qmod/ReproProbe.qmod"
        self.assertEqual("/configuration/ReproProbe.qm",
                         json.loads(self.run_tool(QCC, "--dump-index-json", artifact))["source"])

    def test_invalid_maps_and_incremental_modes_fail_before_emission(self):
        for mapping in ("missing-separator", "=empty"):
            self.assertIn("nonempty OLD prefix",
                          self.run_tool(QCC, "--file-prefix-map=" + mapping, success=False))
        source = self.root / "ReproProbe.qm"
        source.write_text(HEADER + FUNCTIONS)
        for option in ("-c", "--from-objects", "--archive", "--link-qo"):
            self.assertIn("incremental objects", self.run_tool(QCC,
                f"--file-prefix-map={self.root}=.", option, source, success=False))


if __name__ == "__main__":
    unittest.main()
