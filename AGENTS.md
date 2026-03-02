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
- When editing the core parser (`lib/parser.ypp`, `lib/scanner.lpp`), mirror changes in `modules/astparser/src/`.

## Build, Test, and Development Commands
- `cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr` configures the recommended CMake build.
- `cmake --build build` builds the compiler, runtime, and tools.
- `cmake --build build --target docs` builds documentation (requires doxygen).
- `./run_tests.sh` runs Qore tests from `examples/test/` (expects a built `qore`/`libqore`).
- `./run_tests.sh -d <subdir>` runs a subset of tests, e.g. `./run_tests.sh -d http`.

## Coding Style & Naming Conventions
- C++ code uses 4-space indentation and braces on the same line; follow nearby file style.
- Qore sources typically opt into `%new-style`; many files set `indent-tabs-mode: nil`.
- File naming is consistent by type: `.cpp`/`.h` for C++, `.q`/`.qm`/`.qc` for Qore, `.qtest` for tests.
- No formatter is mandated; keep changes minimal and match local conventions.

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
