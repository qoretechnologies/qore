#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Compiler launcher: make an object compiled from an input edited during its compile out of date.

usage: compile-race-guard.py <compiler or launcher> [args...]

make and Ninja rebuild an object when an input is newer than the object.  A compiler reads its inputs at the start
and writes the object at the end, so an input edited in between is older than the object although the object holds
the code from before the edit, and the edit is never compiled (issue #6951; a single-compilation-unit object of
libqore compiles for minutes).  This launcher takes the file system's time just before the compile; after a
successful compile, when any input named in the compiler's dependency file (-MF <file>; without one, the -c source)
has a modification time at or after that time, it sets the object's modification time to before the second of that
time, so the next build compiles it again.  It changes nothing else: the command's output and exit status are the
compiler's.  See design/compile-race-guard.md.
"""

import os
import signal
import sys

SECOND_NS = 1_000_000_000


def backdated(start):
    """The last nanosecond of the second before the one of start: older than any time at or after start, also for a
    tool that compares whole seconds."""
    return start // SECOND_NS * SECOND_NS - 1


def option_value(args, flag, joined=True):
    """The value of the last "<flag> <value>" (or "<flag><value>" when joined) of the command line, or None."""
    value = None
    for i, arg in enumerate(args):
        if arg == flag:
            if i + 1 < len(args):
                value = args[i + 1]
        elif joined and arg.startswith(flag) and len(arg) > len(flag):
            value = arg[len(flag):]
    return value


def depfile_inputs(path):
    """The prerequisites of a make-syntax dependency file (the compiler's -MD output)."""
    with open(path, encoding="utf-8", errors="surrogateescape") as f:
        text = f.read()
    text = text.replace("\\\r\n", " ").replace("\\\n", " ")
    inputs = []
    for line in text.splitlines():
        # the prerequisites follow the first unescaped ":" that ends a word
        words, word, colon, i = [], [], False, 0
        while i <= len(line):
            c = line[i] if i < len(line) else " "
            if c == "\\" and i + 1 < len(line) and line[i + 1] in " #\\":
                word.append(line[i + 1])
                i += 2
                continue
            if c == "$" and i + 1 < len(line) and line[i + 1] == "$":
                word.append("$")
                i += 2
                continue
            if not colon and c == ":" and (i + 1 == len(line) or line[i + 1] in " \t"):
                colon, word = True, []
            elif c in " \t":
                if word and colon:
                    words.append("".join(word))
                word = []
            else:
                word.append(c)
            i += 1
        inputs.extend(words)
    return inputs


def start_time(directory):
    """The file system's current time (what a write now would set), in nanoseconds."""
    path = os.path.join(directory, ".compile-race-guard-%d" % os.getpid())
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    try:
        os.write(fd, b"\n")
        return os.fstat(fd).st_mtime_ns
    finally:
        os.close(fd)
        os.unlink(path)


def edited_during(inputs, start):
    for path in inputs:
        try:
            if os.stat(path).st_mtime_ns >= start:
                return True
        except OSError:
            # a removed or renamed input: compile again rather than trust the object
            return True
    return False


def main(argv):
    if not argv:
        sys.stderr.write("usage: compile-race-guard.py <compiler> [args...]\n")
        return 2
    obj = option_value(argv, "-o")
    if obj is None or os.environ.get("QORE_COMPILE_RACE_GUARD") == "0":
        os.execvp(argv[0], argv)
    try:
        start = start_time(os.path.dirname(obj) or ".")
    except OSError:
        os.execvp(argv[0], argv)

    # a signal is passed on to the compiler (which also gets those sent to the build's process group); the launcher
    # waits for it, then ends as it does
    received = []
    child = []

    def forward(sig, frame):
        received.append(sig)
        if child:
            try:
                os.kill(child[0], sig)
            except OSError:
                pass

    signals = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)
    for sig in signals:
        signal.signal(sig, forward)
    pid = os.fork()
    if pid == 0:
        for sig in signals:
            signal.signal(sig, signal.SIG_DFL)
        try:
            os.execvp(argv[0], argv)
        except OSError as e:
            sys.stderr.write("compile-race-guard: %s: %s\n" % (argv[0], e.strerror))
        os._exit(127)
    child.append(pid)
    if received:
        os.kill(pid, received[0])
    wstatus = os.waitpid(pid, 0)[1]
    status = -os.WTERMSIG(wstatus) if os.WIFSIGNALED(wstatus) else os.WEXITSTATUS(wstatus)

    if status == 0 and os.path.exists(obj):
        # the inputs: those of the dependency file, else the source alone
        depfile = option_value(argv, "-MF")
        try:
            if depfile is not None:
                inputs = depfile_inputs(depfile)
            else:
                source = option_value(argv, "-c", joined=False)
                inputs = [source] if source is not None else []
            stale = edited_during(inputs, start)
        except (OSError, ValueError):
            stale = True
        if stale:
            os.utime(obj, ns=(backdated(start), backdated(start)))

    sig = -status if status < 0 else (received[0] if received else 0)
    if sig:
        signal.signal(sig, signal.SIG_DFL)
        os.kill(os.getpid(), sig)
        return 128 + sig
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
