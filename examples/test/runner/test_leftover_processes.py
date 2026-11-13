#!/usr/bin/env python3
"""What run_tests.sh ends when a test has ended, and a test that needs podman without running it.

A test must end the processes it starts, and run_tests.sh fails and kills a test's leftovers.  It must not kill a
per-user helper that a container runtime started as a side effect of a command the test ran: rootless podman's first
command starts a pause process (catatonit -P) in a user namespace of its own, which every later container of the user
needs, and ending it broke all the rootless containers of the host (found on a host where RestClientPoolPerf.qtest ran
"podman image exists").  The helper is told by its user namespace, not by its name.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import textwrap
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import BUILD_DIR, HAVE_QORE, HAVE_TIMEOUT, ROOT, RunnerTree  # noqa: E402

PODMAN_TEST = ROOT / "examples/test/qlib/RestClient/RestClientPoolPerf.qtest"


def can_make_user_namespace():
    """Returns True if an unprivileged process can make a user namespace (some CI hosts block it)"""
    unshare = shutil.which("unshare")
    return bool(unshare) and subprocess.run([unshare, "-U", "--map-root-user", "true"],
                                            capture_output=True).returncode == 0


def alive(pid):
    """Returns True if the process is running (not ended, not a zombie)"""
    try:
        state = Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()[0]
    except OSError:
        return False
    return state not in ("Z", "X")


@unittest.skipUnless(HAVE_TIMEOUT and Path("/proc/self/ns/user").exists(), "Linux with a timeout command only")
class LeftoverProcessesTest(unittest.TestCase):
    """The processes a test leaves are ended; a per-user helper in a user namespace of its own is not"""

    def setUp(self):
        self.pids = []

    def tearDown(self):
        for pid in self.pids:
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass

    def test_user_helper_survives_and_leaked_child_is_ended(self):
        if not can_make_user_namespace():
            self.skipTest("user namespaces are not available")
        with tempfile.TemporaryDirectory(prefix="qore-runner-leftover-") as directory:
            tree = RunnerTree(directory)
            # a test that runs a command which starts a helper in a user namespace of its own (the helper has the
            # test's environment, so also its process mark, as podman's pause process has), and that leaks a child
            tree.add_test("leaks.qtest", f"""\
                import os
                import subprocess
                import sys
                import time
                helper = subprocess.Popen(["unshare", "-U", "--map-root-user", "sleep", "301"],
                                          start_new_session=True, stdin=subprocess.DEVNULL,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                leaked = subprocess.Popen(["sleep", "302"], start_new_session=True, stdin=subprocess.DEVNULL,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                # the helper has made its user namespace once its namespace differs from this process's: the runner
                # must not look for survivors before (no waiting by time: the child's exec is what changes it)
                mine = os.readlink("/proc/self/ns/user")
                deadline = time.monotonic() + 30
                while os.readlink("/proc/%d/ns/user" % helper.pid) == mine:
                    if helper.poll() is not None or time.monotonic() > deadline:
                        sys.exit("the helper did not make its user namespace")
                    os.sched_yield()
                with open({str(tree.root / "pids")!r}, "w") as f:
                    f.write("%d %d\\n" % (helper.pid, leaked.pid))
                """)
            result = tree.run()
            helper_pid, leaked_pid = (int(p) for p in (tree.root / "pids").read_text().split())
            self.pids.extend((helper_pid, leaked_pid))
            self.assertNotEqual(0, result.returncode, result.output)
            self.assertIn("*** FAILURE: the test left processes running; they are killed: ***", result.output)
            self.assertIn("sleep 302", result.output)
            self.assertNotIn("FAILURE: the test left processes running; they are killed: ***\n  pid %d" % helper_pid,
                             result.output)
            self.assertIn("not ending process %d" % helper_pid, result.output, "the skipped helper is not noted")
            self.assertTrue(alive(helper_pid), "the user's helper was killed by the runner\n" + result.output)
            # the leak is killed with SIGKILL before the runner returns (a zombie is not running)
            self.assertFalse(alive(leaked_pid), "the process the test leaked was not killed")

    def test_namespaced_leak_that_holds_the_output_of_the_test_is_reported(self):
        # a leak in a user namespace of its own that holds the test's output keeps a CI job from ending, so the
        # namespace does not excuse it (a parallel run: the output of a test is its log, which no other runner holds)
        if not can_make_user_namespace():
            self.skipTest("user namespaces are not available")
        with tempfile.TemporaryDirectory(prefix="qore-runner-leftover-") as directory:
            tree = RunnerTree(directory)
            tree.add_test("holds.qtest", f"""\
                import os
                import subprocess
                import sys
                import time
                # no redirection: the child keeps the test's output open
                p = subprocess.Popen(["unshare", "-U", "--map-root-user", "sleep", "305"], start_new_session=True,
                                     stdin=subprocess.DEVNULL)
                mine = os.readlink("/proc/self/ns/user")
                deadline = time.monotonic() + 30
                while os.readlink("/proc/%d/ns/user" % p.pid) == mine:
                    if p.poll() is not None or time.monotonic() > deadline:
                        sys.exit("the helper did not make its user namespace")
                    os.sched_yield()
                open({str(tree.root / "pid")!r}, "w").write(str(p.pid))
                """)
            tree.add_test("other.qtest", "print('other')\n")
            result = tree.run(QORE_TEST_JOBS="2")
            self.pids.append(int((tree.root / "pid").read_text()))
            self.assertNotEqual(0, result.returncode, result.output)
            self.assertIn("*** FAILURE: the test left processes running; they are killed: ***", result.output)
            self.assertIn("sleep 305", result.output)

    def test_detached_leak_in_the_runner_user_namespace_is_ended(self):
        # a leak that only calls setsid() (no namespace of its own) is still a leak
        with tempfile.TemporaryDirectory(prefix="qore-runner-leftover-") as directory:
            tree = RunnerTree(directory)
            tree.add_test("detaches.qtest", f"""\
                import subprocess
                p = subprocess.Popen(["sleep", "304"], start_new_session=True, stdin=subprocess.DEVNULL,
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                open({str(tree.root / "pid")!r}, "w").write(str(p.pid))
                """)
            result = tree.run()
            self.pids.append(int((tree.root / "pid").read_text()))
            self.assertNotEqual(0, result.returncode, result.output)
            self.assertIn("sleep 304", result.output)


def write_fake_podman(tree, log):
    """A podman that records its arguments in the log and runs nothing"""
    tree.write_script("podman", f"""\
        #!/bin/sh
        echo "$*" >> '{log}'
        case "$1" in
            run) exit 0 ;;
        esac
        exit 1
        """)


@unittest.skipUnless(shutil.which("qore") or HAVE_QORE, "needs qore")
class PodmanTestWithoutImageTest(unittest.TestCase):
    """RestClientPoolPerf.qtest skips without running podman when the image is not in a local store"""

    def run_qtest(self, home_setup=None):
        with tempfile.TemporaryDirectory(prefix="qore-runner-podman-") as directory:
            tree = RunnerTree(directory, qore=HAVE_QORE)
            log = tree.root / "podman.log"
            write_fake_podman(tree, log)
            home = tree.root / "home"
            home.mkdir()
            if home_setup:
                home_setup(home)
            # the test, with the modules of this tree (QORE_TEST_QLIB_DIR: another set that the qore in use can load)
            case = tree.root / "examples/test/qlib/RestClient"
            case.mkdir(parents=True)
            shutil.copy(PODMAN_TEST, case / PODMAN_TEST.name)
            (tree.root / "qlib").unlink()
            (tree.root / "qlib").symlink_to(os.environ.get("QORE_TEST_QLIB_DIR", str(ROOT / "qlib")))
            qore = str(BUILD_DIR / "qore") if HAVE_QORE else shutil.which("qore")
            env = tree.env(HOME=str(home), XDG_DATA_HOME="", QORE_PODMAN_SYSTEM_STORE=str(tree.root / "no-system-store"))
            if HAVE_QORE:
                env["LD_LIBRARY_PATH"] = str(BUILD_DIR)
            result = subprocess.run([qore, "--enable-debug", str(case / PODMAN_TEST.name)], env=env, cwd=case,
                                    capture_output=True, text=True, timeout=300)
            output = result.stdout + result.stderr
            return result, output, (log.read_text().splitlines() if log.exists() else [])

    def test_no_image_store_skips_without_running_podman(self):
        result, output, calls = self.run_qtest()
        self.assertEqual(0, result.returncode, output)
        self.assertIn("nginx container not available", output)
        self.assertNotIn("FAIL", output)
        self.assertEqual([], calls, "podman was run on a host without the image: " + output)

    def test_store_without_the_image_skips_without_running_podman(self):
        def other_image(home):
            store = home / ".local/share/containers/storage/overlay-images"
            store.mkdir(parents=True)
            (store / "images.json").write_text('[{"id":"abc","names":["docker.io/library/nginx:1.0"]}]')
        result, output, calls = self.run_qtest(other_image)
        self.assertEqual(0, result.returncode, output)
        self.assertEqual([], calls, output)

    def test_store_with_the_image_runs_podman(self):
        # the other side of the check: the image is there, so the test does start its container (here, a fake one that
        # starts nothing), and removes it again at the end
        def nginx(home):
            store = home / ".local/share/containers/storage/vfs-images"
            store.mkdir(parents=True)
            (store / "images.json").write_text('[{"id":"abc","names":["docker.io/library/nginx:alpine"]}]')
        result, output, calls = self.run_qtest(nginx)
        self.assertEqual(0, result.returncode, output)
        self.assertTrue(any(c.startswith("run -d") for c in calls), calls)
        self.assertTrue(any(c.startswith("rm -f") for c in calls), calls)
        self.assertFalse(any(c.startswith("image exists") for c in calls), calls)


if __name__ == "__main__":
    unittest.main()
