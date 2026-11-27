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

The jobs that build Qore keep the compiler output in a log (ci-build-log.sh: build/build.log of the build jobs, of the
macOS job and of the release build of the performance tests, build-separate-units/build.log of the build of separate
compilation units); the warnings are reported by platform and build type (BUILD_JOBS).  The check-build-warnings
job runs this script with --fetch: it downloads that file of each of these jobs (only that file of its artifacts), finds
the warnings that gcc and clang report for files of the Qore tree (not for bundled third-party code or system
headers), prints them and writes them to a report.  Local logs are checked with --log LABEL=PATH.

It also reports the memory of the compiles of the build jobs (build/compile-memory.txt, which build.sh writes with
ci-compile-memory.py, qore#7780: the peak memory and the limit of the job's container and its largest compiles), and
warns when a build's peak is above 90% of its container's limit minus 2 GiB (MEMORY_LIMIT_FRACTION,
MEMORY_HEADROOM_BYTES), before a growth of a compile gets the job killed; local reports are checked with
--memory LABEL=PATH.  A build whose limit is not known is reported without a check.

Exit codes:
- 0: no warning in Qore's own sources and no build near its memory limit (also when no build log was found: the build
  jobs were skipped)
- 3 (WARNINGS_FOUND): there are warnings, or a build's memory is near its limit; the CI job allows this exit code only, so that the pipeline passes with a
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

# the jobs whose build logs are checked, by the platform and build type they are reported for (the last part of the
# label: debug or release, as the job's script configures it), with the path of the log in their artifacts: the build
# jobs (build.sh), the macOS job, which builds with test-macos.sh, the build of separate compilation units
# (build-separate-units.sh), which shows warnings that the single compilation unit hides, and the release build of the
# performance tests (build-perf.sh), the only release build with gcc, which reports warnings that only its
# optimizations find (it builds the targets of the performance tests)
BUILD_JOBS = {
    "ubuntu-amd64-debug": ("build-ubuntu-amd64", "build/build.log"),
    "ubuntu-arm64-debug": ("build-ubuntu-arm64", "build/build.log"),
    "alpine-amd64-debug": ("build-alpine-amd64", "build/build.log"),
    "alpine-arm64-debug": ("build-alpine-arm64", "build/build.log"),
    "macos-release": ("test-macos", "build/build.log"),
    "ubuntu-amd64-non-scu-debug": ("check-build-separate-units", "build-separate-units/build.log"),
    "ubuntu-amd64-release": ("build-perf", "build/build.log"),
}

# the jobs whose compile memory is checked (build.sh), by label, with the path of the report in their artifacts
MEMORY_JOBS = {
    "ubuntu-amd64-debug": ("build-ubuntu-amd64", "build/compile-memory.txt"),
    "ubuntu-arm64-debug": ("build-ubuntu-arm64", "build/compile-memory.txt"),
    "alpine-amd64-debug": ("build-alpine-amd64", "build/compile-memory.txt"),
    "alpine-arm64-debug": ("build-alpine-arm64", "build/compile-memory.txt"),
}

# a build's memory is near its limit above this fraction of the limit less the headroom
MEMORY_LIMIT_FRACTION = 0.9
MEMORY_HEADROOM_BYTES = 2 << 30

# the largest compiles that a warning about a build's memory names
MEMORY_TOP_COMPILES = 3

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


def gib(n):
    """Returns a size in bytes as GiB for the report"""
    return f"{n / (1 << 30):.1f} GiB"


