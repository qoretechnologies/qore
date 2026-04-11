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

### Doxygen Code Block Language Tags

When embedding Qore source in doxygen `@code` blocks, always use `@code{.py}`:

```
@code{.py}
%modern
%requires openldap
LdapClient ldap("ldap://localhost");
# ...
@endcode
```

**Reason:** Doxygen has no `qore` language highlighter, so `@code{.qore}` produces no
highlighting at all and bare `@code` falls back to plain text. Python's highlighter
handles Qore's `#`-prefixed line comments correctly and is a close enough match for the
rest of the syntax (curly braces, string literals, function definitions, `%`-prefixed
parse directives) to render readably.

This rule applies to every file where doxygen processes `@code` blocks containing Qore
source: `.dox`, `.dox.tmpl`, `.doxygen.tmpl`, `.qpp`, `.qm`, `.qc`, and class header
comments in `.h` / `.cpp`. When touching a file that still has bare `@code` or
`@code{.qore}` Qore blocks, prefer fixing them in the same pass.

### Multi-Page Documentation Layout

For any non-trivial module, the `@mainpage` should act as a concise index rather than a
single page holding every example, helper-module description, and release-note entry.
Once the mainpage template grows past roughly 200 lines — or once it visibly contains
more than one "type" of content — split it into topical subpages.

**Recommended split for a typical module:**

| File                          | Content                                                     |
|-------------------------------|-------------------------------------------------------------|
| `mainpage.doxygen.tmpl`       | Intro, key-capabilities bullet list, operations/API overview table, `@subpage` navigation index, user-modules one-liner list, quick links, installation caveats |
| `getting-started.doxygen.tmpl`| Prerequisites, minimal "hello world" walkthrough, pointers to the cookbook and feature guides |
| `cookbook.doxygen.tmpl`       | Complete typed examples for every operation the module exposes |
| `<feature>-guide.doxygen.tmpl`| One page per major feature that needs more than a cookbook entry — reference tables, decision guides, hardening recommendations (e.g. `sasl-guide.doxygen.tmpl`, `authentication.doxygen.tmpl`, `sftp-backends.doxygen.tmpl`) |
| `helper-modules.doxygen.tmpl` | Each user sub-module with its own section and code example   |
| `release-notes.doxygen.tmpl`  | Full version history in reverse-chronological order         |

**Rules:**

- The mainpage must keep the intro section (`@section <lowercasemodname>intro`) per
  the naming convention above — it is the required landing section and external doc
  builds reference it.
- Each subpage is a doxygen page declared with `@page <pageid> <Title>` where
  `<pageid>` starts with the module name in lowercase (e.g. `openldapgettingstarted`,
  `openldapsaslguide`). Anchors on subpages use `@section <pageid><anchor>` so the
  rendered URLs stay stable.
- The mainpage links subpages via `@subpage` (not `@ref`) so they appear in the
  doxygen tree navigation; subpages link peers via `@ref`.
- Release notes always live on their own page — they only grow, and nobody visiting
  the mainpage for orientation wants a changelog.
- Cookbook examples always live on their own page — examples dwarf the index content.
- A major feature warrants its own guide page when it has **non-example** content
  (reference tables, decision guides, operational recommendations, caveats) — not
  just additional code samples. Pure code samples belong in the cookbook.
- Wire every `.doxygen.tmpl` file into `CMakeLists.txt` via `QORE_DOX_TMPL_SRC`, the
  same list passed to `qore_wrap_dox()`. Missing files silently skip processing.

**Reference implementations:**

- `module-ssh/docs/` — eight-subpage layout (getting-started, authentication, cookbook,
  sftp-backends, ssh-key-management, typed-contracts, binary-typed-symbols,
  release-notes) with a 66-line index mainpage
- `module-openldap/docs/` — six-subpage layout (getting-started, cookbook, sasl-guide,
  helper-modules, release-notes) with a 106-line index mainpage

### Two-Phase Doc Build for External Modules with User Sub-Modules

When an external binary module includes user sub-modules (e.g., `krb5` includes
`Krb5Util`, `ncurses` includes `NcursesUi` and `NcursesReplUi`), the binary module
docs must be built in two passes so that the binary module's mainpage can `@ref`
into user module symbols:

1. **Initial pass** (`docs-module`): generates the binary module's tag file (e.g.,
   `krb5.tag`) with empty `TAGFILES` and `WARN_IF_DOC_ERROR = NO` to suppress
   unresolved cross-reference warnings.
