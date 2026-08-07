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
- The `astparser` parser is a **tree-sitter grammar**: edit `modules/astparser/grammars/tree-sitter-qore/grammar.js`, then regenerate the committed `src/parser.c`/`src/grammar.json`/`src/node-types.json` with `npx tree-sitter-cli@0.26.8 generate` (version pinned in `package.json`; needs node, `nvm use v24`). CMake compiles the committed `parser.c` directly and never runs tree-sitter, so changes to language syntax accepted by the core parser (`lib/parser.ypp`, `lib/scanner.lpp`) must be reflected in `grammar.js` too, or `qdx`/docs builds will fail to parse them.

## Build, Test, and Development Commands
- `cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr` configures the recommended CMake build.
- `cmake --build build` builds the compiler, runtime, and tools.
- `cmake --build build --target docs` builds documentation (requires doxygen).
- `./run_tests.sh` runs Qore tests from `examples/test/` (expects a built `qore`/`libqore`).
- `./run_tests.sh -d <subdir>` runs a subset of tests, e.g. `./run_tests.sh -d http`.
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
- Primary tests are Qore scripts under `examples/test/` with the `*.qtest` suffix.
- Prefer adding tests alongside similar modules and keep naming descriptive.
- Use `QORE_TEST_OPTS` and database env vars (for example `QORE_DB_CONNSTR`) when tests need external services.

## Commit & Pull Request Guidelines
- Commit messages use a short type prefix such as `fix:`, `feat:`, or `refactor:` in a present-tense summary.
- Use `Revert` for rollbacks and `Merge` when bringing in upstream branches.
- Keep commits focused; include build/test commands run in the body when relevant.
- PRs should include a concise description, linked issues, and any required setup notes.
- Add screenshots or logs when changing CLI output.

## Security & Configuration Tips
- Avoid committing credentials; prefer environment variables for connection strings.
