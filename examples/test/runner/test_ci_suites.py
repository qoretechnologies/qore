#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
# persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""A CI job runs all of its test suites and ends with a summary of the failing ones (batch rule v9.2 B)

The CI scripts run their suites with qore_ci_suite (examples/test/docker_test/ci-suites.sh): a failing suite does not
stop the next one, also under set -e, and qore_ci_suites_summary prints every failing suite with its failing tests and
fails the job.  In pipeline 70914 test-macos stopped at a failure of the runner checks and ran no qtest.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
DOCKER = ROOT / "examples" / "test" / "docker_test"
SUITES = DOCKER / "ci-suites.sh"
CI = ROOT / ".gitlab-ci.yml"

# the CI scripts that run test suites, with the suites that each must run through qore_ci_suite
SUITE_SCRIPTS = {
    DOCKER / "test-ubuntu.sh": ["./run_tests.sh"],
    DOCKER / "test-alpine.sh": ["./run_tests.sh"],
    DOCKER / "test-macos.sh": ["-s examples/test/cmake", "-s examples/test/runner", "./run_tests.sh"],
    DOCKER / "test-cmake.sh": ["-s examples/test/cmake", "-s examples/test/runner"],
    DOCKER / "test-perf.sh": ["./run_tests.sh -P"],
    DOCKER / "test-tsan.sh": ["./run_tests.sh ${TSAN_TESTS}", 'test -z "${REPORTS}"'],
    DOCKER / "check-docs.sh": ["test_module_release_notes.py", "check-module-release-notes.py",
                               "test_release_notes.py", "check-release-notes.py", "test_module_mainpage_layout.py",
                               "check-module-mainpage.py"],
    ROOT / "examples" / "test" / "cpp" / "run-sanitizers.sh": ['"$out/core-$name"'],
    DOCKER / "build-perf.sh": ["check-stack-usage.py", "check-hot-inlining.py"],
}


def run_shell(shell, script, cwd=None):
    return subprocess.run([shell, "-c", script], capture_output=True, text=True, cwd=cwd,
                          env={**os.environ, "TMPDIR": tempfile.gettempdir()})


def statements(text):
    """Returns the commands of a shell script with their continuation lines joined, without comments"""
    out = []
    current = ""
    for line in text.splitlines():
        stripped = line.strip()
        if not current and (not stripped or stripped.startswith("#")):
            continue
        current += " " + stripped
        if stripped.endswith("\\"):
            current = current[:-1]
            continue
        out.append(current.strip())
        current = ""
    return out


class SuiteHelperTest(unittest.TestCase):
    def test_failing_first_suite_runs_the_next_and_fails(self):
        # under set -e, with sh and bash: the first suite fails, the next ones run, and the summary fails the script
        script = (f'set -e; . "{SUITES}"; '
                  "qore_ci_suite first sh -c 'echo FAIL: test_a \\(m.T\\); echo ERROR: test_b \\(m.T\\); exit 1'; "
                  "qore_ci_suite second sh -c 'echo second-ran'; "
                  "qore_ci_suite third sh -c 'printf \"TESTING RESULT: Failure.\\nFailed tests:\\n./x/a.qtest\\n"
                  "./x/b.qtest\\n*****\\n\"; exit 2'; "
                  "qore_ci_suites_summary; echo not-reached")
        for shell in ("sh", "bash"):
            r = run_shell(shell, script)
            self.assertNotEqual(0, r.returncode, shell)
            self.assertIn("second-ran", r.stdout, shell)
            self.assertNotIn("not-reached", r.stdout, shell)
            summary = r.stdout[r.stdout.index("CI suite summary"):]
            self.assertIn("FAILED: first (exit code 1)", summary, shell)
            self.assertIn("FAIL: test_a (m.T)", summary, shell)
            self.assertIn("ERROR: test_b (m.T)", summary, shell)
            self.assertIn("passed: second", summary, shell)
            self.assertIn("FAILED: third (exit code 2)", summary, shell)
            self.assertRegex(summary, r"Failed tests:\n +\./x/a\.qtest\n +\./x/b\.qtest\n")
            self.assertIn("3 suites, 2 failed", summary, shell)

    def test_all_passing(self):
        r = run_shell("sh", f'set -e; . "{SUITES}"; qore_ci_suite one true; qore_ci_suite two true; '
                            "qore_ci_suites_summary; echo rc=$?")
        self.assertEqual(0, r.returncode)
        self.assertIn("2 suites, 0 failed", r.stdout)
        self.assertIn("rc=0", r.stdout)

    def test_suite_output_is_shown(self):
        r = run_shell("sh", f'. "{SUITES}"; qore_ci_suite one sh -c "echo shown; echo err >&2; exit 3"; '
                            "qore_ci_suites_summary || echo summary-failed")
        self.assertIn("shown", r.stdout)
        self.assertIn("err", r.stdout)
        self.assertIn("FAILED: one (exit code 3)", r.stdout)
        self.assertIn("no failing test named in its output", r.stdout)
        self.assertIn("summary-failed", r.stdout)


