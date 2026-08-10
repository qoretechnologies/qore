# Qore Module Structure and Conventions

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
- Register the module once in `CMakeLists.txt` using the directory path (no `.qm`
  suffix):
  - `qore_user_module("qlib/<ModuleName>")`
- **Any arguments after the module path are extra resource files, not
  dependencies.** They are passed to `qdx` as `--extra-files` so the
  documentation build knows about assets that are not `.qm`/`.qc` sources —
  logo SVGs, AsyncAPI YAMLs, and the like:
  - `qore_user_module("qlib/<ModuleName>" "<modulename>-logo.svg")`
- Multiple extra files are separated by semicolons, and each is resolved
  relative to the module's own source directory (`qdx` is invoked with
  `--extra-prefix <module_src_dir>/`), so a `../..`-relative path reaches
  assets outside `qlib/`:
  - `qore_user_module("qlib/FileDataProvider" "file-logo-white.svg;file-logo-black.svg")`
  - `qore_user_module("qlib/SqlUtil" "../../doxygen/SqlUtil-Full.svg")`
- **Module dependencies are never declared here.** Build-order and doc-target
  edges are derived automatically from each module's own `%requires` directives
  by `QORE_FINALIZE_USER_MODULE_DEPENDENCIES()`, which runs after every module
  target exists. `%requires` in the `.qm` is the single source of truth, so a
  module may `%requires` a sibling declared later in `CMakeLists.txt` with no
  ordering concern.
- That single call is all that is required. `qore_user_module()` automatically:
  - installs every `*.qm`, `*.qc`, `*.yaml`, `*.svg`, and `*.proto` file under
    the module directory into the installed module subdirectory on
    `make install` — this is a glob over the directory and is independent of the
    extra-files arguments, which affect documentation only, and
  - creates the `docs-<ModuleName>` documentation target, wires it into the
    top-level `docs` target, and registers the module in `QORE_USER_MODULE_NAMES`
    for cross-reference tag files.
- There are **no** separate doc-list or asset-packaging variables to maintain in
  this CMake build. The autotools-era `DOX_SRC_SPLIT_MODULES` / `DOX_SPLIT_MODULES`
  lists and `dist_<ModuleName>_modver_DATA = $(wildcard ...)` rules do not exist
  here and must not be added. If a module ships asset types other than
  `*.qm`/`*.qc`/`*.yaml`/`*.svg`/`*.proto`, extend the install glob in the
  `QORE_USER_MODULE` macro in `cmake/QoreMacros.cmake`.
  With `QORE_BUILD_AOT_MODULES=ON` (the default) the same macro also AOT-compiles the
  module to a `.qmod` via `qcc`. Projects that drive `qcc` compilation/linking directly
  (pre-compiled `.qo` objects, C/C++ hosts) use the lower-level AOT build macros
  (`qore_qcc_compile_objects`, `qore_qcc_link_objects`, `qore_qcc_script_aggregate`, and
  helpers) also in `cmake/QoreMacros.cmake` — documented under "AOT Module-Build Macros"
  in the CMake API guide (`doxygen/lib/90_cmake.doxygen`).

### Source-Owned Native Catalogs

Provider presentation catalogs belong to the module that registers the app or
action contribution. Directory modules store them below
`qlib/<ModuleName>/i18n`; flat modules use `qlib/<ModuleName>.i18n`. The module
registration macros install each locale as the owner-qualified fragment
`<catalog-root>/<domain>/<locale>/<ModuleName>.json`, so several modules can
contribute disjoint messages to the same app domain without overwriting one
another. Contributions with the same locale and message identity must be
structurally identical; provider presentation rejects conflicting owner fragments
instead of selecting text by catalog search-path or filename order.

Projects that consume Qore's CMake API but keep modules outside `qlib/` call
`QORE_INSTALL_USER_MODULE_CATALOGS(<ModuleName> <catalog-source-dir>
<install-component> <catalog-install-root>)` explicitly. The source directory,
component, and install-root arguments are optional; omitting them preserves the
standard Qore layout and component. A downstream project installed under its
own prefix passes a relative install root so CMake resolves the catalogs below
that project's `CMAKE_INSTALL_PREFIX` rather than Qore's installation prefix.
Do not install the source `i18n/` directory inside the runtime module directory:
the native i18n search path resolves the owner-qualified installed fragments.

### 2) Single-File Modules (for small modules)

Use when a module is defined in a single `.qm` file.

Layout:

- `qlib/<ModuleName>.qm`

Rules:

- Register once in `CMakeLists.txt` with the full file path **including** the `.qm`
  suffix:
  - `qore_user_module("qlib/<ModuleName>.qm")`
- As with directory modules, this single call handles installation and
  documentation automatically; there are no doc-list or packaging variables to
  update, and dependencies are derived from the module's own `%requires`
  directives rather than declared in `CMakeLists.txt`.

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

## Module Search Path

