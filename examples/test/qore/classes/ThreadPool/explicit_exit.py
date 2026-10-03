#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Bounded subprocess regressions for Qore explicit exit and native cleanup."""

import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qore", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--repeat", type=int, default=1)
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error("--repeat must be positive")
    qore = str(args.qore.resolve(strict=True))
    native = str(args.native.resolve(strict=True))
    cases = []

    # Existing normal qore_cleanup() controls, including a never-started reaper.
    for mode in ([], ["--empty"]):
        cases.append(("native cleanup " + str(mode), [native, *mode], 0,
                      "Passed native thread cleanup checks\n"))
    cases.append(("native return with idle reaper", [native, "--return-with-idle-reaper"], 0,
                  "Returning without qore_cleanup\n"))
    for mode in ("empty", "idle", "active", "tls", "tls-custom", "signal", "signal-tls"):
        # The idle cases must flush C stdio and run atexit callbacks. Active
        # workers, native TLS, and signal-handler exits must bypass both.
        expected = "buffered output\natexit\n" if mode in ("empty", "idle") else ""
        cases.append(("native exit " + mode, [native, "--exit-" + mode], 17, expected))

    for mode in ("empty", "handler", "blocked", "replaced", "native-blocked"):
        expected = "buffered output\nsignal joined\n" if mode in ("empty", "handler") else ""
        cases.append(("managed signal exit " + mode, [native, "--exit-managed-" + mode], 17, expected))

    stopped = """
pool.stopWait();
delete pool;
while (num_threads() > 1) { usleep(1000); }
"""
    active = """
Counter ready(1);
Counter release(1);
ThreadPool pool();
pool.submit(sub() { ready.dec(); release.waitForZero(); });
ready.waitForZero();
"""
    signal_exit = """
set_signal_handler(SIGUSR1, sub(int sig) { exit(17); });
kill(getpid(), SIGUSR1);
while (True) { usleep(1000); }
"""
    setups = {
        "no pool": "",
        "empty stopped pool": "ThreadPool pool();" + stopped,
        "stopped pool with work": "ThreadPool pool(); pool.submit(sub() {});" + stopped,
    }
    for name, setup in setups.items():
        for finish, status in (("return 17;", 17), ("exit(17);", 17), ("exit(0);", 0), ("exit(255);", 255)):
            source = "%modern\n" + setup + 'print("pool output");' + finish
            cases.append((name + " " + finish, [qore, "--enable-debug", "-e", source], status, "pool output"))
    for name, setup in (("active pool", active), ("signal after stopped pool", setups["empty stopped pool"]),
                        ("signal with active pool", active)):
        source = "%modern\n" + setup + (signal_exit if name.startswith("signal") else "exit(17);")
        cases.append((name, [qore, "--enable-debug", "-e", source], 17, ""))

    for iteration in range(args.repeat):
        for name, command, status, output in cases:
            # Select each executable's build library, including when another
            # Qore release is installed. Preserve additional module library paths.
            env = os.environ.copy()
            library_path = "DYLD_LIBRARY_PATH" if os.uname().sysname == "Darwin" else "LD_LIBRARY_PATH"
            env[library_path] = os.pathsep.join(filter(None, (
                str(Path(command[0]).parent), env.get(library_path))))
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=5, env=env)
            except subprocess.TimeoutExpired as ex:
                raise AssertionError(
                    f"{name} timed out (iteration {iteration + 1}): {ex.stdout!r}, {ex.stderr!r}") from ex
            if result.returncode != status or result.stdout != output:
                raise AssertionError(
                    f"{name} (iteration {iteration + 1}): expected status {status}, stdout {output!r}; "
                    f"got status {result.returncode}, stdout {result.stdout!r}, stderr {result.stderr!r}")
    print(f"Passed {len(cases) * args.repeat} explicit-exit and normal-cleanup subprocess checks")


if __name__ == "__main__":
    main()
