# Repository Guidelines

## Project Structure & Module Organization
- `include/` and `lib/` contain the core C++ headers and implementation of the Qore runtime.
- `qlib/` holds standard Qore modules (`.qm`, `.qc`) shipped with the language.
- `modules/` contains optional/binary modules (built separately from the core).
- `examples/` provides sample programs; tests live under `examples/test/` as `*.qtest`.
- `docs/` and `doxygen/` hold documentation assets.

## Architecture Overview
- The runtime, compiler, and core tooling are implemented in C++ with headers in `include/` and sources in `lib/`.
- Standard library modules are Qore sources in `qlib/`, loaded by the core at runtime.
- Optional integrations live in `modules/` (versioned separately).
- Parser/AST tooling lives in `modules/astparser/`; `qlib/Qdx.qm` and `doxygen/qdx` use it for docs.
- The `astparser` parser is a **tree-sitter grammar**: edit `modules/astparser/grammars/tree-sitter-qore/grammar.js`, then regenerate the committed `src/parser.c`/`src/grammar.json`/`src/node-types.json` with `npx tree-sitter-cli@0.26.13 generate` (version pinned in `package.json`; needs node, `nvm use v24`). CMake compiles the committed `parser.c` directly and never runs tree-sitter, so changes to language syntax accepted by the core parser (`lib/parser.ypp`, `lib/scanner.lpp`) must be reflected in `grammar.js` too, or `qdx`/docs builds will fail to parse them.
- The grammar's highlight query `modules/astparser/grammars/tree-sitter-qore/queries/highlights.scm` is compiled into astparser (`AstTree::getHighlights()`) and used by `qdx --post` to highlight Qore code in the HTML docs (`Qdx::QoreCodeFragmentHighlighter`); a grammar change that renames or removes node types, fields or tokens must update it, or the query fails to compile (`modules/astparser/test/highlights.qtest`). See `design/doc-code-highlighting.md`.

## Build, Test, and Development Commands
- `cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr` configures the recommended CMake build.
- `cmake --build build` builds the compiler, runtime, and tools.
- `cmake --build build --target docs` builds documentation (requires doxygen).
- `./run_tests.sh` runs Qore tests from `examples/test/` (expects a built `qore`/`libqore`).
- `./run_tests.sh -d <subdir>` runs a subset of tests, e.g. `./run_tests.sh -d http`.
- The tests of the binary modules (`modules/*/test/`) are not in `examples/test/`; run them by naming their
  directories in `QORE_TEST_EXTRA_DIRS`, e.g. `QORE_TEST_EXTRA_DIRS="modules/ml/test" ./run_tests.sh -d qore/misc`.
  CI runs every module's tests this way (`examples/test/docker_test/test-*.sh`).
- **After editing any `qlib/` `.qm`/`.qc` source, rebuild that module's qmod target**
  (`cmake --build build --target <Module>-qmod`) before testing. Each `qlib/<Module>.qmod` is a
  symlink to the AOT-compiled `build/qlib-qmod/` output and is preferred over the `.qm`/`.qc`
  sources, so source edits are silently ignored until the qmod is rebuilt — even with `qlib`
  prepended to the module path. Symptom: the edited code simply never runs (no error). The same
  applies to `build-debug/` when testing the debug build.

## Coding Style & Naming Conventions
- C++ code uses 4-space indentation and braces on the same line; follow nearby file style.
- File naming is consistent by type: `.cpp`/`.h` for C++, `.q`/`.qm`/`.qc` for Qore, `.qtest` for tests.
- No formatter is mandated; keep changes minimal and match local conventions.
- **Documentation tables use the Qore pipe format, never Markdown** — `|!Header|!Header` then
  `|cell|cell`, no trailing `|`, no `|---|---|` alignment row, literal pipes escaped as `\|`.
  A Markdown table renders as literal text in the generated HTML. See `design/doc-tables.md`;
  malformed tables fail the build (`QORE_DOX_TABLE_STRICT`, on by default in this repo).

## Release Notes
- The language release notes are `doxygen/lang/900_release_notes.dox.tmpl`; see `design/release-notes.md`.
- **Never add text directly after a `@section qore_X_Y_Z` line or after its `@par Release Summary`.** A new entry goes
  into the matching subsection of the newest section: `_compatibility` or `_new_features` as a `@par Title` block
  (`@parblock` for more than one paragraph), `_updated_modules`, `_new_builtin_modules`, `_new_user_modules` or
  `_bug_fixes` as a bullet. The summary lists only headline-level highlights.
- Entries are one-sentence summaries (at most four lines) that link (`@ref`) to the reference documentation, which
  holds the details; put any detail there first. Describe only changes relative to the last *release*: a fix to
  functionality that no release has shipped is not an entry.
- Never write a backslash before a double quote (`\\"`) in doxygen text: it breaks the markup that follows.
- `python3 -I doxygen/check-release-notes.py` checks the structure (CI and the `docs-lang*` targets run it); check the
  rendered page with `cmake --build build --target docs-lang-final-fast`.

## Testing Guidelines
- Primary tests are Qore scripts under `examples/test/` with the `*.qtest` suffix; a binary module's own tests are in
  `modules/<module>/test/`.
- Prefer adding tests alongside similar modules and keep naming descriptive.
- A test must end every process it starts (servers, helpers, children): on Linux, `run_tests.sh` fails a test that
  leaves processes running and kills them, as a leftover process keeps a CI job's output open and so the job running.
