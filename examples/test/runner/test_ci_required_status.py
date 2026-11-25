#!/usr/bin/env python3
"""The required GitHub status of a pipeline (the context "${REPO_NAME}") reflects every job of the pipeline (qore#7203).

GitHub keeps the last state of a status context, so when every test job posted its result to the required context,
the check showed the job that finished last.  Only the jobs pipeline-passed (which needs every other job and runs when
all of them passed) and github-status (which runs in every outcome) post success or failure there, with
examples/test/docker_test/ci-github-status.sh; these tests check .gitlab-ci.yml and run the script with a stand-in
curl.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CI = ROOT / ".gitlab-ci.yml"
SCRIPT = ROOT / "examples/test/docker_test/ci-github-status.sh"
PASSED = "pipeline-passed"
FINAL = "github-status"
MARKER = "ci-pipeline-passed"
# the top-level keys of .gitlab-ci.yml that are not jobs
NOT_JOBS = {"stages", "default", "variables", "workflow", "include"}


def ci_blocks():
    """The top-level keys of .gitlab-ci.yml with the text of each"""
    text = CI.read_text()
    blocks = {}
    for match in re.finditer(r"(?ms)^([.\w-]+):\n(.*?)(?=^[.\w-]+:\n|\Z)", text):
        blocks[match.group(1)] = match.group(2)
    return text, blocks


def split_flow(items):
    """The items of a YAML flow sequence (the text between its brackets)"""
    result, depth, current = [], 0, ""
    for char in items:
        if char in "{[":
            depth += 1
        elif char in "}]":
            depth -= 1
        if char == "," and depth == 0:
            result.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        result.append(current.strip())
    return result


def needs(block):
    """The needs of a job: {job name: {"optional": bool, "artifacts": bool}}"""
    result = {}
    flow = re.search(r"(?m)^  needs: \[(.*)\]$", block)
    if flow:
        for item in split_flow(flow.group(1)):
            if item.startswith("{"):
                fields = dict(re.findall(r"(\w+): ([\w.-]+)", item))
                result[fields["job"]] = {"optional": fields.get("optional") == "true",
                                         "artifacts": fields.get("artifacts", "true") == "true"}
            else:
                result[item] = {"optional": False, "artifacts": True}
        return result
    listed = re.search(r"(?m)^  needs:\n((?:    .*\n)+)", block)
    if listed:
        for match in re.finditer(r"(?m)^    - job: ([\w.-]+)\n((?:      \w+: .*\n)*)", listed.group(1)):
            fields = dict(re.findall(r"(?m)^      (\w+): (.*)$", match.group(2)))
            result[match.group(1)] = {"optional": fields.get("optional") == "true",
                                      "artifacts": fields.get("artifacts", "true") == "true"}
        # every item is a "- job:" map
        items = re.findall(r"(?m)^    - ", listed.group(1))
        assert len(items) == len(result), "needs items that are not '- job:' maps"
    return result


def has_rules(blocks, job):
    """True when rules can leave the job out of a pipeline (its own, or those of a template it extends)"""
    block = blocks[job]
    if re.search(r"(?m)^  (rules|only|except):", block):
        return True
    extends = re.search(r"(?m)^  extends: (.+)$", block)
    if extends:
        names = [name.strip() for name in extends.group(1).strip("[]").split(",")]
        return any(has_rules(blocks, name) for name in names)
    return False


class CiRequiredStatusConfigTest(unittest.TestCase):
    def setUp(self):
        self.text, self.blocks = ci_blocks()
        self.jobs = {name for name in self.blocks if not name.startswith(".") and name not in NOT_JOBS}

    def job_texts(self, job):
        """The text of the job and of every template it extends"""
        texts = [self.blocks[job]]
        extends = re.search(r"(?m)^  extends: (.+)$", self.blocks[job])
        if extends:
            for name in extends.group(1).strip("[]").split(","):
                texts += self.job_texts(name.strip())
        return texts

    def test_only_the_status_jobs_post_the_required_context(self):
        # no job posts the bare context itself, and no script but ci-github-status.sh does: the per-job contexts
        # (${REPO_NAME}-ir, ...) are not required and may stay
        bare = re.compile(r'context\\?"\s*:\s*\\?"\$\{?REPO_NAME\}?\\?"')
        self.assertIsNone(bare.search(self.text), "a job of .gitlab-ci.yml posts the required status itself")
        for script in sorted((ROOT / "examples/test/docker_test").glob("*.sh")):
            if script == SCRIPT:
                continue
            with self.subTest(script=script.name):
                self.assertIsNone(bare.search(script.read_text()), "the script posts the required status")
        # the script is run by the resolver (pending), pipeline-passed (passed) and github-status (final) only
        runs = {}
        for job, block in self.blocks.items():
            for mode in re.findall(r"ci-github-status\.sh (\w+)", block):
                runs.setdefault(mode, []).append(job)
        self.assertEqual({"pending": ["resolve-qore-test-base-images"], "passed": [PASSED], "final": [FINAL]}, runs)
        # the default before_script, which the test jobs without their own run, posts nothing
        default = self.blocks["default"]
        self.assertNotIn("statuses", default)
        # the job of each status context is the only one that posts it: a parallel job's shards share it
        for job, block in self.blocks.items():
            if job in (PASSED, FINAL):
                continue
            states = re.findall(r'\\"state\\": \\"(\w+)\\", \\"context\\": \\"\$\{REPO_NAME\}(-[\w-]+)\\"', block)
            for state, suffix in states:
                with self.subTest(job=job, context=suffix):
                    self.assertIn(state, ("pending", "success", "failure"))
                    others = [other for other, text in self.blocks.items()
                              if other != job and '\\"context\\": \\"${REPO_NAME}' + suffix + '\\"' in text]
                    self.assertEqual([], others, "the context is posted by one job only")

    def test_pipeline_passed_needs_every_job(self):
        self.assertIn(PASSED, self.jobs)
        block = self.blocks[PASSED]
        needed = needs(block)
        gating = self.jobs - {PASSED, FINAL}
        self.assertGreater(len(gating), 20)
        self.assertEqual(sorted(gating), sorted(needed), "pipeline-passed needs every other job of the pipeline")
        for job in sorted(gating):
            with self.subTest(job=job):
                # a job that rules can leave out of a pipeline is optional (a need of a job that is not in the
                # pipeline is an error), and no other job is
                self.assertEqual(has_rules(self.blocks, job), needed[job]["optional"])
                # nothing is downloaded (the builds' build/, the test jobs' crash dumps) but the dotenv report of
                # tree-already-tested, which names the pipeline that passed the tree
                self.assertEqual(job == "tree-already-tested", needed[job]["artifacts"])
                # nothing makes a job's failure a pass: no job may fail without failing the pipeline
                # also in its rules and in the templates it extends
                for text in self.job_texts(job):
                    self.assertNotRegex(text, r"(?m)^\s+allow_failure:")
                    # the job's own when, or a rule's ("    - when:", or "      when:" after "    - if:"); not
                    # artifacts:when
                    self.assertNotRegex(text, r"(?m)^(  |    - |      )when: (manual|always|on_failure|delayed)")
                # a parallel job (parallel: N or parallel: matrix) is needed by its name, which needs every shard
                parallel = re.search(r"(?m)^  parallel:(.*)$", self.blocks[job])
                if parallel:
                    self.assertRegex(parallel.group(1), r"^ \d+$|^$")
        self.assertRegex(block, r"(?m)^  when: on_success$")
        self.assertRegex(block, r"(?m)^  stage: \.post$")
        self.assertRegex(block, r"(?m)^  services: \[\]$")
        self.assertRegex(block, r"(?m)^  before_script: \[\]$")
        self.assertNotRegex(block, r"(?m)^  (rules|only|except|allow_failure|retry):")
        # the marker is the artifact that github-status reads
        self.assertRegex(block, r"(?m)^  artifacts:\n    when: always\n    paths:\n      - {}\n".format(MARKER))
        self.assertIn('marker="${{QORE_CI_PASSED_MARKER:-{}}}"'.format(MARKER), SCRIPT.read_text())

    def test_github_status_runs_in_every_outcome(self):
        self.assertIn(FINAL, self.jobs)
        block = self.blocks[FINAL]
        self.assertEqual({PASSED: {"optional": False, "artifacts": True},
                          "tree-already-tested": {"optional": True, "artifacts": True}}, needs(block))
        self.assertRegex(block, r"(?m)^  when: always$")
        self.assertRegex(block, r"(?m)^  stage: \.post$")
        self.assertRegex(block, r"(?m)^  services: \[\]$")
        self.assertRegex(block, r"(?m)^  before_script: \[\]$")
        self.assertNotRegex(block, r"(?m)^  (rules|only|except|allow_failure):")
        # it and pipeline-passed run on the tag, which needs no resolved digest: a failed resolver is reported too
        for job in (PASSED, FINAL):
            self.assertRegex(self.blocks[job], r"(?m)^  image: \$CI_REGISTRY/infrastructure/qore-test-base/"
                                               r"qore-test-base:develop$")

    def test_pending_at_pipeline_start(self):
        resolver = self.blocks["resolve-qore-test-base-images"]
        self.assertRegex(resolver, r"(?m)^  stage: \.pre$")
        self.assertNotRegex(resolver, r"(?m)^  rules:")
        self.assertRegex(resolver, r"(?m)^  before_script:\n    - sh test/docker_test/ci-github-status\.sh "
                                   r"pending\n")


class CiRequiredStatusScriptTest(unittest.TestCase):
    TOKEN = "stand-in-token-7203"

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-ci-required-status-")
        self.dir = Path(self.tmp.name)
        self.work = self.dir / "work"
        self.work.mkdir()
        self.bin = self.dir / "bin"
        self.bin.mkdir()
        self.log = self.dir / "curl.log"
        # a stand-in curl: writes its arguments, one per line, and exits with CURL_RC
        curl = self.bin / "curl"
        curl.write_text('#!/bin/sh\nfor a in "$@"; do printf "%s\\n" "$a"; done >> "$CURL_LOG"\n'
                        'printf "%s\\n" "--" >> "$CURL_LOG"\nexit "${CURL_RC:-0}"\n')
        curl.chmod(curl.stat().st_mode | stat.S_IXUSR)

    def tearDown(self):
        self.tmp.cleanup()

    def run_script(self, mode, env=None, dry_run=False, marker=False):
        if marker:
            (self.work / MARKER).write_text("")
        environment = {"PATH": "{}:{}".format(self.bin, os.environ.get("PATH", "/usr/bin:/bin")),
                       "REPO_NAME": "qore", "CI_COMMIT_SHA": "0123abcd", "CI_PIPELINE_URL": "https://ci/p/7",
                       "CI_PROJECT_URL": "https://ci/mirror/qore", "CURL_LOG": str(self.log)}
        if dry_run:
            environment["QORE_CI_GITHUB_STATUS_DRY_RUN"] = "1"
        environment.update(env or {})
        return subprocess.run(["sh", str(SCRIPT), mode], cwd=self.work, env=environment, capture_output=True,
                              text=True, timeout=60)

    def posts(self):
        if not self.log.exists():
            return []
        return [call.strip("\n").split("\n") for call in self.log.read_text().split("--\n") if call.strip()]

    def body(self, call):
        return call[call.index("-d") + 1]

    def test_final_decides_by_marker(self):
        result = self.run_script("final", dry_run=True, marker=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('{"state": "success", "context": "qore", "description": "Gitlab CI: every job passed", '
                      '"target_url": "https://ci/p/7"}', result.stdout)
        (self.work / MARKER).unlink()
        result = self.run_script("final", dry_run=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('{"state": "failure", "context": "qore", "description": "Gitlab CI: a job failed or was '
                      'canceled", "target_url": "https://ci/p/7"}', result.stdout)
        self.assertNotIn('"success"', result.stdout)

    def test_passed_writes_marker(self):
        result = self.run_script("passed", dry_run=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertTrue((self.work / MARKER).is_file())
        self.assertIn('"state": "success", "context": "qore"', result.stdout)

    def test_tree_tested(self):
        result = self.run_script("final", env={"QORE_TREE_TESTED_IN": "63064"}, dry_run=True, marker=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('{"state": "success", "context": "qore", "description": "tree tested in pipeline 63064", '
                      '"target_url": "https://ci/mirror/qore/-/pipelines/63064"}', result.stdout)

    def test_pending(self):
        result = self.run_script("pending", dry_run=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('{"state": "pending", "context": "qore", "description": "Gitlab CI", '
                      '"target_url": "https://ci/p/7"}', result.stdout)

    def test_posts_with_curl(self):
        result = self.run_script("final", env={"GITHUB_ACCESS_TOKEN": self.TOKEN})
        self.assertEqual(0, result.returncode, result.stderr)
        calls = self.posts()
        self.assertEqual(1, len(calls))
        call = calls[0]
        self.assertIn("https://api.github.com/repos/qoretechnologies/qore/statuses/0123abcd", call)
        self.assertEqual(self.TOKEN, call[call.index("--oauth2-bearer") + 1])
        self.assertIn("-f", call)
        self.assertIn('"state": "failure"', self.body(call))
        self.assertNotIn(self.TOKEN, result.stdout + result.stderr)

    def test_failed_post(self):
        # pipeline-passed and github-status fail when they cannot post (after a retried job only pipeline-passed
        # runs again); the pending post never fails its job, and is tried once
        env = {"GITHUB_ACCESS_TOKEN": self.TOKEN, "CURL_RC": "22"}
        for mode in ("final", "passed"):
            with self.subTest(mode=mode):
                result = self.run_script(mode, env=env, marker=True)
                self.assertEqual(1, result.returncode)
                self.assertIn("could not be posted", result.stderr)
                self.assertNotIn(self.TOKEN, result.stdout + result.stderr)
        # the marker is written before the post, so github-status still posts success
        (self.work / MARKER).unlink()
        self.run_script("passed", env=env)
        self.assertTrue((self.work / MARKER).is_file())
        self.log.unlink()
        result = self.run_script("pending", env=env)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertNotIn(self.TOKEN, result.stdout + result.stderr)
        call = self.posts()[0]
        self.assertNotIn("--retry", call)
        self.assertEqual("10", call[call.index("--max-time") + 1])
        # without a token nothing is posted, and github-status fails
        self.log.unlink()
        result = self.run_script("final", marker=True)
        self.assertEqual(1, result.returncode)
        self.assertIn("GITHUB_ACCESS_TOKEN is not set", result.stderr)
        self.assertEqual([], self.posts())

    def test_bad_mode(self):
        for args in ("", "success"):
            with self.subTest(mode=args):
                result = self.run_script(args, dry_run=True)
                self.assertEqual(2, result.returncode)
                self.assertIn("usage:", result.stderr)


if __name__ == "__main__":
    unittest.main()
