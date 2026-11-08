#!/usr/bin/env python3
"""The shards of a CI test job (CI_NODE_INDEX of CI_NODE_TOTAL) are balanced by the durations of the tests.

run_tests.sh packs the tests longest first, each into the shard with the least work so far, by the durations in
examples/test/docker_test/test-durations.tsv (the column of the job, named by CI_JOB_NAME without its shard); a test
with no duration counts as the median of the others.  update-test-durations.py writes the file from the job logs of
a pipeline.  These tests run run_tests.sh -l (which lists the tests that would run) on a tree of stand-in tests, check
that the shards of a job split its tests exactly and as the packing does, check the durations file against the CI
jobs, and parse job logs with the update script.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
DURATIONS = ROOT / "examples/test/docker_test/test-durations.tsv"

spec = importlib.util.spec_from_file_location("update_test_durations",
                                              ROOT / "examples/test/docker_test/update-test-durations.py")
update = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update)


def lpt(durations, shards):
    """The reference packing: longest first (then by name), each into the first shard with the least work"""
    loads = [0.0] * shards
    plan = {}
    for name, seconds in sorted(durations.items(), key=lambda item: (-max(item[1], 0.1), item[0])):
        best = loads.index(min(loads))
        plan[name] = best + 1
        loads[best] += max(seconds, 0.1)
    return plan, loads


class ShardPlanTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="qore-runner-shards-")
        self.root = Path(self.directory.name)
        self.tests = self.root / "examples/test"
        (self.tests / "docker_test").mkdir(parents=True)

    def tearDown(self):
        self.directory.cleanup()

    def make_tests(self, names):
        for name in names:
            path = self.tests / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("#!/usr/bin/env qore\n")

    def write_durations(self, columns, rows):
        lines = ["# durations", "\t".join(["test"] + columns)]
        for name, values in rows.items():
            lines.append("\t".join(["examples/test/" + name] + values))
        (self.tests / "docker_test/test-durations.tsv").write_text("\n".join(lines) + "\n")

    def run_list(self, total=None, index=None, job=None, extra=None):
        env = {"PATH": os.defpath}
        if total is not None:
            env.update({"CI_NODE_TOTAL": str(total), "CI_NODE_INDEX": str(index)})
        if job is not None:
            env["CI_JOB_NAME"] = job
        env.update(extra or {})
        result = subprocess.run(["sh", str(ROOT / "run_tests.sh"), "-l"], cwd=self.root, env=env,
                                capture_output=True, text=True, timeout=120)
        names = [line[len("./examples/test/"):] for line in result.stdout.split()]
        return result, names

    def shards(self, total, job="test-x 1/1", extra=None):
        plan = {}
        for index in range(1, total + 1):
            result, names = self.run_list(total, index, re.sub(r" \d+/\d+$", " %d/%d" % (index, total), job), extra)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual(sorted(names), names, "a shard runs its tests in the order of their names")
            self.assertRegex(result.stderr, r"Shard %d/%d selected: %d tests" % (index, total, len(names)))
            for name in names:
                self.assertNotIn(name, plan, "%s is in one shard only" % name)
                plan[name] = index
        return plan

    def test_shards_split_tests_exactly(self):
        names = ["t%02d.qtest" % i for i in range(23)]
        self.make_tests(names)
        self.write_durations(["test-ubuntu-amd64"], {name: ["%d" % (i * 7 % 11)] for i, name in enumerate(names)})
        for total in (1, 2, 3, 4, 5):
            with self.subTest(total=total):
                if total == 1:
                    result, listed = self.run_list(1, 1)
                    self.assertEqual(names, listed)
                    continue
                plan = self.shards(total)
                self.assertEqual(set(names), set(plan), "every test is in a shard")

    def test_packing_is_longest_first(self):
        durations = {"a.qtest": 30.0, "b.qtest": 1.0, "c.qtest": 20.0, "d.qtest": 10.0, "e.qtest": 10.0,
                     "f.qtest": 9.5, "g.qtest": 0.0, "h.qtest": 0.0, "i.qtest": 5.0, "j.qtest": 4.0}
        self.make_tests(durations)
        self.write_durations(["test-alpine-amd64"], {name: ["%.1f" % s] for name, s in durations.items()})
        for total in (2, 3):
            with self.subTest(total=total):
                expected, loads = lpt(durations, total)
                self.assertEqual(expected, self.shards(total, "test-alpine-amd64 1/%d" % total))
                # the shards are balanced: no shard has more than the shortest one plus the longest test
                self.assertLessEqual(max(loads) - min(loads), max(durations.values()))
        # a round-robin split of these tests would give shard 1 the three longest
        self.assertEqual({"a.qtest": 1, "c.qtest": 2}, {k: v for k, v in lpt(durations, 2)[0].items()
                                                        if k in ("a.qtest", "c.qtest")})

    def test_job_column_and_fallback(self):
        names = ["a.qtest", "b.qtest", "c.qtest", "d.qtest"]
        self.make_tests(names)
        # in test-jit, a and b are long; in test-ubuntu-amd64 (the fallback), a and c are
        self.write_durations(["test-jit-ubuntu-amd64", "test-ubuntu-amd64"],
                             {"a.qtest": ["50", "50"], "b.qtest": ["40", "1"], "c.qtest": ["1", "40"],
                              "d.qtest": ["1", "1"]})
        jit = self.shards(2, "test-jit-ubuntu-amd64 1/2")
        self.assertNotEqual(jit["a.qtest"], jit["b.qtest"])
        for job in ("test-unknown 1/2", None):
            with self.subTest(job=job):
                plan = {}
                for index in (1, 2):
                    name = None if job is None else "test-unknown %d/2" % index
                    result, listed = self.run_list(2, index, name)
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertIn("by the durations of test-ubuntu-amd64", result.stderr)
                    plan.update({n: index for n in listed})
                self.assertNotEqual(plan["a.qtest"], plan["c.qtest"])
        # QORE_TEST_DURATION_PROFILE names the column instead of the job
        plan = self.shards(2, "other 1/2", {"QORE_TEST_DURATION_PROFILE": "test-jit-ubuntu-amd64"})
        self.assertEqual(jit, plan)

    def test_test_without_duration_counts_as_median(self):
        names = ["a.qtest", "b.qtest", "c.qtest", "new.qtest"]
        self.make_tests(names)
        self.write_durations(["test-ubuntu-amd64"], {"a.qtest": ["10"], "b.qtest": ["2"], "c.qtest": ["4"],
                                                     "gone.qtest": ["100"]})
        # the median of the selected tests (2, 4, 10) is 4: new.qtest packs like a 4 s test, after c (4 s, by name)
        expected, _ = lpt({"a.qtest": 10, "b.qtest": 2, "c.qtest": 4, "new.qtest": 4}, 2)
        self.assertEqual(expected, self.shards(2))
        result, _ = self.run_list(2, 1)
        self.assertIn(" of 20s by the durations", result.stderr)

    def test_without_durations_file_shards_by_count(self):
        names = ["t%d.qtest" % i for i in range(9)]
        self.make_tests(names)
        plan = self.shards(3)
        self.assertEqual([3, 3, 3], [list(plan.values()).count(i) for i in (1, 2, 3)])

    def test_invalid_shard(self):
        self.make_tests(["a.qtest", "b.qtest"])
        for index in ("0", "3"):
            with self.subTest(index=index):
                result, names = self.run_list(2, index)
                self.assertEqual(1, result.returncode)
                self.assertIn("is not a shard of 2", result.stderr)
                self.assertEqual([], names)


class DurationsFileTest(unittest.TestCase):
    def read(self):
        lines = [line for line in DURATIONS.read_text().splitlines() if line and not line.startswith("#")]
        header = lines[0].split("\t")
        rows = {}
        for line in lines[1:]:
            fields = line.split("\t")
            self.assertEqual(len(header), len(fields), line)
            for value in fields[1:]:
                self.assertRegex(value, r"^(-|\d+(\.\d)?)$", line)
            rows[fields[0]] = fields[1:]
        return header, rows

    def test_columns_are_ci_jobs(self):
        header, rows = self.read()
        self.assertEqual("test", header[0])
        jobs = set(re.findall(r"(?m)^(test-[\w-]+):\s*$", (ROOT / ".gitlab-ci.yml").read_text()))
        for column in header[1:]:
            self.assertIn(column, jobs, "a column of test-durations.tsv is a CI test job")
        # every sharded job has its column
        text = (ROOT / ".gitlab-ci.yml").read_text()
        for job in re.split(r"(?m)^(?=[a-z][\w-]*:\s*$)", text):
            if re.search(r"(?m)^  parallel:\s*\d+", job):
                self.assertIn(job.split(":", 1)[0], header, "the sharded job has durations")
        self.assertIn("test-ubuntu-amd64", header, "the column of a job with none of its own")

    def test_durations_are_current(self):
        # most tests have a duration; when many do not, refresh the file (see update-test-durations.py)
        header, rows = self.read()
        column = header.index("test-ubuntu-amd64") - 1
        tests = [str(path.relative_to(ROOT)) for path in sorted((ROOT / "examples/test").rglob("*.qtest"))]
        known = [test for test in tests if test in rows and rows[test][column] != "-"]
        self.assertGreater(len(known), 0.75 * len(tests), "refresh test-durations.tsv with update-test-durations.py")


class UpdateScriptTest(unittest.TestCase):
    def test_parse_timestamped_log(self):
        log = "\n".join([
            "2026-11-07T12:00:00.000000Z 01O Shard 1/2 selected",
            "2026-11-07T12:00:01.500000Z 01O Running test (1/3): ./examples/test/a.qtest",
            "2026-11-07T12:00:02.000000Z 01O some output",
            "2026-11-07T12:00:11.750000Z 01O Running test (2/3): ./examples/test/dir/b.qtest",
            "2026-11-07T12:01:00.000000Z 01E Running test (3/3): modules/ml/test/c.qtest",
            "2026-11-07T12:01:03.000000Z 01O TESTING RESULT: Success.",
        ])
        self.assertEqual({"examples/test/a.qtest": 10.25, "examples/test/dir/b.qtest": 48.25,
                          "modules/ml/test/c.qtest": 3.0}, update.parse_trace(log))

    def test_parse_measured_times(self):
        # the times run_tests.sh measures itself win over the log's timestamps, which tests run at the same time mix
        log = "\n".join([
            "2026-11-07T12:00:00.000000Z 01O Running test (1/2): ./examples/test/a.qtest",
            "2026-11-07T12:00:00.100000Z 01O Test time: ./examples/test/a.qtest: 12s",
            "2026-11-07T12:00:00.200000Z 01O Running test (2/2): ./examples/test/b.qtest",
            "2026-11-07T12:00:00.300000Z 01O Test time: ./examples/test/b.qtest: 3s",
            "2026-11-07T12:00:00.400000Z 01O TESTING RESULT: Success.",
            "Test time: ./examples/test/c.qtest: 7s",
        ])
        self.assertEqual({"examples/test/a.qtest": 12.0, "examples/test/b.qtest": 3.0,
                          "examples/test/c.qtest": 7.0}, update.parse_trace(log))

    def test_merge_and_render(self):
        a = "2026-11-07T12:00:00Z 01O Running test (1/1): ./examples/test/a.qtest\n" \
            "2026-11-07T12:00:05Z 01O TESTING RESULT: Success.\n"
        b = "2026-11-07T12:00:00Z 01O Running test (1/1): ./examples/test/b.qtest\n" \
            "2026-11-07T12:00:02Z 01O TESTING RESULT: Success.\n"
        columns = update.merge([("test-alpine-amd64 1/3", a), ("test-alpine-amd64 2/3", b),
                                ("test-macos", b), ("build-perf", "no tests here\n")])
        self.assertEqual({"test-alpine-amd64": {"examples/test/a.qtest": 5.0, "examples/test/b.qtest": 2.0},
                          "test-macos": {"examples/test/b.qtest": 2.0}}, columns)
        text = update.render(columns)
        self.assertEqual(["test\ttest-alpine-amd64\ttest-macos", "examples/test/a.qtest\t5.0\t-",
                          "examples/test/b.qtest\t2.0\t2.0"],
                         [line for line in text.splitlines() if not line.startswith("#")])


if __name__ == "__main__":
    unittest.main()