2. **User module builds** (`docs-<UserModuleName>`): each generates its own tag
   file, referencing the binary module's tag file for cross-references back to
   binary module symbols.
3. **Final pass** (`docs-module-final`): rebuilds the binary module docs with the
   user module tag files in `TAGFILES`, enabling `@ref` cross-references from the
   mainpage into user module symbols.

#### Preferred: `qore_binary_module_two_phase_docs()` macro

External modules should use the `qore_binary_module_two_phase_docs()` macro from
`QoreMacros.cmake`. It handles all three phases, including the correct relative
paths for tag files (a subtle point — see below), and replaces ~25 lines of
duplicated inline CMake with a single call:

```cmake
qore_external_binary_module(${module_name} ${PROJECT_VERSION} ${LINK_FLAGS})
qore_external_user_module("qlib/Krb5Util" "")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_SRC ${QORE_DOX_TMPL_SRC})
    add_custom_target(QORE_MOD_DOX_FILES DEPENDS ${QORE_DOX_SRC})
    add_dependencies(docs-module QORE_MOD_DOX_FILES)

    # Two-phase doc build so the binary module's mainpage can @ref into user modules
    qore_binary_module_two_phase_docs(${module_name} "Krb5Util")
endif()
```

With multiple user sub-modules, pass a semicolon-separated list:

```cmake
qore_binary_module_two_phase_docs(ncurses "NcursesUi;NcursesReplUi")
```

The macro must be called **after** `qore_external_binary_module()` and all
`qore_external_user_module()` calls for the sub-modules, because it reads the
binary-module Doxyfile that the former generates and adds dependencies on the
`docs-<UserModuleName>` targets that the latter register.

#### Relative-path gotcha in `TAGFILES`

The binary module's HTML output is at `${CMAKE_BINARY_DIR}/docs/${binary}/html/`
and each user module's HTML is at `${CMAKE_BINARY_DIR}/docs/${usermod}/html/`.
From the binary module's HTML pages, the correct relative path to a user module
is therefore **`../../${usermod}/html`** (two `..` levels — one to escape the
`html/` subdirectory, another to escape the binary module's own
`docs/${binary}/` parent back to the shared `docs/` root).

Earlier inline implementations of this pattern used a single `..` level and
produced broken cross-module links that resolved to a non-existent
`docs/${binary}/${usermod}/html/` path. The `qore_binary_module_two_phase_docs()`
macro fixes this in one place; always prefer the macro over a bespoke inline
implementation.

#### Reference implementations

- `cmake/QoreMacros.cmake` — `QORE_BINARY_MODULE_TWO_PHASE_DOCS` macro
- `module-krb5/CMakeLists.txt` — single-user-sub-module usage
- `qore/CMakeLists.txt` — qore lang docs two-phase build (search for
  `docs-lang-final`); this predates the macro and uses its own inline pattern
  tailored to the lang-docs layout

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

## Binary Module Build Dependencies and Feature Detection

### Match declared dependencies to actual usage

`CMakeLists.txt` must honestly describe what the module needs to build. It is a bug
for CMake to declare a dependency "optional" when the source unconditionally includes
its headers or uses its macros — users on minimal systems will get a compile error
instead of the intended "feature disabled" fallback, and the `HAVE_*` flag becomes a
lie.

**Example of the anti-pattern (from `module-openldap` before the 2.0 cleanup):**

```cmake
# Find Cyrus SASL (optional, for SASL authentication support)
find_path(SASL2_INCLUDE_DIR sasl/sasl.h)
if(SASL2_INCLUDE_DIR AND SASL2_LIBRARY)
    set(HAVE_SASL2 TRUE)
endif()
```

…while `openldap-module.h` unconditionally did `#include <sasl/sasl.h>`. The `HAVE_SASL2`
flag never actually gated anything — builds silently failed on systems without libsasl2.

**Correct:** when the code hard-depends on a library, make the CMake check `FATAL_ERROR`
on miss. When the code truly is optional, gate every include and every usage with the
corresponding `HAVE_*` flag via `config.h`.

### Optional C library features: configure-time probe + always-exported constants

When a C library adds an API in a newer version (e.g. OpenLDAP 2.6 added
`LDAP_OPT_X_SASL_CBINDING`) and you want the module to build against older versions
too, the pattern used across the Qore codebase is:

**1. Probe in `CMakeLists.txt` with `check_symbol_exists()`:**

