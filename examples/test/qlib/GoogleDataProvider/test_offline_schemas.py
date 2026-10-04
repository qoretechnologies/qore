#!/usr/bin/env python3
"""Package integrity and denied-egress regressions for issue #5474.

Run after building GoogleDataProvider-qmod and ProviderIndex-qmod:
    python3 -B -W error examples/test/qlib/GoogleDataProvider/test_offline_schemas.py -v
Linux network tests use an unprivileged user/network namespace, never the host firewall.
"""
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

import errno
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
QLIB = ROOT / "qlib"
QORE = Path(os.environ.get("QORE_BIN", ROOT / "build/qore")).resolve()
ENV = dict(os.environ, QORE_MODULE_DIR=str(QLIB), QORE_BIN=str(QORE),
           LD_LIBRARY_PATH=str(QORE.parent) + ":" + os.environ.get("LD_LIBRARY_PATH", ""))
ENV.pop("QORE_DATA_PROVIDERS", None)
ENV.pop("QORE_PROVIDER_INDEX_DIR", None)


def run(args, **kwargs):
    return subprocess.run(args, env=ENV, text=True, capture_output=True, timeout=60, **kwargs)


def network_child(mode):
    """Establish a real route whose traffic is either rejected or silently dropped."""
    for args in (["ip", "link", "set", "lo", "up"],
                 ["ip", "address", "add", "198.51.100.1/32", "dev", "lo"],
                 ["ip", "route", "add", "default", "dev", "lo"]):
        subprocess.run(args, check=True, capture_output=True)
    rule = "drop" if mode == "DROP" else "ip protocol icmp accept; tcp flags & rst == rst accept; reject"
    subprocess.run(["nft", "-f", "-"], input=(
        "table inet qore_test { chain output { type filter hook output priority 0; "
        f"policy accept; {rule}; " + "}\n}\n"), text=True, check=True, capture_output=True)
    # Prove the namespace has the intended failure mode before running the regression.
    with socket.socket() as probe:
        probe.settimeout(0.2)
        try:
            probe.connect(("198.51.100.1", 443))
            raise AssertionError("egress was not denied")
        except TimeoutError:
            assert mode == "DROP", "REJECT unexpectedly timed out"
        except OSError as ex:
            assert mode == "REJECT" and ex.errno == errno.ECONNREFUSED, repr(ex)
    with tempfile.TemporaryDirectory(prefix="qore-google-egress-") as tmp:
        for test in (HERE / "OfflineSchemas.qtest", HERE / "OfflineIndex.qtest",
                     HERE.parent / "DataProvider/ProviderIndexWorkers.qtest"):
            trace = Path(tmp) / (test.stem + ".trace")
            result = run(["strace", "-f", "-e", "trace=network", "-o", str(trace),
                          str(QORE), "--enable-debug", str(test), "-v"])
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            calls = trace.read_text()
            assert "AF_INET" not in calls, calls
            print(result.stdout, end="")


class OfflineSchemasTest(unittest.TestCase):
    def test_package_integrity_and_repair(self):
        """Absent, stale, truncated and wrong-API packages fail; repaired source/AOT loads succeed."""
        original = QLIB / "GoogleDataProvider"
        for compiled in (False, True):
            with self.subTest(compiled=compiled), tempfile.TemporaryDirectory(prefix="qore-google-package-") as tmp:
                module = Path(tmp) / "GoogleDataProvider"
                module.mkdir()
                for src in original.iterdir():
                    if src.suffix in (".qm", ".qc", ".json") or (compiled and src.suffix == ".qmod"):
                        shutil.copyfile(src, module / src.name)
                target = module / "calendar.v3.json"
                content = target.read_bytes()
                script = Path(tmp) / "load.qr"
                script.write_text(f'''%requires {module}
DataProviderStaticMetadataHelper metadata();
@assert(GoogleDataProviderBase::getSchemaInfo("calendar").version == "v3");
@assert(GoogleDataProviderBase::getRequestInfo("gmail").hasKey("users/messages/get"));
''')
                def invoke():
                    return run([str(QORE), "--enable-debug", str(script)])
                result = invoke()
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                stale = json.loads(content)
                stale["revision"] = "20000101"
                for damaged in (None, b"{", json.dumps(stale).encode(), (module / "gmail.v1.json").read_bytes()):
                    with self.subTest(compiled=compiled, damage="missing" if damaged is None else len(damaged)):
                        if damaged is None:
                            target.unlink()
                        else:
                            target.write_bytes(damaged)
                        result = invoke()
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("GOOGLE-SCHEMA-UNAVAILABLE", result.stdout + result.stderr)
                        target.write_bytes(content)
                        result = invoke()
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                # A failed load must also be recoverable in the same interpreter, without a permanent error cache.
                target.write_bytes(b"{")
                repair = Path(tmp) / "repair.qr"
                repair.write_text('''bool failed = False;
try { load_module(ARGV[0]); }
catch (hash<ExceptionInfo> ex) {
    @assert(ex.err == "GOOGLE-SCHEMA-UNAVAILABLE" || ex.desc.find("GOOGLE-SCHEMA-UNAVAILABLE") >= 0);
    failed = True;
}
@assert(failed);
File out();
out.open2(ARGV[1], O_WRONLY | O_TRUNC);
out.write(ReadOnlyFile::readBinaryFile(ARGV[2]));
out.close();
load_module(ARGV[0]);
printf("recovered\\n");
''')
                result = run([str(QORE), "--enable-debug", str(repair), str(module), str(target),
                              str(original / "calendar.v3.json")])
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("recovered", result.stdout)

    def test_denied_egress(self):
        for tool in ("unshare", "ip", "nft", "strace"):
            if not shutil.which(tool):
                self.skipTest(f"{tool} is required for Linux network fault tests")
        probe = run(["unshare", "-Urn", "true"])
        if probe.returncode:
            self.skipTest("unprivileged user/network namespaces are unavailable")
        for mode in ("REJECT", "DROP"):
            with self.subTest(mode=mode):
                result = run(["unshare", "-Urn", sys.executable, str(Path(__file__).resolve()), "--net-child", mode])
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--net-child":
        network_child(sys.argv[2])
    else:
        unittest.main()
