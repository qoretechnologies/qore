#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Exercise real Linux signal denial in an isolated child, including reload and shutdown.

Run with --qore build/qore against Release and Debug builds. Requires a C compiler;
seccomp is installed only in the child and never changes the caller or host policy.
"""
import argparse
import os
from pathlib import Path
import platform
import shlex
import signal
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qore", type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != "Linux":
        print("SKIP: Linux seccomp is required for this denial regression")
        return
    qore = args.qore.resolve(strict=True)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = os.pathsep.join(filter(None, (str(qore.parent), env.get("LD_LIBRARY_PATH"))))
    cases = {
        "normal cleanup": 'print("complete");',
        "explicit exit": 'set_signal_handler(SIGUSR1, sub(int sig) {}); print("complete"); exit(0);',
        "reload, delivery and removal": '''
Queue received();
set_signal_handler(SIGUSR1, sub(int sig) { received.push(sig); });
kill(getpid(), SIGUSR1);
if (received.get(2s) != SIGUSR1) { throw "TEST-ERROR", "signal not delivered"; }
remove_signal_handler(SIGUSR1);
print("complete");''',
        "concurrent handler reloads": '''
Counter start(1); Counter done(4);
foreach int sig in (SIGUSR1, SIGUSR2, SIGWINCH, SIGURG) {
    background sub(int signal_number) {
        start.waitForZero();
        set_signal_handler(signal_number, sub(int received) {});
        remove_signal_handler(signal_number);
        done.dec();
    }(sig);
}
start.dec(); done.waitForZero(); print("complete");''',
        "fork and resumed signal thread": '''
if (backquote("printf child") != "child") { throw "TEST-ERROR", "child did not finish"; }
Queue received();
set_signal_handler(SIGUSR1, sub(int sig) { received.push(sig); });
kill(getpid(), SIGUSR1);
if (received.get(2s) != SIGUSR1) { throw "TEST-ERROR", "signal not delivered after fork"; }
remove_signal_handler(SIGUSR1);
print("complete");''',
    }
    with tempfile.TemporaryDirectory(prefix="qore-denied-wakeup-") as tmp:
        launcher = Path(tmp) / "deny-status-signal"
        source = Path(__file__).with_name("deny-status-signal.c")
        subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-Wall", "-Wextra", "-Werror", "-O2",
                        str(source), "-o", str(launcher)], check=True, timeout=30)
        for denied in (False, True):
            for name, source in cases.items():
                command = ([str(launcher)] if denied else []) + [str(qore), "-e", "%modern\n" + source]
                # Own a session so timeout cleanup also removes any forked test child.
                child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True, env=env, start_new_session=True)
                try:
                    stdout, stderr = child.communicate(timeout=8)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.communicate(timeout=5)
                    raise AssertionError(f"{name}, denied={denied}: timed out") from None
                if child.returncode or stdout != "complete":
                    raise AssertionError(f"{name}, denied={denied}: {child.returncode}, {stdout!r}, {stderr!r}")
                if denied != ("internal wakeup failed" in stderr):
                    raise AssertionError(f"{name}: denial was not observed as expected: {stderr!r}")
                print(f"PASS {name}, denied={denied}")
    print(f"Passed all {2 * len(cases)} signal wakeup checks; temporary files and processes removed")


if __name__ == "__main__":
    main()
