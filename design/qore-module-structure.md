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
- Register with `qore_user_module()` in `CMakeLists.txt`.

## Documentation and Dependencies

- If a module references another module in docs, keep doc tags in sync.
- Keep dependencies visible in the module `.qm` file:
  - Use `%requires` for hard deps.
  - Use `%try-module` only for optional deps.
- Avoid circular load paths where possible; move optional services into a separate module.

## Module Documentation

### Intro Section Naming Convention

Every module's `@mainpage` must start with a section using the naming convention
`@section <lowercasemodname>intro <ModuleName> Module Introduction`, where the section
ID is the module name in all lowercase concatenated with `intro` (no underscores, no
hyphens).

Examples:
- `@section swaggerintro Swagger Module Introduction`
- `@section httpserverintro HttpServer Module Introduction`
- `@section dataproviderintro DataProvider Module Introduction`
- `@section ncursesuiintro NcursesUi Module Introduction`

This convention enables cross-referencing via `@ref <modname>intro "display text"` from
other documentation. In the qore lang docs, these section IDs are referenced from the
module list in `doxygen/lang/120_modules.dox.tmpl` and release notes.

### Two-Phase Doc Build for External Modules with User Sub-Modules

When an external binary module includes user sub-modules (e.g., ncurses includes
NcursesUi and NcursesReplUi), the binary module docs should be built twice so that the
binary module's mainpage can `@ref` into user module symbols:

1. **Initial pass** (`docs-module`): generates the binary module's tag file (e.g.,
   `ncurses.tag`) with empty `TAGFILES` and `WARN_IF_DOC_ERROR = NO` to suppress
   unresolved cross-reference warnings.
2. **User module builds** (`docs-<ModuleName>`): each generates its own tag file,
   referencing the binary module's tag file for cross-references back to binary module
   symbols.
3. **Final pass** (`docs-module-final`): rebuilds binary module docs with user module
   tag files in `TAGFILES`, enabling `@ref` cross-references from the mainpage into
   user module symbols.

Implementation pattern (in `CMakeLists.txt`):
```cmake
# Suppress warnings in initial pass
file(APPEND ${CMAKE_BINARY_DIR}/Doxyfile
    "\nWARN_IF_DOC_ERROR = NO\n")

# Configure final-pass Doxyfile with user module tag files
set(TAGFILES "\"${CMAKE_BINARY_DIR}/UserMod1.tag=../UserMod1/html\" ...")
configure_file(${QORE_USERMODULE_DOXYGEN_TEMPLATE}
    ${CMAKE_BINARY_DIR}/Doxyfile.final @ONLY)

# Create final-pass target
add_custom_target(docs-module-final
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile.final
    COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ...
    ...)
add_dependencies(docs-module-final docs-UserMod1 docs-UserMod2)
add_dependencies(docs docs-module-final)
```

Reference implementations:
- qore lang docs two-phase build: `CMakeLists.txt` (search for `docs-lang-final`)
- ncurses module: `CMakeLists.txt` (search for `docs-module-final`)

## Binary Module Namespace Registration

Binary modules (C++ `.qmod` files) register their namespaces in the `ns_init` callback, which
receives two namespace pointers:
- `rns` — the root namespace (`::`)
- `qns` — the Qore namespace (`::Qore`)

### Namespace naming convention

The `QoreNamespace` constructor argument must include the **full path** from the parent namespace
where it will be added. When adding to `qns`, include the `Qore::` prefix:

```cpp
// CORRECT: full path matches where the namespace is added
QoreNamespace MLNS("Qore::ML");

static void ml_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(MLNS.copy());
}
```

```cpp
// WRONG: missing Qore:: prefix causes getPathName() to return incomplete paths
QoreNamespace MLNS("ML");  // DO NOT do this
```

When the namespace name doesn't include the full path, `Class::getPathName()` in the reflection API
returns an incomplete path (e.g., `ML::DBSCAN` instead of `Qore::ML::DBSCAN`). This breaks:
- `qjar` JNI bytecode generation (uses `'::' + cls.getPathName()` for class resolution)
- Any code that relies on `getPathName()` for absolute namespace lookups

### QPP `ns=` attribute must match

The `ns=` attribute in `qclass` declarations must match the full namespace path:

```cpp
// If namespace is Qore::ML, use ns=Qore::ML
qclass DBSCAN [arg=QoreDBSCAN* dbscan; ns=Qore::ML; flags=final];
```

See `design/qpp-development.md` for more details on the `ns=` attribute.

### Examples from existing modules

| Module     | QoreNamespace constructor  | `ns=` in qpp        | Added to |
|------------|---------------------------|----------------------|----------|
| reflection | `"Qore::Reflection"`      | `Qore::Reflection`   | `qns`    |
| logger     | `"Qore::Logger"`          | `Qore::Logger`       | `qns`    |
| ml         | `"Qore::ML"`              | `Qore::ML`           | `qns`    |
| linenoise  | (user module pattern)     | `Qore::Linenoise`    | `qns`    |

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
- Modules delivered with Qore itself (e.g., `HttpServer`, `Mime`, `Logger`, `HttpServerUtil`,
  `DataProvider`, `ConnectionProvider`, `QUnit`) are always available and should use hard
  `%requires` — they do not need the `%try-module` pattern. Only use `%try-module` for modules
  from other external repos (e.g., `json` from module-json, `xml` from module-xml) that may not
  be installed.

## Checklist

- [ ] Correct layout chosen (directory vs single-file)
- [ ] `.qm` location matches layout
- [ ] all parse directives in the main .qm file for separated modules
- [ ] `%modern` used / redundant parse directives (`%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`) removed
- [ ] `CMakeLists.txt` module registration updated
- [ ] `CMakeLists.txt` module lists updated
- [ ] Docs module list (`doxygen/lang/120_modules.dox.tmpl`) updated when applicable
- [ ] entry in `doxygen/lang/900_release_notes.dox.tmpl` for new modules or updates
- [ ] Tests added/updated for the module
- [ ] External module dependencies use `%try-module` (not `%requires`) — except modules delivered with Qore itself (e.g., `HttpServer`, `Mime`, `Logger`, `DataProvider`) which are always available and use hard `%requires`; only in-repo modules and Qore-delivered modules use hard `%requires`
- [ ] Binary module `QoreNamespace` constructor uses full path (e.g., `"Qore::ML"`, not just `"ML"`)
- [ ] QPP `ns=` attribute matches the full namespace path
- [ ] all scripts have the execute bit set
- [ ] all tests and scripts use `%modern`
- [ ] module mainpage has `@section <lowercasemodname>intro` as first section
- [ ] documentation section IDs use `<modname>` prefix (not underscored variants)
- [ ] external modules with user sub-modules: two-phase doc build configured
