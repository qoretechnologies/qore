# Qore Module Structure and Conventions

/*  qore-module-structure.md Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

This document clarifies how Qore user modules are structured in this repository.
Follow these rules to avoid load and documentation issues.

## Module Layout

There are two supported layouts. Choose one and keep it consistent.

### 1) Directory Modules (preferred for multi-file modules)

Use when a module has multiple implementation files or assets.

Layout:

- `qlib/<ModuleName>/`
  - `<ModuleName>.qm`
  - `*.qc`, `*.qpp`, assets, schemas

Examples:

- `qlib/ConnectionProvider/ConnectionProvider.qm`
- `qlib/AsyncApiDataProvider/AsyncApiDataProvider.qm`

Rules:

- The `.qm` file must live inside the module directory.
- Do not place a second `.qm` for the same module at `qlib/<ModuleName>.qm`.
- The module is registered using the directory path in build config:
  - `qore_user_module("qlib/<ModuleName>" "...")`
- Include the module in split-doc lists:
  - `DOX_SRC_SPLIT_MODULES` and/or `DOX_SPLIT_MODULES`
- Add packaging for assets if needed:
  - `dist_<ModuleName>_modver_DATA = $(wildcard qlib/<ModuleName>/*)`

### 2) Single-File Modules (for small modules)

Use when a module is defined in a single `.qm` file.

Layout:

- `qlib/<ModuleName>.qm`

Rules:

- Register with `qore_user_module("qlib/<ModuleName>.qm" "...")`.
- Add to the flat module list in `Makefile.am`.

## Documentation and Dependencies

- If a module references another module in docs, keep doc tags in sync.
- Keep dependencies visible in the module `.qm` file:
  - Use `%requires` for hard deps.
  - Use `%try-module` only for optional deps.
- Avoid circular load paths where possible; move optional services into a separate module.

## %include Deprecation (Modules)

The `%include` parse directive is deprecated for Qore user modules in this
repository. It should not be used in new modules, and existing uses should be
migrated.

Preferred alternatives:

- Use directory modules and rely on Qore's module loading to bring in all
  module files from the module directory.
- Split optional features into separate modules rather than `%include`ing extra
  files conditionally.

Current usages to migrate:

- `qlib/QoreRepl.qm` (includes core and WebSocket implementation files)
- `qlib/QUnit.qm` (commented examples only)

## Tests

- New modules must have tests under:
  - `examples/test/qlib/<ModuleName>/`
- Use `%try-module` patterns in tests to allow skipping when optional modules are not available.

## Checklist

- [ ] Correct layout chosen (directory vs single-file)
- [ ] `.qm` location matches layout
- [ ] all parse directives in the main .qm file for separated modules
- [ ] `%modern` used / redundant parse directives (`%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`) removed
- [ ] `CMakeLists.txt` module registration updated
- [ ] `Makefile.am` module lists updated
- [ ] Docs module list (`doxygen/lang/120_modules.dox.tmpl`) updated when applicable
- [ ] entry in `doxygen/lang/900_release_notes.dox.tmpl` for new modules or updates
- [ ] Tests added/updated for the module
- [ ] all scripts have the execute bit set
- [ ] all tests and scripts use `%modern`
