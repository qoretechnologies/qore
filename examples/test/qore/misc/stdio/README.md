# Standard descriptor startup regression tests

Copyright (C) 2026 Qore Technologies, s.r.o.

Build and run from the repository root:

```sh
cmake --build build --target qore qcc qore-stdio-startup-test -j4
QORE_BINARY="$PWD/build/qore" ./run_tests.sh -d qore/misc/stdio
```

The QUnit suite closes each combination of standard descriptors in a child process
and checks input, output, and an unrelated data file. The zero-mask control checks
that normal redirection still works. The V8 cases use a real script file, so module
initialization happens while that file is open; they skip if V8 is not installed.
Shell arguments are quoted and files are isolated in a temporary directory.
When `qcc` is available beside the interpreter, the suite also compiles the data
fixture with compiler stdin and stdout closed, then checks all eight masks with
the resulting AOT executable.

The native executable is built by default on POSIX and is run by the QUnit suite
when available beside the interpreter (or at `QORE_STDIO_NATIVE_TEST`). It covers
direct `qore_init()` embedding with all eight descriptor masks, preservation of
existing descriptors and flags, repeated helper calls, inheritance across exec,
interrupted syscalls, unexpected descriptor allocation, temporary descriptor cleanup,
and fatal startup failures. The fault-injection wrappers affect only the private
helper compiled into the test, leaving the linked libqore embedding tests intact.

The native parent never initializes libqore: each child starts a fresh runtime.
Children have a 30-second alarm and disable core dumps; checks remain active in
release builds. Windows requires separate native platform validation; the POSIX
shell and fork tests skip there.
