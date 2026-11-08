#!/usr/bin/env python3
"""The Linux CI test jobs run as many tests at a time as the CPU and memory limits of their pod allow.

qore_ci_test_jobs (examples/test/docker_test/ci-resource-report.sh) reads the limits from a cgroup directory, which
the cases make (cgroup v2 and v1), and the number of CPUs online, which they give; the test scripts of the Linux jobs
give the number it prints to run_tests.sh as QORE_TEST_JOBS.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / "examples/test/docker_test/ci-resource-report.sh"


class CiTestJobsTest(unittest.TestCase):
    def jobs(self, cpus, files, env=None):
        with tempfile.TemporaryDirectory(prefix="qore-ci-test-jobs-") as directory:
            for name, value in files.items():
                path = Path(directory) / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(value + "\n")
            # the test scripts of the jobs source the report under set -e
            result = subprocess.run(["sh", "-c", 'set -e; . "$1"; jobs=$(qore_ci_test_jobs "$2" "$3"); echo "$jobs"',
                                     "sh", str(REPORT),
                                     str(cpus), directory], env=env or {}, capture_output=True, text=True,
                                    timeout=60)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual("", result.stderr)
            return int(result.stdout)

    def test_no_cgroup_files(self):
        # a pod that shows neither cpu.max nor the cgroup v1 files (the CI pods of pipeline 63612): the CPUs online,
        # at most 4, and no failure under set -e
        self.assertEqual(4, self.jobs(32, {}))
        self.assertEqual(2, self.jobs(2, {}))
        # cgroup v1 with only some of the files
        self.assertEqual(3, self.jobs(32, {"cpu/cpu.cfs_quota_us": "250000", "cpu/cpu.cfs_period_us": "100000"}))

    def test_cgroup_v2(self):
        gib = 1024 ** 3
        # the CPU quota, rounded up
        self.assertEqual(3, self.jobs(32, {"cpu.max": "250000 100000", "memory.max": str(16 * gib)}))
        # the memory limit: 1536 MB per test
        self.assertEqual(2, self.jobs(32, {"cpu.max": "400000 100000", "memory.max": str(3 * gib + 1)}))
        # no limits: the CPUs online, at most 4
        self.assertEqual(4, self.jobs(32, {"cpu.max": "max 100000", "memory.max": "max"}))
        self.assertEqual(2, self.jobs(2, {"cpu.max": "max 100000", "memory.max": "max"}))
        # never less than one
        self.assertEqual(1, self.jobs(32, {"cpu.max": "50000 100000", "memory.max": str(gib)}))

    def test_cgroup_v1(self):
        gib = 1024 ** 3
        self.assertEqual(2, self.jobs(8, {"cpu/cpu.cfs_quota_us": "200000", "cpu/cpu.cfs_period_us": "100000",
                                          "memory/memory.limit_in_bytes": str(8 * gib)}))
        # -1 and the huge number of v1 are no limits
        self.assertEqual(3, self.jobs(3, {"cpu/cpu.cfs_quota_us": "-1", "cpu/cpu.cfs_period_us": "100000",
                                          "memory/memory.limit_in_bytes": "9223372036854771712"}))

    def test_no_cgroup_files(self):
        self.assertEqual(4, self.jobs(16, {}))
        self.assertEqual(1, self.jobs(1, {}))

    def test_settings(self):
        gib = 1024 ** 3
        files = {"cpu.max": "max 100000", "memory.max": str(8 * gib)}
        self.assertEqual(8, self.jobs(32, files, {"QORE_CI_MAX_TEST_JOBS": "16", "QORE_CI_TEST_JOB_MEMORY_MB": "1024"}))
        self.assertEqual(1, self.jobs(32, files, {"QORE_CI_TEST_JOB_MEMORY_MB": "8192"}))

    def test_linux_test_jobs_run_tests_in_parallel(self):
        # the test scripts of the Linux jobs set QORE_TEST_JOBS from the pod's limits and give it to run_tests.sh,
        # which they run as another user with an environment of its own
        for script in ("test-ubuntu.sh", "test-alpine.sh"):
            with self.subTest(script=script):
                source = (ROOT / "examples/test/docker_test" / script).read_text()
                self.assertIn('QORE_TEST_JOBS="${QORE_TEST_JOBS:-$(qore_ci_test_jobs)}"', source)
                run = source[source.index("QORE_TEST_JOBS=\"${QORE_TEST_JOBS:-"):]
                run = run[:run.index("./run_tests.sh")]
                self.assertRegex(run, r'gosu qore:qore env \\\n(.*\\\n)*    QORE_TEST_JOBS="\$\{QORE_TEST_JOBS\}" \\\n')


if __name__ == "__main__":
    unittest.main()