class JobScriptTest(unittest.TestCase):
    def test_check_docs_runs_every_check(self):
        # the docs job with a failing first check still runs the others and fails
        with tempfile.TemporaryDirectory(prefix="qore-ci-suites-") as tmp:
            tree = Path(tmp)
            (tree / "examples" / "test" / "docker_test").mkdir(parents=True)
            (tree / "examples" / "test" / "cmake").mkdir()
            (tree / "doxygen").mkdir()
            shutil.copy(SUITES, tree / "examples" / "test" / "docker_test")
            shutil.copy(DOCKER / "check-docs.sh", tree / "examples" / "test" / "docker_test")
            (tree / "test").symlink_to("examples/test")
            checks = {"examples/test/cmake/test_module_release_notes.py": 1,
                      "doxygen/check-module-release-notes.py": 0,
                      "examples/test/cmake/test_release_notes.py": 0,
                      "doxygen/check-release-notes.py": 0,
                      "examples/test/cmake/test_module_mainpage_layout.py": 0,
                      "doxygen/check-module-mainpage.py": 0}
            for path, rc in checks.items():
                (tree / path).write_text(f"import sys\nprint('ran {Path(path).name}')\nsys.exit({rc})\n")
            subprocess.run(["git", "init", "-q"], cwd=tree, check=True)
            r = subprocess.run(["sh", "examples/test/docker_test/check-docs.sh"], cwd=tree, capture_output=True,
                               text=True, env={**os.environ, "QORE_SRC_DIR": str(tree)})
            self.assertNotEqual(0, r.returncode, r.stdout + r.stderr)
            for path in checks:
                self.assertIn(f"ran {Path(path).name}", r.stdout, path)
            self.assertIn("FAILED: module release notes tests (exit code 1)", r.stdout)
            self.assertIn("6 suites, 1 failed", r.stdout)

    def test_scripts_run_their_suites_through_the_helper(self):
        for script, suites in SUITE_SCRIPTS.items():
            text = script.read_text()
            self.assertRegex(text, r'\. "\$\{?(QORE_SRC_DIR|src)\}?/test/docker_test/ci-suites\.sh"',
                             f"{script.name} sources ci-suites.sh")
            commands = statements(text)
            for suite in suites:
                # a command that runs the suite (not one that names it as the output of a build, -o)
                found = [c for c in commands if not c.startswith("echo")
                         and re.search(r"(?<!-o )" + re.escape(suite) + r"(\s|$|[\"'])", c)]
                self.assertTrue(found, f"{script.name} runs {suite}")
                for c in found:
                    self.assertTrue(c.startswith("qore_ci_suite "), f"{script.name}: {suite} is a suite: {c}")
            # a suite's status is never left to set -e or kept aside where it would stop or hide the next suite
            self.assertNotRegex(text, r"run_tests\.sh[^\n]*\|\| TEST_RC=", script.name)
            self.assertNotRegex(text, r"unittest discover[^\n]*\|\| TEST_RC=", script.name)
            self.assertIn("qore_ci_suites_summary", text, f"{script.name} ends with the summary")
            self.assertGreater(text.rindex("qore_ci_suites_summary"), text.rindex("qore_ci_suite "),
                               f"{script.name}: the summary comes after the last suite")

    def test_docs_job_runs_the_script(self):
        text = CI.read_text()
        m = re.search(r"^check-module-release-notes:\n(?P<body>(?:(?: .*)?\n)+)", text, re.M)
        self.assertTrue(m)
        body = m.group("body")
        self.assertIn("- test/docker_test/check-docs.sh", body)
        # the checks are no longer separate script lines, of which GitLab runs none after the first that fails
        self.assertNotIn("check-release-notes.py", body)

    def test_jobs_fail_when_their_script_fails(self):
        # the jobs that wrap their script in a condition end with an error when it fails, so that pipeline-passed and
        # ci-github-status see the job as failed
        text = CI.read_text()
        for job in ("test-ast-ubuntu-amd64", "test-ir-ubuntu-amd64", "test-jit-ubuntu-amd64",
                    "test-tiered-ubuntu-amd64", "test-macos", "test-perf"):
            m = re.search(rf"^{re.escape(job)}:\n(?P<body>(?:(?: .*)?\n)+)", text, re.M)
            self.assertTrue(m, job)
            body = m.group("body")
            self.assertRegex(body, r"if test/docker_test/test-[a-z]+\.sh; then", job)
            self.assertRegex(body, r"else\n(?:.*\n)*?\s+exit 1\n", job)


if __name__ == "__main__":
    unittest.main()