```cmake
include(CheckSymbolExists)
set(_saved_cmake_required_includes "${CMAKE_REQUIRED_INCLUDES}")
list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBRARY_INCLUDE_DIR})
check_symbol_exists(LDAP_OPT_X_SASL_CBINDING "ldap.h" HAVE_LDAP_SASL_CBINDING)
set(CMAKE_REQUIRED_INCLUDES "${_saved_cmake_required_includes}")
```

**2. Propagate to `config.h` via `cmake/config.h.cmake`:**

```
#cmakedefine HAVE_LDAP_SASL_CBINDING
```

**3. In `.qpp` files exporting related constants, use `#ifndef` sentinel fallbacks so
the constant is always defined:**

```cpp
// OpenLDAP < 2.6 does not define these; fall back to sentinel values so the
// constants are always exported. Attempting to use them on an unsupported library
// yields a clear runtime error from the method that consumes them.
#ifndef LDAP_OPT_X_SASL_CBINDING_NONE
#define LDAP_OPT_X_SASL_CBINDING_NONE -1
#endif

const LDAP_SASL_CBINDING_NONE = LDAP_OPT_X_SASL_CBINDING_NONE;
```

This matches the established pattern in `lib/qc_errno.qpp` where every `errno` constant
is exported regardless of whether the target platform defines it.

**4. Gate runtime behavior with `#ifdef HAVE_*` in the C++ implementation:**

```cpp
DLLLOCAL static bool parseOption(const QoreHashNode& opts, int& value,
        ExceptionSink* xsink) {
    QoreValue v = opts.getKeyValue("channel-binding");
    if (v.isNullOrNothing()) {
        return false;
    }
#ifndef HAVE_LDAP_SASL_CBINDING
    xsink->raiseException("LDAP-SASL-BIND-ERROR",
        "SASL channel binding is not supported by the linked OpenLDAP library; "
        "OpenLDAP 2.6 or later is required");
    return false;
#else
    // ... real parsing ...
#endif
}
```

And the consumer that calls the raw library API:

```cpp
#ifdef HAVE_LDAP_SASL_CBINDING
    if (has_channel_binding && setLdapIntOption("saslBind", "LDAP_OPT_X_SASL_CBINDING",
            LDAP_OPT_X_SASL_CBINDING, channel_binding, xsink)) {
        return -1;
    }
#else
    // The parse helper rejects the option before we get here; suppress unused warnings.
    (void)has_channel_binding;
    (void)channel_binding;
#endif
```

**Why the constants are always exported instead of `#ifdef`-guarded at the QPP level:**
Qore's parser evaluates module constant references at parse time, so if a constant is
conditionally absent, every test or script that references it breaks at parse time even
when it only reads the constant inside a runtime `if`. Sentinel fallback values keep
parse-time references stable; runtime consumers catch sentinel/unsupported attempts
and raise a clear error.

**Rules:**

- Always raise an exception with a message that names the required library version
  (e.g. "OpenLDAP 2.6 or later is required") when a feature is unavailable. Silent
  no-ops are never acceptable.
- Emit a `message(STATUS ...)` line in CMake showing whether the feature probe
  succeeded, so build logs make the configuration obvious.
- The CMake variable, the `config.h` define, and the `#ifdef` guard must all use the
  same name (e.g. `HAVE_LDAP_SASL_CBINDING`) — three parallel spellings are a
  common source of bugs.

**Reference implementations:**

- `module-openldap/CMakeLists.txt` + `src/QoreLdapClient.h` — `HAVE_LDAP_SASL_CBINDING`
  probe and gates for OpenLDAP 2.6 SASL channel binding
- `qore/lib/qc_errno.qpp` — always-exported `errno` constants with `#ifndef` sentinel
  fallbacks

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
- [ ] all doxygen `@code` blocks containing Qore source use `@code{.py}` (never `@code{.qore}` or bare `@code`)
- [ ] non-trivial modules split docs into topical subpages (`getting-started`, `cookbook`, feature guides, `helper-modules`, `release-notes`) with a concise mainpage index; every file wired into `QORE_DOX_TMPL_SRC`
- [ ] release notes live on their own page, not on the mainpage
- [ ] external modules with user sub-modules: two-phase doc build configured via `qore_binary_module_two_phase_docs()`
- [ ] `CMakeLists.txt` dependency declarations match actual code usage — no "optional" deps that the source unconditionally includes
- [ ] optional C library features detected with `check_symbol_exists()`, propagated via `config.h`, always-exported constants use `#ifndef` sentinel fallbacks in `.qpp`, and runtime paths raise a clear version-specific error via `#ifdef HAVE_*` gates
