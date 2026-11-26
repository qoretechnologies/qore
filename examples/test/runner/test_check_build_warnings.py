#!/usr/bin/env python3
"""Exercise test/docker_test/check-build-warnings.py, which reports the compiler warnings in Qore's own sources from the
build logs of the CI build jobs, and the check-build-warnings job of .gitlab-ci.yml that runs it (qore#7757).

The job must show warnings as a warning, not a failure: it exits with WARNINGS_FOUND (3), which the job allows, so the
pipeline passes with a warning; any other failure of the check is a failure.  The cases parse sample logs (warnings in
Qore's sources, in bundled third-party code and in system headers, and none), and fetch the logs from a stand-in GitLab
API as the job does.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import http.server
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "test/docker_test/check-build-warnings.py"
CI = ROOT / ".gitlab-ci.yml"

spec = importlib.util.spec_from_file_location("check_build_warnings", SCRIPT)
cbw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cbw)

SRC = "/builds/mirror/qore"

# a log with warnings in Qore's own sources, in a generated qpp .cpp, in third-party code and in a system header
SAMPLE_LOG = f"""[ 10%] Building CXX object CMakeFiles/libqore.dir/lib/ql_debug.cpp.o
In file included from {SRC}/include/qore/intern/QC_HttpClientConnection.h:36,
                 from {SRC}/lib/ql_debug.cpp:40:
{SRC}/lib/ql_debug.cpp:785:37: warning: 'visibility' attribute ignored [-Wattributes]
  785 |     DLLLOCAL DbgCompletionRejectOp(QoreObject* sock_obj, QorePromise* promise, std::string err,
{SRC}/lib/ql_debug.cpp:785:37: warning: 'visibility' attribute ignored [-Wattributes]
{SRC}/lib/thread.cpp:6192:26: warning: format not a string literal and no format arguments [-Wformat-security]
{SRC}/build/QC_AsyncIoController.cpp:101:37: warning: 'visibility' attribute ignored [-Wattributes]
{SRC}/build/_deps/ngtcp2-src/lib/ngtcp2_conn.c:42:5: warning: unused variable 'x' [-Wunused-variable]
{SRC}/modules/ngtcp2/lib/foo.c:7: warning: something [-Wpedantic]
/usr/include/c++/14/bits/stl_vector.h:1100:7: warning: deprecated thing [-Wdeprecated]
{SRC}/lib/QoreLib.cpp:10:1: note: in expansion of macro
ld: warning: creating DT_TEXTREL in a PIE
{SRC}/lib/QoreLib.cpp:12: warning: a warning without a flag
"""


MACOS_SRC = "/private/var/tmp/builds/mirror/qore"

# a log of the macOS job (clang), whose first line names the source directory of its runner (ci-build-log.sh)
CLANG_LOG = f"""qore-ci-build-log source-dir={MACOS_SRC}
[ 20%] Building CXX object CMakeFiles/libqore.dir/lib/QoreLib.cpp.o
{MACOS_SRC}/lib/QoreLib.cpp:5:10: warning: unused variable 'x' [-Wunused-variable]
    5 |     int x = 0;
      |         ^
{MACOS_SRC}/include/qore/intern/QoreLibIntern.h:77:3: warning: 'f' overrides a member function but is not marked \
'override' [-Winconsistent-missing-override]
/Applications/Xcode.app/Contents/Developer/usr/include/c++/v1/vector:10:2: warning: deprecated [-Wdeprecated]
{MACOS_SRC}/build/_deps/nghttp3-src/lib/nghttp3_conn.c:9:1: warning: unused function [-Wunused-function]
1 warning generated.
ld: warning: object file was built for newer macOS version than being linked
"""


class ParseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-cbw-")
        self.addCleanup(self.tmp.cleanup)
        self.source = Path(self.tmp.name)
        (self.source / "lib").mkdir()
        (self.source / "lib" / "QC_AsyncIoController.qpp").write_text("")

    def test_own_sources_only(self):
        warnings = cbw.parse_log(SAMPLE_LOG, SRC, str(self.source))
        self.assertEqual([
            ("lib/ql_debug.cpp", 785, "-Wattributes"),
            ("lib/thread.cpp", 6192, "-Wformat-security"),
            ("lib/QC_AsyncIoController.qpp", 101, "-Wattributes"),
            ("lib/QoreLib.cpp", 12, ""),
        ], [(w["file"], w["line"], w["flag"]) for w in warnings])

    def test_no_warnings(self):
        log = f"[100%] Built target qore\n{SRC}/lib/QoreLib.cpp:10:1: note: something\n"
        self.assertEqual([], cbw.parse_log(log, SRC, str(self.source)))

    def test_clang_log_with_its_source_dir(self):
        # the header's source directory, not the one given (the checking job's), is the build's
        warnings = cbw.parse_log(CLANG_LOG, SRC, str(self.source))
        self.assertEqual([
            ("lib/QoreLib.cpp", 5, "-Wunused-variable"),
            ("include/qore/intern/QoreLibIntern.h", 77, "-Winconsistent-missing-override"),
        ], [(w["file"], w["line"], w["flag"]) for w in warnings])

    def test_relative_paths(self):
        log = "lib/QoreLib.cpp:3:4: warning: relative [-Wshadow]\n"
        self.assertEqual([("lib/QoreLib.cpp", 3)],
                         [(w["file"], w["line"]) for w in cbw.parse_log(log, SRC, str(self.source))])

    def test_main_exit_codes(self):
        with_warnings = self.source / "with.log"
        with_warnings.write_text(SAMPLE_LOG)
        clean = self.source / "clean.log"
        clean.write_text("[100%] Built target qore\n")
        report = self.source / "report.txt"
        args = ["--log-source-dir", SRC, "--source-dir", str(self.source), "--report", str(report)]
        with mock.patch("sys.stdout"):
            self.assertEqual(cbw.WARNINGS_FOUND, cbw.main(args + ["--log", f"ubuntu-amd64={with_warnings}",
                                                                  "--log", f"alpine-amd64={clean}"]))
        text = report.read_text()
        self.assertIn("ubuntu-amd64: lib/ql_debug.cpp:785 [-Wattributes]", text)
        self.assertNotIn("ngtcp2", text)
        self.assertNotIn("alpine-amd64", text)
        data = json.loads(report.with_suffix(".json").read_text())
        self.assertEqual([], data["alpine-amd64"])
        self.assertEqual(4, len(data["ubuntu-amd64"]))
        with mock.patch("sys.stdout"):
            self.assertEqual(0, cbw.main(args + ["--log", f"alpine-amd64={clean}"]))
            # a build that was skipped has no log: nothing to check
            self.assertEqual(0, cbw.main(args + ["--log", f"ubuntu-arm64={self.source / 'missing.log'}"]))
        with mock.patch("sys.stderr"):
            self.assertEqual(2, cbw.main(args))
            self.assertEqual(2, cbw.main(args + ["--log", "no-equals-sign"]))


class GitLab:
    """A stand-in GitLab API: the jobs of a pipeline and the build.log of their artifacts"""
    def __init__(self, jobs, logs, job_token_allowed=True):
        self.jobs = jobs
        self.logs = logs
        self.job_token_allowed = job_token_allowed
        self.requests = []
        api = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                api.requests.append((self.path, self.headers.get("JOB-TOKEN"), self.headers.get("PRIVATE-TOKEN")))
                if self.headers.get("JOB-TOKEN") and not api.job_token_allowed:
                    self.send_response(403)
                    self.end_headers()
                    return
                m = re.match(r"^/api/v4/projects/7/pipelines/99/jobs\?per_page=100&page=(\d+)$", self.path)
                if m:
                    body = json.dumps(api.jobs if m.group(1) == "1" else []).encode()
                    self.send_response(200)
                    self.end_headers()
                    self.wfile.write(body)
                    return
                m = re.match(r"^/api/v4/projects/7/jobs/(\d+)/artifacts/(.+)$", self.path)
                if m and (int(m.group(1)), m.group(2)) in api.logs:
                    self.send_response(200)
                    self.end_headers()
                    self.wfile.write(api.logs[(int(m.group(1)), m.group(2))].encode())
                    return
                self.send_response(404)
                self.end_headers()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def url(self):
        return f"http://127.0.0.1:{self.server.server_address[1]}/api/v4"

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class FetchTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-cbw-fetch-")
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)

    def run_fetch(self, api, env_extra=None):
        env = {"CI_API_V4_URL": api.url(), "CI_PROJECT_ID": "7", "CI_PIPELINE_ID": "99", "CI_JOB_TOKEN": "jt",
               "CI_PROJECT_DIR": SRC}
        env.update(env_extra or {})
        report = self.dir / "report.txt"
        with mock.patch.dict(os.environ, env, clear=False), mock.patch("sys.stdout"), mock.patch("sys.stderr"):
            return cbw.main(["--fetch", "--download-dir", str(self.dir / "logs"), "--report", str(report)]), report

    def test_fetch_reports_each_platform(self):
        jobs = [
            {"id": 1, "name": "build-ubuntu-amd64", "status": "success"},
            {"id": 2, "name": "build-ubuntu-arm64", "status": "success"},
            {"id": 3, "name": "build-alpine-amd64", "status": "failed"},
            # skipped by tree-tested-skip.sh: it succeeded without a log
            {"id": 4, "name": "build-alpine-arm64", "status": "success"},
            {"id": 5, "name": "test-ubuntu-amd64", "status": "success"},
            # the macOS job failed in its tests, after its build: its log is checked
            {"id": 6, "name": "test-macos", "status": "failed"},
            {"id": 7, "name": "check-build-separate-units", "status": "success"},
            # the gcc release build: a warning that only its optimizations find
            {"id": 8, "name": "build-perf", "status": "success"},
        ]
        logs = {(1, "build/build.log"): SAMPLE_LOG, (2, "build/build.log"): "[100%] Built target qore\n",
                (6, "build/build.log"): CLANG_LOG,
                (7, "build-separate-units/build.log"):
                    f"{SRC}/include/qore/intern/QoreHttp1ClientConnection.h:76:7: warning: 'Http1ClientConnection' "
                    "declared with greater visibility than the type of its field [-Wattributes]\n",
                (8, "build/build.log"):
                    f"qore-ci-build-log source-dir={SRC}\n"
                    f"{SRC}/lib/QuicPollOperations.cpp:648:9: warning: unused variable 'count' [-Wunused-variable]\n"}
        api = GitLab(jobs, logs)
        self.addCleanup(api.close)
        rc, report = self.run_fetch(api)
        self.assertEqual(cbw.WARNINGS_FOUND, rc)
        data = json.loads(report.with_suffix(".json").read_text())
        self.assertEqual({"ubuntu-amd64-debug", "ubuntu-arm64-debug", "macos-release", "ubuntu-amd64-non-scu-debug",
                          "ubuntu-amd64-release"}, set(data))
        self.assertEqual([], data["ubuntu-arm64-debug"])
        self.assertEqual(["lib/QoreLib.cpp", "include/qore/intern/QoreLibIntern.h"],
                         [w["file"] for w in data["macos-release"]])
        self.assertEqual(["include/qore/intern/QoreHttp1ClientConnection.h"],
                         [w["file"] for w in data["ubuntu-amd64-non-scu-debug"]])
        self.assertEqual([("lib/QuicPollOperations.cpp", 648, "-Wunused-variable")],
                         [(w["file"], w["line"], w["flag"]) for w in data["ubuntu-amd64-release"]])
        text = report.read_text()
        self.assertIn("ubuntu-amd64-release: lib/QuicPollOperations.cpp:648 [-Wunused-variable]", text)
        self.assertTrue(all(r[1] == "jt" for r in api.requests))

    def test_fetch_clean(self):
        api = GitLab([{"id": 1, "name": "build-ubuntu-amd64", "status": "success"}], {(1, "build/build.log"): "ok\n"})
        self.addCleanup(api.close)
        rc, _ = self.run_fetch(api)
        self.assertEqual(0, rc)

    def test_fetch_with_the_read_token(self):
        api = GitLab([{"id": 1, "name": "build-ubuntu-amd64", "status": "success"}],
                     {(1, "build/build.log"): SAMPLE_LOG}, job_token_allowed=False)
        self.addCleanup(api.close)
        rc, _ = self.run_fetch(api, {"QORE_CI_READ_API_TOKEN": "rt"})
        self.assertEqual(cbw.WARNINGS_FOUND, rc)
        self.assertTrue(any(r[2] == "rt" for r in api.requests))

    def test_fetch_failure_is_a_failure(self):
        api = GitLab([], {}, job_token_allowed=False)
        self.addCleanup(api.close)
        rc, _ = self.run_fetch(api)
        self.assertEqual(1, rc)


class CiConfigTest(unittest.TestCase):
    def test_builds_keep_their_logs(self):
        # every script that builds Qore keeps its compiler output where the checker reads it
        for script, log in (("build.sh", "build.log"), ("test-macos.sh", '"${BUILD_DIR}/build.log"'),
                            ("build-separate-units.sh", '"${BUILD_DIR}/build.log"'), ("build-perf.sh", "build.log")):
            text = (ROOT / "test/docker_test" / script).read_text()
            self.assertIn(f"qore_ci_build_logged {log} ", text, script)

    def test_labels_name_the_build_type(self):
        # each log is labelled with the platform and the build type that the job's script configures, and the job's
        # artifacts keep it
        text = CI.read_text()
        for label, (job, log) in cbw.BUILD_JOBS.items():
            # the job's lines: indented or empty, up to the next top-level line
            m = re.search(rf"^{re.escape(job)}:\n(?P<body>(?:(?: .*)?\n)+)", text, re.M)
            self.assertTrue(m, f"{job} is a job of the pipeline")
            body = m.group("body")
            # the script that builds (tree-tested-skip.sh only ends a job whose tree has already passed)
            scripts = sorted(set(re.findall(r"test/docker_test/([\w-]+\.sh)", body)) - {"tree-tested-skip.sh"})
            self.assertEqual(1, len(scripts), f"{job} builds with one script: {scripts}")
            script = (ROOT / "test/docker_test" / scripts[0]).read_text()
            types = {t.lower() for t in re.findall(r"-DCMAKE_BUILD_TYPE=(?:\$\{CMAKE_BUILD_TYPE:-)?(\w+)", script)}
            self.assertEqual(1, len(types), f"{scripts[0]} configures one build type: {types}")
            build_type = types.pop()
            self.assertIn(build_type, ("debug", "release"))
            self.assertTrue(label.endswith(f"-{build_type}"), f"{label}: {job} makes a {build_type} build")
            self.assertNotIn("CMAKE_BUILD_TYPE", body, f"{job} keeps the build type of its script")
            paths = re.search(r"^\s+artifacts:\n(?:\s+when:.*\n)?\s+paths:\n(?P<paths>(?:\s+- .*\n)+)", body, re.M)
            self.assertTrue(paths, f"{job} keeps artifacts")
            kept = [p.strip()[2:].strip() for p in paths.group("paths").splitlines()]
            self.assertTrue(any(log == k or log.startswith(k.rstrip("/") + "/") for k in kept),
                            f"{job} keeps {log} in its artifacts: {kept}")

    def test_the_gcc_release_build_is_checked(self):
        # warnings that only the optimizations of a release build find (qore#7757) are reported from build-perf
        self.assertEqual(("build-perf", "build/build.log"), cbw.BUILD_JOBS["ubuntu-amd64-release"])

    def test_logged_build_keeps_the_status(self):
        # the helper shows the output, keeps it with the source directory in its first line, and returns the
        # command's status
        with tempfile.TemporaryDirectory(prefix="qore-cbw-log-") as tmp:
            log = Path(tmp) / "build.log"
            for command, rc in (("echo compiling; exit 0", 0), ("echo failing; exit 7", 7)):
                result = subprocess.run(
                    ["sh", "-c", f'. "{ROOT}/test/docker_test/ci-build-log.sh"; '
                     f'qore_ci_build_logged "{log}" sh -c "{command}"; echo "rc=$?"'],
                    capture_output=True, text=True, env={**os.environ, "QORE_SRC_DIR": "/src/qore"})
                self.assertIn(f"rc={rc}", result.stdout)
                lines = log.read_text().splitlines()
                self.assertEqual("qore-ci-build-log source-dir=/src/qore", lines[0])
                self.assertIn(command.split(";")[0].split()[1], lines[1])

    def test_job_allows_only_the_warning_exit_code(self):
        text = CI.read_text()
        m = re.search(r"^check-build-warnings:\n(?P<body>(?:[ \n].*\n)+)", text, re.M)
        self.assertTrue(m, "the check-build-warnings job is defined")
        body = m.group("body")
        self.assertRegex(body, r"allow_failure:\s*\n\s+exit_codes:\s*\[3\]")
        self.assertIn("check-build-warnings.py --fetch", body)
        self.assertIn("tree-tested-skip.sh", body)
        for job, _ in cbw.BUILD_JOBS.values():
            self.assertRegex(body, rf"job: {job}\n\s+optional: true\n\s+artifacts: false")
        # the pipeline passes with the warning: pipeline-passed does not need the job
        passed = re.search(r"^pipeline-passed:\n(?P<body>(?:[ \n].*\n)+)", text, re.M).group("body")
        self.assertNotIn("check-build-warnings", passed)


if __name__ == "__main__":
    unittest.main()
