# Atomic Link Outputs

## Status

Implemented in `cmake/QoreMacros.cmake` (`QORE_ATOMIC_LINK_RULES()`), tested by
`examples/test/cmake/test_atomic_link.py`.

## Invariant

Every executable, shared library and module (`add_library(MODULE)`, i.e. every
binary `.qmod`) built by Qore, by a bundled module, or by an external module
that loads Qore's CMake macros is linked to `<target>.partial` and renamed over
`<target>` only after the link succeeds. The target path therefore always holds
either the previous complete output or the new complete output.

## Why

GNU BFD ld truncates its output in place (unlink, then `O_CREAT|O_TRUNC`) and
writes it as the link proceeds. If make is killed with `SIGKILL` mid-link (the
OOM killer, a CI timeout, `kill -9` of the process group), it cannot apply
`.DELETE_ON_ERROR`, which only covers the signals make catches. The killed link
left a short or empty output that was newer than its inputs, so the next
incremental make treated it as up to date. Qorus installed a 0-byte `qwf` this
way. With the rename, a killed link leaves at most a `.partial` file, and the
target stays older than its objects, so the next make relinks it.

## Mechanism

`QORE_ATOMIC_LINK_RULES()` rewrites `CMAKE_<LANG>_LINK_EXECUTABLE`,
`CMAKE_<LANG>_CREATE_SHARED_LIBRARY` and `CMAKE_<LANG>_CREATE_SHARED_MODULE` for
C and C++:

- CMake's own rule for the platform is kept; only its `-o <TARGET>` operand
  becomes `-o <TARGET>.partial`. A rule without `-o <TARGET>`, or with more than
  one command, fails the configure instead of silently linking in place.
- The rename is a second element of the rule's command list
  (`cmake -E rename <TARGET>.partial <TARGET>`), never a string joined with
  `&&`: the Makefile generator runs `link.txt` through
  `cmake -E cmake_link_script` without a shell, one command per list element.
  Ninja joins the elements with `&&` itself.
- CMake runs the rule's commands before the versioned-library symlinks and any
  `POST_BUILD` commands, so both see the renamed output. The SONAME and the
  macOS install name do not depend on the output path.
- A linker dependency file (`-Wl,--dependency-file`, used by CMake 3.27+ with
  GNU ld 2.35+ or lld) names `<target>.partial` as its output. That is
  harmless: the Makefile generator records the dependencies for the target
  itself, and Ninja stores them in its log for the declared output.
- The marker `-o <TARGET>.partial` makes the macro idempotent: a rule that
  already contains it is left alone. Qorus's `cmake/QorusLinker.cmake` uses the
  same marker, so the two never stack.

## Where It Applies

- Qore's own build: the top-level `CMakeLists.txt` includes `QoreMacros.cmake`
  after `project()` and before every `add_subdirectory()`, so libqore, `qore`,
  `qpp`, `qcc`, the test executables, the bundled modules and the fetched
  third-party projects inherit the rules.
- External modules: `QoreConfig.cmake` includes `QoreMacros.cmake` during
  `find_package(Qore)`, in the caller's scope after its `project()`. The code is
  a macro run at file scope, not a function, because the rule variables are
  directory-scoped and a function's `set()` would not reach the caller.
- A language enabled after the macros are loaded keeps CMake's in-place rule
  unless the project calls `QORE_ATOMIC_LINK_RULES()` again. MSVC-style
  compilers, whose rules have no `-o`, are not changed.

Not covered, because they already write a temporary file and rename it: the
outputs of `qcc` (`.qo`, `.qoa` and AOT `.qmod`) and `install(TARGETS)`. Static
libraries are archived by multi-command rules and are not changed.

## Tests

`test_atomic_link.py` builds a toy project with an executable, a versioned
shared library and a `MODULE` in directories with spaces, and checks that:

- each `link.txt` links to `.partial` and renames it, exactly once, also when
  the macros are loaded twice or through `find_package(Qore)` from an installed
  configuration; the Ninja rules do the same;
- a build whose process group is killed with `SIGKILL` after a link and before
  its rename keeps the previous target and leaves only that `.partial`, and the
  next build relinks it;
- a dependency recorded only in the linker's dependency file still relinks the
  target, with both generators;
- a rule without `-o <TARGET>` or with several commands fails the configure,
  and an already applied rule is kept;
- with `QORE_TEST_BUILD_DIR` set, every linked target of the Qore build tree,
  including libqore, `qore`, `qpp`, `qcc` and the bundled modules, uses the
  rule.
