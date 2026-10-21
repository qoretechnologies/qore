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

## Testing Guidelines
- Primary tests are Qore scripts under `examples/test/` with the `*.qtest` suffix; a binary module's own tests are in
  `modules/<module>/test/`.
- Prefer adding tests alongside similar modules and keep naming descriptive.
- A test must end every process it starts (servers, helpers, children): on Linux, `run_tests.sh` fails a test that
  leaves processes running and kills them, as a leftover process keeps a CI job's output open and so the job running.
- Use `QORE_TEST_OPTS` and database env vars (for example `QORE_DB_CONNSTR`) when tests need external services.

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

## Commit & Pull Request Guidelines
- Commit messages use a short type prefix such as `fix:`, `feat:`, or `refactor:` in a present-tense summary.
- Use `Revert` for rollbacks and `Merge` when bringing in upstream branches.
- Keep commits focused; include build/test commands run in the body when relevant.
- PRs should include a concise description, linked issues, and any required setup notes.
- Add screenshots or logs when changing CLI output.

## Security & Configuration Tips
- Avoid committing credentials; prefer environment variables for connection strings.