Usage of `%prepend-module-path` / `%append-module-path` is documented for users
in `doxygen/lang/245_parse_directives.dox.tmpl` — syntax, the `${NAME}` macro
table, escaping, and examples. What follows is the resolution model those
directives plug into, which several places in the runtime depend on.

### Layering

The effective search path at any point, highest precedence first:

1. **Per-Program prepended paths** — most-recently-added first, so the last
   `%prepend-module-path` parsed sits at the head
2. **Process `QORE_MODULE_DIR`** entries, plus legacy `%append-module-path`
   additions, which write to the process-global list
3. **Compiled-in defaults** — `${PREFIX}/share/qore-modules/<ver>` and
   `${PREFIX}/lib*/qore-modules/<ver>`
4. **Per-Program appended paths**, in the order added

`ModuleManager` builds this order directly; the per-Program lists live on
`qore_program_private` as `prepended_module_paths` / `appended_module_paths`.

### The same order applies at run time

`Program::loadModule()`, `loadApplyToUserModule()` and the `load_module()`
builtin consult the per-Program lists too, in exactly the parse-time order.
Without that, a program could `%prepend-module-path` for its `%requires` and
still have a runtime load resolve against the system path — the same module name
resolving two different ways within one Program.

### Subprogram inheritance

A subprogram inherits its parent's prepend and append lists, appended uniquely,
matching how parse options inherit. A subprogram's own directives extend the
inherited lists rather than replacing them. There is no opt-out.

### Persistence into AOT artifacts

Macro and environment expansion happens **at parse time on the parsing host**,
so the expanded strings — not the directives — are what persist. The AOT writer
emits them as `MODULE_PATH_PREPEND` and `MODULE_PATH_APPEND` sections, gated by
the `QORE_AOT_FEAT_MODULE_PATH_LISTS` feature flag so that older blobs without
the bit simply deserialize to empty lists.

The reader re-applies both lists to the target Program **before any of the blob's
dependency loads run**; for multi-blob batches the per-blob lists are merged and
de-duplicated before `resolveAll()`. This ordering is the whole point: it is what
lets an AOT-compiled binary find its own vendored modules with no host-side
`MM.addModuleDir()` call and no `QORE_MODULE_DIR` set by the launcher.

The consequence for deployment is that an AOT binary whose paths came from an
environment variable needs that variable set at **compile** time, not at run
time.

## Child Modules

`%try-child-module` lets a user module declare an optional extension that the
loader activates automatically. Syntax, version comparison, the three outcomes
(absent / failed / skipped), warning names and `get_module_hash()` introspection
are documented for users in `doxygen/lang/245_parse_directives.dox.tmpl` and
`doxygen/lang/120_modules.dox.tmpl` (`user_module_children`). What follows is why
the mechanism is shaped the way it is.

### Why a parent cannot just load its own extension

Every simpler approach fails, and the reason is specific:

| Attempt | Outcome |
|---|---|
| `%try-module Ext` in the parent | parse time — far too early |
| `load_module("Ext")` in the parent's `init` closure | `PARSE-EXCEPTION: cannot find any namespace or class 'Base'` |
| `load_module("Ext")` lazily on first use | works, but ties activation to a call, cannot tell absent from broken, and needs boilerplate in every extensible base |

The middle row is the one that matters. While a user module's `init` closure
runs, its feature is reserved in `QoreModuleManager::module_load_map` as
`MLS_INITIALIZING` owned by the current thread. When the child then parses
`%requires(reexport) Base`, `loadModuleIntern()` matches `e.owner_tid ==
q_gettid()` and returns `nullptr` **with no exception** — so the parent's
namespace is never merged into the child's `QoreProgram`, and every `Base::`
reference fails to resolve. The parent is published into the module map by
`setupUserModule()` strictly after both the parse-phase and init-phase
reservations are released.

Children can therefore only be attached **after the parent's load has returned**.

### Attach point

Attachment happens at the outermost module-load boundary on the current thread
(`module_load_depth == 0`), not at the end of the parent's own load.

If the parent `P` is loaded from inside another module `X`'s parse, attaching
`P`'s children immediately would run a child's parse while `X` is still reserved
`MLS_INITIALIZING` on this thread — reproducing the failure above one level up
for any child that requires `X`. Deferring to the outermost boundary removes
that class of failure by construction.

Within one drain, children attach in declaration order, depth-first. The drain
runs **before** the parent is merged into the requesting `QoreProgram`, so a
caller sees a complete extension set as soon as the parent is available.

### The child gets the parent's context, not the caller's

A child loads with `pgm = nullptr`: it registers globally and its `init` runs,
but its namespace is **not** merged into any `QoreProgram`. A base module must
not gain symbols from an optional extension, or its API surface would depend on
which extensions happen to be installed. Code wanting the child's classes
`%requires` the child.