def parse_memory(text):
    """Returns the compile memory report of a build (ci-compile-memory.py): {"peak": bytes or None, "limit": bytes or
    None, "compiles": count, "top": [(bytes, seconds, output)...]}; raises ValueError for a malformed report"""
    rv = {"peak": None, "limit": None, "compiles": 0, "top": []}
    for line in text.splitlines():
        if not line:
            continue
        key, sep, value = line.partition("=")
        if not sep:
            raise ValueError(f"malformed compile memory report line: {line!r}")
        if key == "peak_bytes":
            rv["peak"] = int(value)
        elif key == "limit_bytes":
            rv["limit"] = int(value)
        elif key == "compiles":
            rv["compiles"] = int(value)
        elif key == "compile":
            parts = value.split("\t", 2)
            if len(parts) != 3:
                raise ValueError(f"malformed compile memory report line: {line!r}")
            rv["top"].append((int(parts[0]), float(parts[1]), parts[2]))
    return rv


def memory_threshold(limit):
    """Returns the memory above which a build with the given limit is near it"""
    return int(limit * MEMORY_LIMIT_FRACTION) - MEMORY_HEADROOM_BYTES


def check_memory(label, mem):
    """Returns (the report line of a build's compile memory, True if it is near its limit)

    The build's peak is that of its container (cgroup); without it, that of its largest compile.
    """
    top = mem["top"]
    peak = mem["peak"] if mem["peak"] is not None else (top[0][0] if top else None)
    if peak is None:
        return f"{label}: no compile memory recorded", False
    what = "peak" if mem["peak"] is not None else "largest compile"
    largest = ", ".join(f"{Path(out).name} {gib(rss)}" for rss, _, out in top[:MEMORY_TOP_COMPILES])
    text = f"{label}: {what} {gib(peak)}"
    near = False
    if mem["limit"] is None:
        text += " (limit unknown)"
    else:
        threshold = memory_threshold(mem["limit"])
        near = peak > threshold
        text += f" of {gib(mem['limit'])} (warning above {gib(threshold)})"
        if near:
            text += ": NEAR THE LIMIT"
    if largest:
        text += f"; largest compiles: {largest}"
    return text, near


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


def pipeline_jobs(opener):
    """Returns the jobs of the pipeline by name"""
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
    return jobs


def fetch_artifacts(directory, job_files, suffix, opener=urllib.request.urlopen, jobs=None):
    """Downloads a file of the artifacts of each job of job_files ({label: (job name, path)}) of the pipeline into the
    directory as <job name><suffix>; returns {label: path}"""
    api = os.environ.get("CI_API_V4_URL")
    project = os.environ.get("CI_PROJECT_ID")
    if jobs is None:
        jobs = pipeline_jobs(opener)
    rv = {}
    for label, (name, log_path) in job_files.items():
        job = jobs.get(name)
        if not job or job.get("status") not in DONE_STATUSES:
            state = f"is {job.get('status', '?')}" if job else "is not in the pipeline"
            print(f"{label}: no {Path(log_path).name} ({name} {state})")
            continue
        body = api_get(f"{api}/projects/{project}/jobs/{job['id']}/artifacts/{log_path}", opener, missing_ok=True)
        if body is None:
            # the job ended before its build (the tree had already passed, see tree-tested-skip.sh), or failed in it
            print(f"{label}: no {Path(log_path).name} in {name}")
            continue
        path = Path(directory) / f"{name}{suffix}"
        path.write_bytes(body)
        rv[label] = path
    return rv


def fetch_logs(directory, opener=urllib.request.urlopen, jobs=None):
    """Downloads the build log of each build job of the pipeline into the directory; returns {label: path}"""
    return fetch_artifacts(directory, BUILD_JOBS, ".log", opener, jobs)


def fetch_memory(directory, opener=urllib.request.urlopen, jobs=None):
    """Downloads the compile memory report of each build job of the pipeline into the directory; returns
    {label: path}"""
    return fetch_artifacts(directory, MEMORY_JOBS, "-compile-memory.txt", opener, jobs)


