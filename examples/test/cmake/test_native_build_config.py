#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check recorded compiler flags and native AOT linking with real tools."""

from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

import test_aot_prefix_maps as aot


ROOT = Path(__file__).resolve().parents[3]


class RecordedCFlagsTest(unittest.TestCase):
    def test_c_string_and_shell_argument_roundtrip(self):
        with tempfile.TemporaryDirectory(prefix="qore-recorded-flags-") as temporary:
            root = Path(temporary)
            retained = ["-O2", "-fPIC", "-mmacosx-version-min=13.0", '-DNAME="two words"',
                        "-I/path with spaces", "-DQUOTE=one'two", "-DSEMI=a;b", r"-DBACK=a\b",
                        "-DVALUE=-ffile-prefix-map=keep"]
            outputs = []
            for name in ("first checkout", "second longer checkout"):
                flags = shlex.join(retained + [f"-f{kind}-prefix-map=/{name}=."
                                               for kind in ("file", "debug", "macro")])
                script = root / "flags.cmake"
                script.write_text(f'''include("{ROOT}/cmake/QoreRecordedCFlags.cmake")
set(original [==[{flags}]==])
qore_record_cflags(recorded "${{original}}")
file(WRITE "{root}/flags.h" "#define FLAGS \\\"${{recorded}}\\\"\\n")
file(WRITE "{root}/original" "${{original}}")
''')
                subprocess.run(["cmake", "-P", str(script)], check=True, capture_output=True)
                self.assertEqual(flags, (root / "original").read_text())
                (root / "main.c").write_text('#include <stdio.h>\n#include "flags.h"\n'
                                             'int main(void) { puts(FLAGS); return 0; }\n')
                subprocess.run(["cc", str(root / "main.c"), "-o", str(root / "probe")],
                               check=True, capture_output=True)
                recorded = subprocess.check_output([str(root / "probe")], text=True).strip()
                self.assertEqual(retained, shlex.split(recorded))
                outputs.append((root / "flags.h").read_bytes())
            self.assertEqual(*outputs)

    def test_empty_flags_and_malformed_quoting(self):
        with tempfile.TemporaryDirectory(prefix="qore-recorded-flags-") as temporary:
            script = Path(temporary) / "flags.cmake"
            for flags, valid in (("", True), ("   \n\t", True), ('-DNAME="unterminated', False),
                                 ("-Iunfinished\\", False)):
                script.write_text(f'''include("{ROOT}/cmake/QoreRecordedCFlags.cmake")
qore_record_cflags(recorded [==[{flags}]==])
if(NOT recorded STREQUAL "")
    message(FATAL_ERROR "Expected empty recorded flags")
endif()
''')
                result = subprocess.run(["cmake", "-P", str(script)], text=True, capture_output=True)
                self.assertEqual(valid, result.returncode == 0, result.stderr)
                if not valid:
                    self.assertIn("Unterminated quote or escape", result.stderr)


class NativeLinkConfigTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        aot.AotPrefixMapsTest.setUpClass()
        if sys.platform != "linux" or not shutil.which("readelf"):
            raise unittest.SkipTest("Native ELF configuration tests require Linux and readelf")

    setUp = aot.AotPrefixMapsTest.setUp
    run_tool = aot.AotPrefixMapsTest.run_tool

    def configure_link(self, enabled):
        config = self.root / "aot-link.conf"
        # Older configurations without the new setting retain development RPATHs.
        config.write_text("cxx=c++\n" + ("emit_rpath=0\n" if not enabled else ""))
        self.env["QORE_AOT_LINK_CONF"] = str(config)

    def assert_rpath(self, path, enabled):
        dynamic = self.run_tool("readelf", "-d", path)
        self.assertEqual(enabled, "(RUNPATH)" in dynamic or "(RPATH)" in dynamic, dynamic)
        if not enabled:
            # Removing the dynamic tag after linking would leave the path in .dynstr.
            strings = self.run_tool("readelf", "--string-dump=.dynstr", path)
            self.assertNotIn(str(aot.QCC.parent), strings)

    def test_final_modules_and_executables_with_quoted_paths(self):
        output_dir = self.root / "output with 'quote, comma; dollar$()"
        output_dir.mkdir()
        module = self.root / "ReproProbe.qm"
        module.write_text(aot.HEADER + 'public int sub answer() { return 42; }\n')
        script = self.root / "main.qr"
        script.write_text('%modern\nprintf("42\\n");\n')
        for enabled in (False, True):
            self.configure_link(enabled)
            output = output_dir / "ReproProbe.qmod"
            self.run_tool(aot.QCC, "-m", "-o", output, module)
            self.assert_rpath(output, enabled)
            self.env["QORE_MODULE_DIR"] = str(output_dir)
            self.assertEqual("42\n", self.run_tool(aot.QORE, "-l", "ReproProbe", "-e", "printf(\"%d\\n\", answer());"))
            exe = output_dir / "program"
            self.run_tool(aot.QCC, "-o", exe, script)
            self.assert_rpath(exe, enabled)
            self.assertEqual("42\n", self.run_tool(exe))

    def test_object_link_uses_build_header_sidecar_and_rpath_policy(self):
        self.configure_link(False)
        source = self.root / "main.q"
        source.write_text('%modern\nint sub main() { printf("42\\n"); return 0; }\n')
        objects = self.root / "objects"
        objects.mkdir()
        self.run_tool(aot.QCC, "-c", f"--output-dir={objects}", source)
        inputs = list(objects.glob("*.qo"))
        self.assertEqual(1, len(inputs))
        exe = self.root / "linked program"
        output = self.run_tool(aot.QCC, "-v", "-o", exe, *inputs)
        if (aot.QCC.parent / "qcc-build-paths.conf").is_file():
            self.assertIn(str(aot.QCC.parent / "include"), output)
            relative = (aot.QCC.parent / "qcc-build-paths.conf").read_text().splitlines()[1]
            self.assertIn(str(aot.QCC.parent / relative), output)
        self.assert_rpath(exe, False)
        self.assertEqual("42\n", self.run_tool(exe))

        if (aot.QCC.parent / "qcc-build-paths.conf").is_file():
            # An unrelated build root with a separate source/include directory
            # must work without the original paths compiled into qcc.
            relocated = self.root / "relocated build"
            relocated.mkdir()
            (relocated / "libqore.so").symlink_to(aot.QCC.parent / "libqore.so")
            (relocated / "include").symlink_to(aot.QCC.parent / "include", target_is_directory=True)
            headers = self.root / "separate headers"
            headers.symlink_to(ROOT / "include", target_is_directory=True)
            (relocated / "qcc-build-paths.conf").write_text("qcc-build-paths-v1\n../separate headers\n")
            self.env["QORE_LIBDIR"] = str(relocated)
            output = self.run_tool(aot.QCC, "-v", "-o", exe, *inputs)
            self.assertIn(str(relocated / "../separate headers"), output)
            self.assertNotIn("include dir: " + str(ROOT / "include"), output)
            self.assert_rpath(exe, False)
            self.assertEqual("42\n", self.run_tool(exe))

    def test_multiple_module_objects_with_quoted_paths(self):
        self.configure_link(False)
        source = self.root / "ReproProbe"
        source.mkdir()
        (source / "ReproProbe.qm").write_text(aot.HEADER)
        (source / "answer.qc").write_text('public int sub answer() { return 42; }\n')
        objects = self.root / "objects"
        objects.mkdir()
        inputs = []
        for name in ("ReproProbe.qm", "answer.qc"):
            output = objects / (name + ".qo")
            self.run_tool(aot.QCC, "-c", f"--context={source}", "-o", output, source / name)
            inputs.append(output)
        output_dir = self.root / "multi's output, spaces"
        output_dir.mkdir()
        output = output_dir / "ReproProbe.qmod"
        self.run_tool(aot.QCC, "-m", "--from-objects", f"--context={source}", "-o", output, *inputs)
        self.assert_rpath(output, False)
        self.env["QORE_MODULE_DIR"] = str(output_dir)
        self.assertEqual("42\n", self.run_tool(aot.QORE, "-l", "ReproProbe", "-e", "printf(\"%d\\n\", answer());"))

    def test_http_streaming_compatibility_overload(self):
        source = self.root / "http-abi.cpp"
        source.write_text(r'''#include <qore/Qore.h>
#include <qore/HttpClientConnectionManager.h>
#include <cstring>

int main() {
    qore_init(QL_MIT);
    bool ok = true;
    {
        ExceptionSink xsink;
        HttpClientConnectionManagerBase::Options options;
        HttpClientConnectionManagerBase manager(options, &xsink);
        ok = !xsink;
        manager.closeAll(&xsink);
        ok = ok && !xsink;
        QoreChannel* channel = nullptr;
        auto result = manager.requestStreaming("GET", "http", "127.0.0.1", 1, "/",
            nullptr, nullptr, 0, channel, &xsink);
        ok = ok && result == -1 && xsink && !channel;
        if (xsink) {
            ok = ok && !strcmp(xsink.getExceptionErr().get<const QoreStringNode>()->c_str(),
                "HTTPCLIENT-SHUTDOWN");
            xsink.clear();
        }
        bool reused = false;
        result = manager.requestStreaming("GET", "http", "127.0.0.1", 1, "/",
            nullptr, nullptr, 0, channel, &xsink, nullptr, &reused);
        ok = ok && result == -1 && xsink && !channel && !reused;
        if (xsink) {
            ok = ok && !strcmp(xsink.getExceptionErr().get<const QoreStringNode>()->c_str(),
                "HTTPCLIENT-SHUTDOWN");
            xsink.clear();
        }
    }
    qore_cleanup();
    return ok ? 0 : 1;
}
''')
        output = self.root / "http-abi"
        # Build-generated headers precede public source headers as in qcc's SDK discovery.
        self.run_tool("c++", "-std=c++20", f"-I{aot.QCC.parent}/include", f"-I{ROOT}/include",
                      source, f"-L{aot.QCC.parent}", "-lqore", "-o", output)
        self.run_tool(output)


if __name__ == "__main__":
    unittest.main()