Parse options and the module search path come from the **parent module's**
`QoreProgram` (`path_pgm`), not from whichever program triggered the load. This
keeps child resolution deterministic, and lets a parent loaded from a
non-standard search path find its children in that same path.

### A broken child never fails the parent

An earlier implementation raised the child's error and failed the parent's load,
arguing this keeps `%requires Parent` deterministic. **That reasoning was
rejected.** Determinism was never in question — the outcome is already a pure
function of what is installed — and it does not justify the blast radius: one
incompatible optional extension takes down every consumer of the base module,
including all those that never use the extension.

The failure mode was not hypothetical. A stale out-of-tree
`SalesforcePubSubDataProvider` on a single CI runner broke three unrelated
`SalesforceRestDataProvider` test suites this way.

So `failed` joins `absent` and `skipped`: reported, skipped, parent fully usable.
Nothing is added to the caller's exception sink and the parent's load return code
is unaffected. A broken child is always reported, though — unlike `skipped`,
which reflects a deliberate restriction in the container `Program`.

### Teardown, cycles, and concurrency

A child holds references to the parent's classes and registers data into the
parent's structures, so the parent must be destroyed after the child. A child
declaring `%requires(reexport) Parent` records that edge automatically; for those
that do not, the attach step records it explicitly with
`setUserModuleDependency(parent, child)` — but only when both are user modules,
since binary modules are not torn down by `delUser()` and must not be tracked.

Three re-entrancy cases are handled without new locking:

- **Mutual child declarations** — the second attach re-enters for a parent
  already attaching on this thread and is skipped via a thread-local in-progress
  set; both modules load, neither recurses.
- **A child loaded explicitly that pulls in its parent** — when the parent then
  attaches that child, the loader sees the child's own in-progress reservation
  owned by this thread and returns without error; the in-flight load completes
  and the child is recorded as attached.
- **Concurrent loads of one parent on two threads** — both run the attach loop
  and serialize per child on the existing `module_load_map` reservation; status
  bookkeeping is idempotent under the module-manager mutex.

### Surviving AOT compilation

`QORE_BUILD_AOT_MODULES` defaults to `ON` and a `.qmod` wins over `.qm` in the
search order, so a qlib module normally reaches deployments as an AOT binary
module. If child declarations did not survive AOT, the feature would be silently
inert exactly where it matters.

- `QoreAOTModuleInfo` carries a `child_modules` list, populated by the same raw
  source scan that collects `%requires` dependencies. Children are **not** emitted
  as module dependencies: a dependency is mandatory and loads before `init`, a
  child is optional and loads after.
- The generated description function calls `qore_aot_fill_module_children()`, a
  separate entry point from `qore_aot_fill_module_desc()` so that previously
  compiled artifacts — which never call it — keep working unchanged.
- `QoreModuleInfo` has a matching field; the loader copies it onto the module
  object, after which binary and user modules share one attach implementation.
- The Program supplying parse options and search path comes from
  `qore_aot_get_module_pgm()`, since an AOT-compiled user module has no
  `QoreUserModule` but does register a module Program with the AOT runtime.
  Without this, a child installed beside its AOT parent outside the standard
  search path is reported absent.
- An AOT child loaded with `pgm = nullptr` is initialized in its private module
  Program after registration, so its `init` runs as it would from source while
  its namespace still stays out of the parent. Init side effects execute only
  during the first shared AOT initialization; importing the child explicitly
  later does not repeat registrations. An exception from that initializer keeps
  its original error code and detail, marks the attachment failed, and is
  reported and skipped exactly as a source child's `init` exception is.
- The directive is stripped from embedded source before it is re-parsed at
  runtime (`stripRequiresDirectives()`), because by then the declaration has
  already arrived through the description function.

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
- Modules delivered with Qore itself are always available and should use hard `%requires` — they
  do not need the `%try-module` pattern. That covers the user modules under `qlib/`
  (`HttpServer`, `Mime`, `Logger`, `HttpServerUtil`, `DataProvider`, `ConnectionProvider`,
  `QUnit`, …) **and** the binary modules built from this repo under `modules/` — notably
  `json`, which is built here and hard-required by ~100 in-tree modules and their tests.
- Only use `%try-module` for binary modules from other repos (e.g., `xml` from module-xml,
  `yaml` from module-yaml, `sqlite3`, `uuid`) that may not be installed.

## Checklist

- [ ] Correct layout chosen (directory vs single-file)
- [ ] `.qm` location matches layout
- [ ] all parse directives in the main .qm file for separated modules
- [ ] `%modern` used / redundant parse directives (`%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`) removed
- [ ] `CMakeLists.txt` module registered with a single `qore_user_module()` call (directory path for directory modules, `.qm` path for single-file modules) — no separate doc-list or packaging variables, and no dependency list (dependencies come from `%requires`)
- [ ] any non-source assets the docs need (logo SVGs, YAMLs) passed as extra-file arguments to `qore_user_module()`
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