def write_report(warnings_by_label, report):
    """Writes the text report and a JSON report beside it; returns the text"""
    lines = []
    for label, warnings in warnings_by_label.items():
        for w in warnings:
            flag = f" [{w['flag']}]" if w["flag"] else ""
            lines.append(f"{label}: {w['file']}:{w['line']}{flag} {w['message']}")
    text = "\n".join(lines) + ("\n" if lines else "")
    if report:
        Path(report).write_text(text)
        Path(report).with_suffix(".json").write_text(json.dumps(warnings_by_label, indent=2) + "\n")
    return text


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--fetch", action="store_true",
                        help="download build/build.log of each build job of the CI pipeline")
    parser.add_argument("--log", action="append", default=[], metavar="LABEL=PATH",
                        help="a local build log, by its label (ex: ubuntu-amd64-release)")
    parser.add_argument("--log-source-dir", default=os.environ.get("CI_PROJECT_DIR"),
                        help="the source directory of the builds, as their logs name it (default: CI_PROJECT_DIR)")
    parser.add_argument("--source-dir", default=None,
                        help="the source tree, to map generated files to their sources (default: the log source dir)")
    parser.add_argument("--memory", action="append", default=[], metavar="LABEL=PATH",
                        help="a local compile memory report (ci-compile-memory.py), by its label")
    parser.add_argument("--report", default="build-warnings.txt", help="the report to write")
    parser.add_argument("--memory-report", default="compile-memory.txt",
                        help="the report of the compile memory of the builds to write")
    parser.add_argument("--download-dir", default="build-logs", help="where --fetch saves the logs")
    args = parser.parse_args(argv)
    if not args.fetch and not args.log and not args.memory:
        parser.print_usage(sys.stderr)
        return 2
    source_dir = args.source_dir or args.log_source_dir
    logs = {}
    memory = {}
    try:
        for arg_name, specs, files, what in (("--log", args.log, logs, "build log"),
                                             ("--memory", args.memory, memory, "compile memory report")):
            for spec in specs:
                label, sep, path = spec.partition("=")
                if not sep or not label or not path:
                    print(f"invalid {arg_name} argument: {spec!r}", file=sys.stderr)
                    return 2
                if not Path(path).is_file():
                    print(f"{label}: no {what} ({path})")
                    continue
                files[label] = Path(path)
        if args.fetch:
            Path(args.download_dir).mkdir(parents=True, exist_ok=True)
            jobs = pipeline_jobs(urllib.request.urlopen)
            logs.update(fetch_logs(args.download_dir, jobs=jobs))
            memory.update(fetch_memory(args.download_dir, jobs=jobs))
        warnings_by_label = {}
        for label, path in logs.items():
            warnings_by_label[label] = parse_log(path.read_text(errors="replace"), args.log_source_dir,
                                                       source_dir)
        memory_by_label = {}
        for label, path in memory.items():
            memory_by_label[label] = parse_memory(path.read_text())
    except (OSError, RuntimeError, ValueError) as e:
        print(f"check-build-warnings: {e}", file=sys.stderr)
        return 1
    rv = 0
    memory_lines = []
    near = 0
    for label, mem in memory_by_label.items():
        line, is_near = check_memory(label, mem)
        memory_lines.append(line)
        near += is_near
    if args.memory_report:
        # written also without a report, so that it never keeps that of an earlier check
        Path(args.memory_report).write_text("".join(line + "\n" for line in memory_lines))
    if memory_by_label:
        print("compile memory of the builds:")
        for line in memory_lines:
            print(line)
        if near:
            print(f"{near} build(s) near the memory limit of the job's container (qore#7780)")
            rv = WARNINGS_FOUND
    text = write_report(warnings_by_label, args.report)
    count = sum(len(w) for w in warnings_by_label.values())
    if not logs:
        print("no build log to check")
        return rv
    if not count:
        print(f"no compiler warning in Qore's own sources ({', '.join(logs)})")
        return rv
    print(f"{count} compiler warning(s) in Qore's own sources:")
    print(text, end="")
    return WARNINGS_FOUND


if __name__ == "__main__":
    sys.exit(main())
