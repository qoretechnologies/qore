#!/usr/bin/env python3
"""Exercise the check of the compile memory of the CI builds (qore#7780): test/docker_test/ci-compile-memory.py, the
compiler launcher of build.sh that logs the peak memory of each compile and writes the report of the build, and the
check of the reports in test/docker_test/check-build-warnings.py, which warns (exit code 3, allowed by its job) when a
build's peak is above 90% of the memory limit of its container less 2 GiB.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_check_build_warnings import GitLab, SRC  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "test/docker_test/ci-compile-memory.py"
CHECKER = ROOT / "test/docker_test/check-build-warnings.py"
BUILD_SH = ROOT / "test/docker_test/build.sh"
CI = ROOT / ".gitlab-ci.yml"

GIB = 1 << 30


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ccm = load("ci_compile_memory", TOOL)
cbw = load("check_build_warnings_memory", CHECKER)


def memory_report(peak=None, limit=None, compiles=()):
    """Returns the text of a report as ci-compile-memory.py writes it"""
    lines = []
    if peak is not None:
        lines.append(f"peak_bytes={peak}")
    if limit is not None:
        lines.append(f"limit_bytes={limit}")
    lines.append(f"compiles={len(compiles)}")
    for rss, secs, out in compiles:
        lines.append(f"compile={rss}\t{secs}\t{out}")
    return "\n".join(lines) + "\n"


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-ccm-")
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)
        self.log = self.dir / "compile-memory.log"

    def launch(self, args, log=True):
        env = {k: v for k, v in os.environ.items() if k != "QORE_CI_COMPILE_MEMORY_LOG"}
        if log:
            env["QORE_CI_COMPILE_MEMORY_LOG"] = str(self.log)
        return subprocess.run([str(TOOL)] + args, capture_output=True, text=True, env=env)

    def test_launcher_is_executable(self):
        # CMake runs the launcher itself (CMAKE_<LANG>_COMPILER_LAUNCHER)
        self.assertTrue(os.access(TOOL, os.X_OK))
        self.assertTrue(TOOL.read_text().startswith("#!/usr/bin/env python3\n"))

    def test_logs_the_memory_of_a_grandchild(self):
        # the compiler driver (cc, c++) runs the compiler proper (cc1plus) in a child: its memory is the compile's
        compiler = self.dir / "fake-cc"
        compiler.write_text("#!/bin/sh\n"
                            f"\"{sys.executable}\" -c 'b = bytearray(300 << 20); b[::4096] = b\"x\" * len(b[::4096])'\n"
                            "echo compiled\n")
        compiler.chmod(0o755)
        out = self.dir / "obj dir" / "a.cpp.o"
        result = self.launch([str(compiler), "-c", "a.cpp", "-o", str(out)])
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("compiled\n", result.stdout)
        lines = self.log.read_text().splitlines()
        self.assertEqual(1, len(lines))
        rss, secs, path = lines[0].split("\t")
        self.assertGreaterEqual(int(rss), 300 << 20)
        self.assertLess(int(rss), 2 * GIB)
        self.assertGreaterEqual(float(secs), 0)
        self.assertEqual(str(out), path)

    def test_status_of_the_compiler(self):
        result = self.launch(["sh", "-c", "echo error >&2; exit 5", "x.o"])
        self.assertEqual(5, result.returncode)
        self.assertEqual("error\n", result.stderr)
        # a failed compile is logged too: a compile that was killed for its memory is one
        self.assertEqual(1, len(self.log.read_text().splitlines()))

    def test_appends_each_compile(self):
        for i in range(3):
            self.assertEqual(0, self.launch(["true", "-o", f"{i}.o"]).returncode)
        self.assertEqual(["0.o", "1.o", "2.o"], [line.split("\t")[2] for line in self.log.read_text().splitlines()])

    def test_no_log_without_the_variable(self):
        self.assertEqual(0, self.launch(["true", "-o", "x.o"], log=False).returncode)
        self.assertFalse(self.log.exists())

    def test_missing_compiler(self):
        result = self.launch([str(self.dir / "no-such-compiler"), "-c", "a.cpp"])
        self.assertEqual(127, result.returncode)
        self.assertIn("cannot run", result.stderr)
        self.assertFalse(self.log.exists())

    def test_usage(self):
        result = self.launch([])
        self.assertEqual(2, result.returncode)
        self.assertIn("usage:", result.stderr)

    def test_output_of(self):
        self.assertEqual("x.o", ccm.output_of(["cc", "-o", "x.o", "-c", "a.c"]))
        self.assertEqual("a.c", ccm.output_of(["cc", "-c", "a.c"]))
        # -o as the last argument has no file: the last argument is reported
        self.assertEqual("-o", ccm.output_of(["cc", "a.c", "-o"]))
        self.assertEqual("", ccm.output_of([]))


class ReportTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-ccm-report-")
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)
        self.log = self.dir / "compile-memory.log"
        self.out = self.dir / "compile-memory.txt"

    def report(self, cgroup):
        with mock.patch.object(ccm, "cgroup_values", return_value=cgroup), mock.patch("sys.stdout"):
            self.assertEqual(0, ccm.report(str(self.log), str(self.out)))
        return cbw.parse_memory(self.out.read_text())

    def test_largest_compiles_first(self):
        lines = [f"{(i * 7919) % 50 + 1}\t{i}.00\tobj/{i}.o" for i in range(30)]
        # a line that is not a compile's (a truncated line) is ignored
        self.log.write_text("\n".join(lines) + "\nnot a line\n123\n")
        mem = self.report((40 * GIB, 24 * GIB))
        self.assertEqual(40 * GIB, mem["peak"])
        self.assertEqual(24 * GIB, mem["limit"])
        self.assertEqual(30, mem["compiles"])
        self.assertEqual(ccm.TOP_COMPILES, len(mem["top"]))
        sizes = [rss for rss, _, _ in mem["top"]]
        self.assertEqual(sorted(sizes, reverse=True), sizes)
        self.assertEqual(max(int(line.split("\t")[0]) for line in lines), sizes[0])

    def test_unknown_cgroup(self):
        self.log.write_text(f"{5 * GIB}\t60.00\tbuild/lib/libqore.cpp.o\n")
        mem = self.report((None, None))
        self.assertIsNone(mem["peak"])
        self.assertIsNone(mem["limit"])
        self.assertEqual([(5 * GIB, 60.0, "build/lib/libqore.cpp.o")], mem["top"])

    def test_no_log(self):
        # a build whose launcher logged nothing (no compile ran) has an empty report
        mem = self.report((GIB, None))
        self.assertEqual(0, mem["compiles"])
        self.assertEqual([], mem["top"])

    def test_cgroup_values_of_this_system(self):
        # whatever the system has: sizes in bytes or None, never the "max" of an unlimited group
        peak, limit = ccm.cgroup_values()
        for v in (peak, limit):
            self.assertTrue(v is None or (isinstance(v, int) and 0 < v < 1 << 60), v)


class CheckTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-ccm-check-")
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)

    def test_threshold(self):
        # 90% of the limit less 2 GiB: the 24 GiB limit of the arm64 build jobs warns above 19.6 GiB
        self.assertEqual(int(24 * GIB * 0.9) - 2 * GIB, cbw.memory_threshold(24 * GIB))

    def test_near_the_limit(self):
        top = [(15 * GIB, 900.0, "lib/CMakeFiles/libqore.dir/libqore.cpp.o"), (3 * GIB, 60.0, "a.o"),
               (2 * GIB, 30.0, "b.o"), (GIB, 10.0, "c.o")]
        threshold = cbw.memory_threshold(24 * GIB)
        line, near = cbw.check_memory("ubuntu-arm64-debug", cbw.parse_memory(
            memory_report(threshold + 1, 24 * GIB, top)))
        self.assertTrue(near)
        self.assertIn("NEAR THE LIMIT", line)
        # the largest compiles are named, by their object file
        self.assertIn("libqore.cpp.o 15.0 GiB, a.o 3.0 GiB, b.o 2.0 GiB", line)
        self.assertNotIn("c.o", line)
        line, near = cbw.check_memory("ubuntu-arm64-debug", cbw.parse_memory(
            memory_report(threshold, 24 * GIB, top)))
        self.assertFalse(near)
        self.assertNotIn("NEAR", line)
        self.assertIn("peak 19.6 GiB of 24.0 GiB (warning above 19.6 GiB)", line)

    def test_without_the_peak_of_the_container(self):
        # the largest compile stands for the build when the container's peak is not known
        line, near = cbw.check_memory("x", cbw.parse_memory(memory_report(None, 16 * GIB, [(13 * GIB, 1.0, "a.o")])))
        self.assertTrue(near)
        self.assertIn("largest compile 13.0 GiB", line)

    def test_limit_unknown(self):
        line, near = cbw.check_memory("x", cbw.parse_memory(memory_report(100 * GIB, None, [(15 * GIB, 1.0, "a.o")])))
        self.assertFalse(near)
        self.assertIn("(limit unknown)", line)

    def test_nothing_recorded(self):
        line, near = cbw.check_memory("x", cbw.parse_memory(memory_report()))
        self.assertFalse(near)
        self.assertIn("no compile memory recorded", line)

    def test_malformed_report(self):
        for text in ("peak_bytes=12x\n", "no equals sign\n", "compile=1\t2\n", "compile=x\t1.0\ta.o\n"):
            with self.assertRaises(ValueError, msg=text):
                cbw.parse_memory(text)

    def main(self, *memory):
        report = self.dir / "warnings.txt"
        mem_report = self.dir / "memory.txt"
        args = ["--report", str(report), "--memory-report", str(mem_report)]
        for label, text in memory:
            path = self.dir / f"{label}.txt"
            if text is not None:
                path.write_text(text)
            args += ["--memory", f"{label}={path}"]
        with mock.patch("sys.stdout"), mock.patch("sys.stderr"):
            rc = cbw.main(args)
        return rc, (mem_report.read_text() if mem_report.exists() else None)

    def test_main_exit_codes(self):
        ok = memory_report(10 * GIB, 24 * GIB, [(9 * GIB, 1.0, "a.o")])
        near = memory_report(23 * GIB, 24 * GIB, [(15 * GIB, 1.0, "libqore.cpp.o")])
        rc, text = self.main(("ubuntu-arm64-debug", ok), ("alpine-arm64-debug", near))
        self.assertEqual(cbw.WARNINGS_FOUND, rc)
        self.assertIn("ubuntu-arm64-debug: peak 10.0 GiB", text)
        self.assertRegex(text, r"(?m)^alpine-arm64-debug: peak 23\.0 GiB .*NEAR THE LIMIT")
        rc, text = self.main(("ubuntu-arm64-debug", ok))
        self.assertEqual(0, rc)
        # a build that was skipped has no report: nothing to check
        rc, text = self.main(("ubuntu-amd64-debug", None))
        self.assertEqual(0, rc)
        # the report of the earlier check is not kept
        self.assertEqual("", text)
        # a malformed report is a failure of the check
        rc, _ = self.main(("ubuntu-amd64-debug", "garbage\n"))
        self.assertEqual(1, rc)
        with mock.patch("sys.stderr"):
            self.assertEqual(2, cbw.main(["--memory", "no-equals-sign"]))

    def test_warnings_and_memory(self):
        # a build near its limit fails the check with the warning's code even without a compiler warning
        log = self.dir / "clean.log"
        log.write_text("[100%] Built target qore\n")
        path = self.dir / "mem.txt"
        path.write_text(memory_report(23 * GIB, 24 * GIB))
        with mock.patch("sys.stdout"):
            self.assertEqual(cbw.WARNINGS_FOUND, cbw.main(
                ["--log", f"ubuntu-arm64-debug={log}", "--memory", f"ubuntu-arm64-debug={path}",
                 "--report", str(self.dir / "w.txt"), "--memory-report", str(self.dir / "m.txt")]))

    def test_fetch(self):
        jobs = [{"id": 1, "name": "build-ubuntu-amd64", "status": "success"},
                {"id": 2, "name": "build-ubuntu-arm64", "status": "success"},
                {"id": 3, "name": "build-alpine-amd64", "status": "success"}]
        files = {(1, "build/build.log"): "ok\n", (2, "build/build.log"): "ok\n",
                 (1, "build/compile-memory.txt"): memory_report(12 * GIB, None, [(11 * GIB, 1.0, "a.o")]),
                 (2, "build/compile-memory.txt"): memory_report(21 * GIB, 24 * GIB, [(15 * GIB, 1.0, "b.o")])}
        api = GitLab(jobs, files)
        self.addCleanup(api.close)
        env = {"CI_API_V4_URL": api.url(), "CI_PROJECT_ID": "7", "CI_PIPELINE_ID": "99", "CI_JOB_TOKEN": "jt",
               "CI_PROJECT_DIR": SRC}
        mem_report = self.dir / "memory.txt"
        with mock.patch.dict(os.environ, env, clear=False), mock.patch("sys.stdout"), mock.patch("sys.stderr"):
            # only the token that the test gives: a CI job has QORE_CI_READ_API_TOKEN set (patch.dict restores it)
            os.environ.pop("QORE_CI_READ_API_TOKEN", None)
            rc = cbw.main(["--fetch", "--download-dir", str(self.dir / "logs"), "--report", str(self.dir / "w.txt"),
                           "--memory-report", str(mem_report)])
        self.assertEqual(cbw.WARNINGS_FOUND, rc)
        text = mem_report.read_text()
        self.assertIn("ubuntu-amd64-debug: peak 12.0 GiB (limit unknown)", text)
        self.assertRegex(text, r"(?m)^ubuntu-arm64-debug: peak 21\.0 GiB .*NEAR THE LIMIT")
        # the build without a report is left out
        self.assertNotIn("alpine-amd64", text)
        # the pipeline's jobs are listed once for both kinds of file
        self.assertEqual(1, sum(1 for r in api.requests if "/pipelines/99/jobs?" in r[0] and r[0].endswith("page=1")))


class CiConfigTest(unittest.TestCase):
    def test_build_sh_uses_the_launcher(self):
        text = BUILD_SH.read_text()
        self.assertIn('COMPILE_MEMORY_TOOL="${QORE_SRC_DIR}/test/docker_test/ci-compile-memory.py"', text)
        for lang in ("C", "CXX"):
            self.assertIn(f"-DCMAKE_{lang}_COMPILER_LAUNCHER=${{COMPILE_MEMORY_TOOL}}", text)
        self.assertRegex(text, r"\$\{QMOD_CACHE_ARG:\+\"\$\{QMOD_CACHE_ARG\}\"\} \$\{COMPILE_MEMORY_ARGS\}")
        self.assertIn('QORE_CI_COMPILE_MEMORY_LOG="${QORE_SRC_DIR}/build/compile-memory.log"', text)
        # the report is written after the build, also when it failed, and the build's status is kept
        build = text.index("qore_ci_build_logged build.log make")
        rep = text.index('python3 "${COMPILE_MEMORY_TOOL}" report "${QORE_CI_COMPILE_MEMORY_LOG}" '
                         '"${QORE_SRC_DIR}/build/compile-memory.txt"')
        exit_rc = text.index('exit "${build_rc}"')
        self.assertLess(build, rep)
        self.assertLess(rep, exit_rc)
        self.assertIn("|| build_rc=$?", text[build:rep])

    def test_reports_are_in_the_artifacts(self):
        text = CI.read_text()
        for job, path in cbw.MEMORY_JOBS.values():
            self.assertEqual("build/compile-memory.txt", path)
            m = re.search(rf"^{re.escape(job)}:\n(?P<body>(?:(?: .*)?\n)+)", text, re.M)
            self.assertTrue(m, job)
            body = m.group("body")
            self.assertIn("test/docker_test/build.sh", body, job)
            self.assertRegex(body, r"artifacts:\n\s+paths:\n\s+- build/\n", job)
        check = re.search(r"^check-build-warnings:\n(?P<body>(?:[ \n].*\n)+)", text, re.M).group("body")
        self.assertIn("- compile-memory.txt\n", check)
        for job, _ in cbw.MEMORY_JOBS.values():
            self.assertRegex(check, rf"job: {job}\n\s+optional: true\n\s+artifacts: false")

    def test_memory_labels_are_build_labels(self):
        # each build checked for its memory is one whose warnings are checked, under the same label
        for label, (job, _) in cbw.MEMORY_JOBS.items():
            self.assertEqual(job, cbw.BUILD_JOBS[label][0], label)


if __name__ == "__main__":
    unittest.main()
