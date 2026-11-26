#!/usr/bin/env python3
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
"""Reports the compiler warnings in Qore's own sources from the build logs of the CI build jobs (qore#7757).

The jobs that build Qore keep the compiler output in a log (ci-build-log.sh: build/build.log of the build jobs and of
the macOS job, build-separate-units/build.log of the build of separate compilation units).  The check-build-warnings
job runs this script with --fetch: it downloads that file of each of these jobs (only that file of its artifacts), finds
the warnings that gcc and clang report for files of the Qore tree (not for bundled third-party code or system
headers), prints them and writes them to a report.  Local logs are checked with --log PLATFORM=PATH.

Exit codes:
- 0: no warning in Qore's own sources (also when no build log was found: the build jobs were skipped)
- 3 (WARNINGS_FOUND): there are warnings; the CI job allows this exit code only, so that the pipeline passes with a
  warning and any other failure of the check is a failure
- 1: the check itself failed (an unreadable log, the GitLab API could not be used)
- 2: wrong arguments
"""

import argparse
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.request

WARNINGS_FOUND = 3

# the jobs whose build logs are checked, by the platform they are reported for, with the path of the log in their
# artifacts: the build jobs (build.sh), the macOS job, which builds with test-macos.sh, and the build of separate
# compilation units (build-separate-units.sh), which shows warnings that the single compilation unit hides
BUILD_JOBS = {
    "ubuntu-amd64": ("build-ubuntu-amd64", "build/build.log"),
    "ubuntu-arm64": ("build-ubuntu-arm64", "build/build.log"),
    "alpine-amd64": ("build-alpine-amd64", "build/build.log"),
    "alpine-arm64": ("build-alpine-arm64", "build/build.log"),
    "macos": ("test-macos", "build/build.log"),
    "ubuntu-amd64-non-scu": ("check-build-separate-units", "build-separate-units/build.log"),
}

# the first line of a log that ci-build-log.sh writes: the source directory of the build
LOG_HEADER_RE = re.compile(r"^qore-ci-build-log source-dir=(?P<dir>.*)$")

# the statuses of the jobs whose artifacts can have a log: a job that failed after its build (a failed test of the
# macOS job) has one
DONE_STATUSES = ("success", "failed")

# file:line[:column]: warning: message [-Wflag] (gcc and clang)
WARNING_RE = re.compile(r"^(?P<path>[^\s:][^:]*):(?P<line>\d+):(?:(?P<col>\d+):)?\s+warning:\s+(?P<msg>.*?)"
                        r"(?:\s+\[(?P<flag>-W[^\]]+)\])?\s*$")

# the parts of a path that mark bundled third-party code
THIRD_PARTY_PARTS = ("_deps", "ngtcp2", "nghttp3", "third_party", "thirdparty", "vendor")

# the extensions of the sources that the build generates C++ code from, by priority
GENERATOR_EXTENSIONS = (".qpp", ".ypp", ".lpp")


def is_third_party(rel):
    """Returns True if the repository-relative path is in bundled third-party code"""
    return any(part in THIRD_PARTY_PARTS for part in Path(rel).parts)


def source_of_generated(rel, source_dir):
    """Returns the lib/ source that a file of a build directory was generated from, or None"""
    parts = Path(rel).parts
    if not parts or not parts[0].startswith("build"):
        return None
    stem = Path(rel).stem
    for ext in GENERATOR_EXTENSIONS:
        candidate = Path("lib") / (stem + ext)
        if source_dir is None or (Path(source_dir) / candidate).is_file():
            return candidate.as_posix()
    return None


def own_path(path, log_source_dir, source_dir):
    """Returns the repository-relative path of a warning's file if it is one of Qore's own, else None

    A path is Qore's own if it is in the source tree of the build (log_source_dir) and not in bundled third-party code;
    a file that the build generated from a Qore source (a qpp .cpp in the build directory) is reported as that source.
    """
    p = path
    if os.path.isabs(p):
        if not log_source_dir:
            return None
        base = log_source_dir.rstrip("/") + "/"
        if not p.startswith(base):
            # a system header or a file outside the tree
            return None
        p = p[len(base):]
    p = os.path.normpath(p)
    if p.startswith(".."):
        return None
    if is_third_party(p):
        return None
    generated = source_of_generated(p, source_dir)
    # another generated file of the build directory is reported as it is
    return generated or p


def parse_log(text, log_source_dir, source_dir=None):
    """Returns the warnings of a build log in Qore's own sources, each once, in the order of the log

    The source directory of the build is the one that the first line of the log names (ci-build-log.sh), else
    log_source_dir.
    """
    seen = set()
    rv = []
    lines = text.splitlines()
    if lines:
        header = LOG_HEADER_RE.match(lines[0])
        if header:
            log_source_dir = header.group("dir")
    for line in lines:
        m = WARNING_RE.match(line)
        if not m:
            continue
        rel = own_path(m.group("path"), log_source_dir, source_dir)
        if rel is None:
            continue
        warning = {
            "file": rel,
            "line": int(m.group("line")),
            "flag": m.group("flag") or "",
            "message": m.group("msg"),
        }
        key = (warning["file"], warning["line"], warning["flag"], warning["message"])
        if key in seen:
            continue
        seen.add(key)
        rv.append(warning)
    return rv


def api_request(url, headers, opener):
    """Returns the body of a GET request"""
    request = urllib.request.Request(url, headers=headers)
    with opener(request, timeout=60) as response:
        return response.read()


