# Compile Race Guard

## Status

Implemented in `tools/compile-race-guard.py` and `cmake/QoreCompileRaceGuard.cmake`
(`qore_compile_race_guard()`), applied to `libqore` and `qore-ir-phase0-smoke` by the
top-level `CMakeLists.txt`, controlled by the option `QORE_COMPILE_RACE_GUARD` (ON by
default); tested by `examples/test/cmake/test_compile_race_guard.py`. Issue #6951.

## Invariant

When an input of an object (the source, an `#include`d source or header) is modified
while the object compiles, the next build compiles the object again.

## Why

make and Ninja rebuild an object when an input is newer than the object. A compiler
reads its inputs at the start and writes the object at the end, so an input edited in
between is older than the object, although the object holds the code from before the
edit: the build treats it as up to date for good, until the input is touched again.
With `SINGLE_COMPILATION_UNIT`, one object includes most of libqore's sources and
compiles for minutes (each `SINGLE_COMPILATION_UNIT_CHUNKS` chunk includes many as
well), so an edit made during a build is easily lost: in #6920 an edit of
`lib/QoreIRVerifier.cpp` made during the compile never reached libqore. The race
exists for every object, but the window of a per-file object is seconds.

The SCU wrapper's `OBJECT_DEPENDS` (every `#include`d source) and the compiler's
dependency file make the build *see* the inputs; they cannot help when the
timestamps are in the wrong order.

## Mechanism

`qore_compile_race_guard(<target>...)` sets the targets' `C_COMPILER_LAUNCHER` and
`CXX_COMPILER_LAUNCHER` to `python3 -I tools/compile-race-guard.py`, followed by the
launcher the target had (`CMAKE_<LANG>_COMPILER_LAUNCHER`, ex: ccache), which therefore
runs inside it. Compiler launchers are supported by the Makefile and Ninja generators.
For each compile with `-o <object>`, the launcher:

1. takes the file system's time `T0` (the modification time of a temporary file it
   writes and removes in the object's directory: the clock and granularity that an
   edit gets, not the process clock);
2. runs the compile, passing its output and exit status through; a signal that the
   launcher receives is passed on to the compiler, and the launcher then ends as the
   compiler does;
3. after a successful compile, when any input named in the compiler's dependency
   file (`-MF`; CMake passes `-MD -MT <object> -MF <object>.d` to GCC and Clang with
   both generators from CMake 3.20; without one, the `-c` source) has a modification
   time at or after `T0`, or no longer exists, sets the object's modification time to
   the last nanosecond of the second before `T0`.

The object is then older than the edited input, also for a tool that compares whole
seconds, and the next build compiles it again. An object whose inputs did not change
keeps its time, so a build that edits nothing is unaffected. An input written in the
same file-system clock tick just before `T0` is counted as edited (`>=`): that costs
at most one extra compile, while `>` could miss an edit made in the tick of `T0`.

The backdated object may be older than the output linked from it in an earlier build;
then the build that compiled it does not link again, but the next build compiles and
links. That build's output was stale in any case, as the edit came during it.

ccache: an edit during the compile changes the input's content, so the next compile
is a cache miss and compiles the new content.

## Cost and Opt-Out

One Python start and a `stat()` of each depfile input per guarded object: about 20 ms
per compile (an empty C++ file: 23 ms directly, 44 ms through the launcher), against
compiles of seconds to minutes; with `SINGLE_COMPILATION_UNIT`, libqore has a few
objects (a full libqore SCU compile took 439 s with and 447 s without it, which is
noise). A build that compiles nothing runs no launcher (1.37 s either way).

`-DQORE_COMPILE_RACE_GUARD=OFF` removes the launcher; `QORE_COMPILE_RACE_GUARD=0` in
the environment makes it run the compile directly without reconfiguring. Without a
Python 3 interpreter, and on Windows, the guard is not set.

## Not Covered

Custom commands (qpp, bison, flex, the qmod builds) are not compiler launches. A
`.qpp` edited while qpp generates from it has the same race, but qpp runs for a
fraction of a second; an edit of a `.qpp` during the C++ compile is caught, as the
regenerated `.cpp` is newer than the object.
