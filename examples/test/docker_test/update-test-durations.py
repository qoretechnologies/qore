#!/usr/bin/env python3
"""Writes examples/test/docker_test/test-durations.tsv, the durations of the tests in each CI test job, from the job
logs of a pipeline, for run_tests.sh to balance the shards of a job (see run_tests.sh).

usage: update-test-durations.py [--project mirror/qore] [--output FILE] PIPELINE_ID
       update-test-durations.py [--output FILE] --trace JOB_NAME=FILE ...

The job logs are read with glab (glab api projects/<project>/pipelines/<id>/jobs and .../jobs/<id>/trace), so glab
must be logged in to the GitLab server of the project.  Take a pipeline whose test jobs all passed, ideally one on
an idle cluster: the durations decide only how the tests are split, never whether a test passes.

Each test job (a job whose log runs tests with run_tests.sh) is a column, named by the job without its shard
("test-alpine-amd64 2/3" is test-alpine-amd64); the tests of all its shards are its rows.  A test's duration is the
"Test time: <seconds>" line run_tests.sh prints after each test, or, in the logs of an older run_tests.sh, the time
between the "Running test" lines in the log's timestamps.  The file is written again as a whole; a test that no job
of the pipeline ran has no duration in it, and run_tests.sh gives it the median of the others.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUTPUT = ROOT / "examples/test/docker_test/test-durations.tsv"

# a GitLab job log line: "<ISO time>Z <stream> <text>" (the stream is e.g. "01O" or "00E")
TIMESTAMP = re.compile(r"^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)(\.\d+)?Z \S+ ?")
RUNNING = re.compile(r"Running test \((\d+)/(\d+)\): (\S+)")
TEST_TIME = re.compile(r"Test time: (\S+?)\.qtest: (\d+(?:\.\d+)?)s")
END = re.compile(r"TESTING RESULT:")

HEADER = """\
# The duration in seconds of each test in each CI test job, which run_tests.sh uses to balance the shards of a job
# (CI_NODE_INDEX, CI_NODE_TOTAL): written by update-test-durations.py from the job logs of a pipeline; see that script
# to refresh it.  A test with no duration ("-") counts as the median of the job's other tests.
"""


def test_name(path):
    """The name of a test as run_tests.sh selects it, without a leading ./"""
    return path[2:] if path.startswith("./") else path


def parse_trace(text):
    """Returns the duration of each test that a job log ran, as {test: seconds}"""
    durations = {}
    timed = {}
    current = None
    for line in text.splitlines():
        stamp = TIMESTAMP.match(line)
        when = None
        if stamp:
            when = datetime.datetime.fromisoformat(stamp.group(1)) \
                + datetime.timedelta(seconds=float(stamp.group(2) or 0))
            line = line[stamp.end():]
        match = TEST_TIME.search(line)
        if match:
            timed[test_name(match.group(1) + ".qtest")] = float(match.group(2))
            continue
        match = RUNNING.search(line)
        if match or END.search(line):
            if current and when and current[1]:
                durations[current[0]] = (when - current[1]).total_seconds()
            current = (test_name(match.group(3)), when) if match else None
    # the durations the runner measured itself are exact, also for tests run at the same time
    durations.update(timed)
    return durations


def job_column(name):
    """The column of a job: its name without the shard ("test-ubuntu-amd64 1/2" -> "test-ubuntu-amd64")"""
    return re.sub(r" \d+/\d+$", "", name)


def merge(traces):
    """Returns {column: {test: seconds}} from (job name, log text) pairs; only the logs that ran tests count"""
    columns = {}
    for name, text in traces:
        durations = parse_trace(text)
        if durations:
            columns.setdefault(job_column(name), {}).update(durations)
    return columns


def render(columns):
    names = sorted(columns)
    tests = sorted(set().union(*(columns[name] for name in names))) if names else []
    lines = [HEADER.rstrip("\n"), "\t".join(["test"] + names)]
    for test in tests:
        row = [test]
        for name in names:
            value = columns[name].get(test)
            row.append("-" if value is None else "%.1f" % value)
        lines.append("\t".join(row))
    return "\n".join(lines) + "\n"


def glab_api(path):
    return subprocess.run(["glab", "api", path], check=True, capture_output=True).stdout


def pipeline_traces(project, pipeline):
    encoded = project.replace("/", "%2F")
    jobs = json.loads(glab_api("projects/%s/pipelines/%s/jobs?per_page=100" % (encoded, pipeline)))
    for job in sorted(jobs, key=lambda j: j["name"]):
        if job["stage"] != "test":
            continue
        if job["status"] != "success":
            print("warning: job %s (%s) did not succeed: %s" % (job["name"], job["id"], job["status"]),
                  file=sys.stderr)
        text = glab_api("projects/%s/jobs/%s/trace" % (encoded, job["id"])).decode("utf-8", "replace")
        yield job["name"], text


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("pipeline", nargs="?", help="the ID of the pipeline whose test jobs are read")
    parser.add_argument("--project", default="mirror/qore", help="the GitLab project (default: mirror/qore)")
    parser.add_argument("--trace", action="append", default=[], metavar="JOB_NAME=FILE",
                        help="read the log of a job from a file instead of a pipeline")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="the file written (default: %(default)s)")
    args = parser.parse_args()
    if bool(args.pipeline) == bool(args.trace):
        parser.error("give a pipeline ID or --trace options")
    if args.trace:
        traces = []
        for item in args.trace:
            name, sep, path = item.partition("=")
            if not sep:
                parser.error("--trace takes JOB_NAME=FILE: %s" % item)
            traces.append((name, Path(path).read_text(errors="replace")))
    else:
        traces = pipeline_traces(args.project, args.pipeline)
    columns = merge(traces)
    if not columns:
        print("no job log ran tests", file=sys.stderr)
        return 1
    Path(args.output).write_text(render(columns))
    for name in sorted(columns):
        print("%s: %d tests, %.0f s" % (name, len(columns[name]), sum(columns[name].values())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