def api_headers():
    """Returns the authentication headers to try, in order: the job's token, then the read API token"""
    rv = []
    if os.environ.get("CI_JOB_TOKEN"):
        rv.append({"JOB-TOKEN": os.environ["CI_JOB_TOKEN"]})
    if os.environ.get("QORE_CI_READ_API_TOKEN"):
        rv.append({"PRIVATE-TOKEN": os.environ["QORE_CI_READ_API_TOKEN"]})
    return rv


def api_get(url, opener, missing_ok=False):
    """Returns the body of a GitLab API request with the first token that is allowed, or None if missing_ok and the
    resource does not exist"""
    headers_list = api_headers()
    if not headers_list:
        raise RuntimeError("no GitLab API token (CI_JOB_TOKEN, QORE_CI_READ_API_TOKEN)")
    last_error = None
    for headers in headers_list:
        try:
            return api_request(url, headers, opener)
        except urllib.error.HTTPError as e:
            # the error is also the response, whose connection it holds
            e.close()
            if e.code == 404 and missing_ok:
                return None
            last_error = e
            if e.code not in (401, 403):
                break
    raise RuntimeError(f"GitLab API request failed: {url}: {last_error}")


def fetch_logs(directory, opener=urllib.request.urlopen):
    """Downloads build/build.log of each build job of the pipeline into the directory; returns {platform: path}"""
    api = os.environ.get("CI_API_V4_URL")
    project = os.environ.get("CI_PROJECT_ID")
    pipeline = os.environ.get("CI_PIPELINE_ID")
    if not api or not project or not pipeline:
        raise RuntimeError("the GitLab API is not known (CI_API_V4_URL, CI_PROJECT_ID, CI_PIPELINE_ID)")
    jobs = {}
    page = 1
    while True:
        body = api_get(f"{api}/projects/{project}/pipelines/{pipeline}/jobs?per_page=100&page={page}", opener)
        batch = json.loads(body)
        for job in batch:
            jobs[job["name"]] = job
        if len(batch) < 100:
            break
        page += 1
    rv = {}
    for platform, (name, log_path) in BUILD_JOBS.items():
        job = jobs.get(name)
        if not job or job.get("status") not in DONE_STATUSES:
            state = f"is {job.get('status', '?')}" if job else "is not in the pipeline"
            print(f"{platform}: no build log ({name} {state})")
            continue
        body = api_get(f"{api}/projects/{project}/jobs/{job['id']}/artifacts/{log_path}", opener, missing_ok=True)
        if body is None:
            # the job ended before its build (the tree had already passed, see tree-tested-skip.sh), or failed in it
            print(f"{platform}: no build log in {name}")
            continue
        path = Path(directory) / f"{name}.log"
        path.write_bytes(body)
        rv[platform] = path
    return rv


def write_report(warnings_by_platform, report):
    """Writes the text report and a JSON report beside it; returns the text"""
    lines = []
    for platform, warnings in warnings_by_platform.items():
        for w in warnings:
            flag = f" [{w['flag']}]" if w["flag"] else ""
            lines.append(f"{platform}: {w['file']}:{w['line']}{flag} {w['message']}")
    text = "\n".join(lines) + ("\n" if lines else "")
    if report:
        Path(report).write_text(text)
        Path(report).with_suffix(".json").write_text(json.dumps(warnings_by_platform, indent=2) + "\n")
    return text


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--fetch", action="store_true",
                        help="download build/build.log of each build job of the CI pipeline")
    parser.add_argument("--log", action="append", default=[], metavar="PLATFORM=PATH",
                        help="a local build log")
    parser.add_argument("--log-source-dir", default=os.environ.get("CI_PROJECT_DIR"),
                        help="the source directory of the builds, as their logs name it (default: CI_PROJECT_DIR)")
    parser.add_argument("--source-dir", default=None,
                        help="the source tree, to map generated files to their sources (default: the log source dir)")
    parser.add_argument("--report", default="build-warnings.txt", help="the report to write")
    parser.add_argument("--download-dir", default="build-logs", help="where --fetch saves the logs")
    args = parser.parse_args(argv)
    if not args.fetch and not args.log:
        parser.print_usage(sys.stderr)
        return 2
    source_dir = args.source_dir or args.log_source_dir
    logs = {}
    try:
        for spec in args.log:
            platform, sep, path = spec.partition("=")
            if not sep or not platform or not path:
                print(f"invalid --log argument: {spec!r}", file=sys.stderr)
                return 2
            if not Path(path).is_file():
                print(f"{platform}: no build log ({path})")
                continue
            logs[platform] = Path(path)
        if args.fetch:
            Path(args.download_dir).mkdir(parents=True, exist_ok=True)
            logs.update(fetch_logs(args.download_dir))
        warnings_by_platform = {}
        for platform, path in logs.items():
            warnings_by_platform[platform] = parse_log(path.read_text(errors="replace"), args.log_source_dir,
                                                       source_dir)
    except (OSError, RuntimeError, ValueError) as e:
        print(f"check-build-warnings: {e}", file=sys.stderr)
        return 1
    text = write_report(warnings_by_platform, args.report)
    count = sum(len(w) for w in warnings_by_platform.values())
    if not logs:
        print("no build log to check")
        return 0
    if not count:
        print(f"no compiler warning in Qore's own sources ({', '.join(logs)})")
        return 0
    print(f"{count} compiler warning(s) in Qore's own sources:")
    print(text, end="")
    return WARNINGS_FOUND


if __name__ == "__main__":
    sys.exit(main())