- Use `QORE_TEST_OPTS` and database env vars (for example `QORE_DB_CONNSTR`) when tests need external services.
- A test that uses `examples/test/ExecModeHarness.qm` declares `# test-exec-mode: children - <why>` when all its checks
  run in child processes that set their own `--exec-mode` (the CI jobs that run every test in one execution mode leave
  it out, as the default-mode jobs run the same children), or `# test-exec-mode: parent - <why>` when it also checks
  anything in its own process (code in its own `Program`, results it computes itself, its own options given to a
  child); `examples/test/runner/test_exec_mode_markers.py` enforces it.  Put in-process checks of a long harness test in
  a test of their own (ex: `qore/operators/hash-value-increment-exec-modes/hash-value-increment-in-process.qtest`).
- `QORE_TEST_JOBS=<n>|auto ./run_tests.sh` runs n tests at a time (the macOS CI job runs as many as the VM has CPUs) and
  prints each test's output whole when it ends.  Tests must therefore not share state: listen on port 0, write under
  their own `make_tmp_dir()`/`TmpDir`, and use names of their own for shared objects.  A test that cannot run beside
  others declares `# test-parallel: serial - <why>` (one at a time beside the others: the database and service tests,
  which share schemas and data) or `# test-parallel: exclusive - <why>` (alone: a test that reads the process limit
  of the machine to reach it, such as `qore/threads/max-threads-count.qtest`, must, or the tests beside it cannot fork;
  `examples/test/runner/test_parallel_runner.py` enforces it).  A test that takes more than about two
  minutes is split into parts: its class adds each case with `addPartCase(<part>, ...)` and each `<name>-<part>.qtest`
  runs one part (ex: `ir/CMakeBuildHelpers.qc`); `examples/test/runner/test_split_tests.py` checks the parts.
- The shards of a CI job are balanced by `examples/test/docker_test/test-durations.tsv`; refresh it from a green
  pipeline with `examples/test/docker_test/update-test-durations.py <pipeline ID>` after adding long tests or shards.

## Running Tests Under valgrind
- Disable signal handling with `-b` (`--disable-signals`), for example
  `QORE_PCRE2_NO_JIT=1 valgrind qore -b --exec-mode=ir --enable-debug <test>`.  Qore wakes its signal handling thread
  with `SIGSYS`, and valgrind treats `SIGSYS` as a fault signal: a wake-up that arrives while the signal thread is
  outside a system call is delivered with its default action, and the process dies with "Bad system call" (exit
  code 159; the valgrind log shows "Process terminating with default action of signal 31 (SIGSYS)").  This happens
  with or without a Qore signal handler, at any wake-up including the one at exit; seen with valgrind 3.25.1 (musl)
  and 3.26.0 (glibc).
- A test of signal handling skips under `-b` (`HAVE_SIGNAL_HANDLING` is `False`).  To check one under valgrind, use a
  verification build whose `QORE_STATUS_SIGNAL` (`include/qore/intern/QoreSignal.h`) is another signal, such as
  `SIGUSR2`; never commit that change.
- Observed, not root-caused: one valgrind 3.25.1 run of `signals.qtest` on musl without `-b` stalled, with both
  threads waiting on valgrind's internal pipe.
- `exit()` ends the process without the library's cleanup, and valgrind reports the memory still in use then as lost:
  a `QoreValue` holds its node pointer with a tag in the high 16 bits, which valgrind's leak scan does not recognize
  as a pointer.  A script whose leak report should mean something must end normally.

## Running Tests Under ThreadSanitizer
- Configure a build with `-fsanitize=thread` in the C, C++ and linker flags (a Debug build in its own directory, for
  example `build-tsan/`), build the `qore` target, and run with
  `LD_LIBRARY_PATH=build-tsan TSAN_OPTIONS="suppressions=test/cpp/tsan.supp die_after_fork=0 report_thread_leaks=0 log_path=<file>" build-tsan/qore -b ...`
  (`QORE_BINARY=build-tsan/qore ./run_tests.sh -d <subdir>` with the same environment).  Build the `qcc` target
  too: the tests that compile a program use the `qcc` next to `qore`, and fall back to an installed one, which crashes
  with a TSan libqore.
- `die_after_fork=0` is needed because TSan ends a forked child that starts a thread (the fork tests in
  `AsyncIoController/asyncio_unit_tests.qtest` do); TSan then exits with code 66 in the child, which a test reports as
  a failure.  `report_thread_leaks=0` is needed because a thread that the parent joined looks unjoined in the child.
- `test/cpp/tsan.supp` suppresses the reports in the libraries that are not built with TSan and that libqore loads
  (OpenSSL, whose atomic reference counts TSan cannot see; the JVM and Node.js/V8 of the jni and JavaScript modules).
  A report in the code of libqore or of this repository's modules is never suppressed there; fix it.
- A TSan report makes the process exit with code 66, so a test whose output has no failure can still fail under TSan.
- Some tests cannot run under TSan whatever the code does, and fail without a report from it: `misc/hashdecl.qtest` and
  `misc/deep-release.qtest` recurse past TSan's limit of 64K nested calls (it crashes in `__tsan::CurrentStackId`);
  `misc/stdio/stdio-startup.qtest` starts V8, which TSan cannot run; `misc/test-listener-addresses.qtest` and
  `misc/module-loader/concurrent-module-program-merge.qtest` do so much work in Qore code that they exceed the 300
  second limit of `run_tests.sh` at TSan's slowdown.

## Commit & Pull Request Guidelines
- Commit messages use a short type prefix such as `fix:`, `feat:`, or `refactor:` in a present-tense summary.
- Use `Revert` for rollbacks and `Merge` when bringing in upstream branches.
- Keep commits focused; include build/test commands run in the body when relevant.
- PRs should include a concise description, linked issues, and any required setup notes.
- Add screenshots or logs when changing CLI output.

## Security & Configuration Tips
- Avoid committing credentials; prefer environment variables for connection strings.
