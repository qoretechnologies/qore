# Audit: calls through weak references to destroyed objects (issue 5509)

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` to all uncommitted changes based on `7f24ff4e86`.
Reviewed the module structure, sandboxing, cancellation and DataProvider guides, plus the closure-bound-local
and documentation-table designs. All 71 checks were reviewed: 21 Pass, 50 N/A, 0 Fail.

The weak reference retains the destroyed object, but dynamic dispatch could fall back to a pseudo-method
without the receiver check performed by ordinary method evaluation and some compiled paths. The check now
runs in shared dynamic dispatch. Compiled copy calls also enter copy construction directly; that path now
rejects a deleted source before allocating a copy. The expanded regression exposed a crash when failure
cleanup ran the destructor of an uninitialized copy, which this early rejection prevents.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped module; the existing ClosureWeakRefChecks test fixture is extended.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Qore 3.0.0 release notes explain the deleted-receiver check and link issue 5509.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module or build target; the existing qtest compiles its fixture module itself.
|Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped qmod; the existing test compiles a source-stripped qmod in a temporary directory.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module; the existing test-only module keeps its documentation layout.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|Pass|ClosureWeakRefChecks.qm uses %modern without redundant legacy directives.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No separated .qc file changes.
|No `%include` usage (deprecated for modules)|Pass|No %include directives in the changed test fixture or drivers.
|Copyright 2026 on all new files|Pass|The new audit report and existing changed test files carry Copyright 2026.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No qlib module added; the existing fixture remains beside its test.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No duplicate module entry point added.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new QPP class or namespace registration.
|`%modern` directive present|Pass|Both executable test drivers and the fixture use %modern.
|Executable permission set (`chmod +x`)|Pass|The .qtest and worker are mode 100755; the .qm fixture is a non-executable module.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|The qtest prepends the repository qlib and fixture paths before %requires; the worker deliberately takes the fixture directory from QORE_MODULE_DIR to test source and AOT independently.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|All dependencies are supplied by this repository and use hard %requires: QUnit, FsUtil, ExecModeHarness and ClosureWeakRefChecks.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|The C++ change only reads object status and raises exceptions; no filesystem operations.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No new network operations.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No filesystem or network operation requires a sandbox helper.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|The added Qore checks use only in-memory objects and closures. Existing TmpDir and worker compilation provide isolated AOT testing and synchronous child-process cleanup.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ loops added or changed.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No cancellation points or deprecated interrupt calls added.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new long-running C++ work; test loops are bounded to three repetitions.
|No blocking operations without cancellation support|Pass|The C++ change adds no blocking operation or lock acquisition.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Response/output types use `private` Fields|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Password/secret fields have `"sensitive": True`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|No DataProvider actions, types, metadata, qualification, locales, app registration, packaging or Java dependencies change.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Checks are in shared dynamic dispatch and copy construction, not in individual execution engines or pseudo-method implementations. No TODO, FIXME, stub or suppression added.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Both error paths return immediately through ExceptionSink. Copy rejection happens before allocation, preventing destructor execution on an uninitialized copy. Existing ReferenceHolder cleanup remains unchanged.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|isValid() reads the existing atomic object status. No mutable shared state, reference ownership, locks or object lifetime rules are changed; a destructor-in-progress is still valid.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|New callbacks use code<auto()> and results remain hash<string, int>. No C casts, raw allocations or untyped callbacks are added.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Each dispatch or copy adds one constant-time status check. No new allocation, scan or container copy is introduced.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|A destroyed receiver raises OBJECT-ALREADY-DELETED before pseudo-method fallback or copy allocation. Tests check the exact exception name, live receivers, destructor counts and repeated calls.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|The weak-assignment language reference and release notes document the corrected exception behavior. New private fixture helpers explain their test contracts; no public API signatures change. Both changed documentation templates pass qpp --table-strict.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method bodies, flags or method signatures change.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Exception format strings are fixed literals, with class and method names passed as arguments. No credentials, buffers, index arithmetic or externally supplied code are introduced.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|The original issue reproduces before the fix and passes in AST, IR, JIT and tiered modes afterward. Six object regression groups plus existing weak-container groups pass 103 QUnit assertions including source-stripped AOT. Native traces confirm promotion of all six new groups; Valgrind reports zero errors and no lost allocations. Nearby method, closure, weak-container and object-lifetime regressions pass.

Validation:

- Built `qore` and `qcc` with `cmake --build build --target qore qcc -j12`.
- The original issue script now raises `OBJECT-ALREADY-DELETED` for both direct and closure calls in AST,
  IR, JIT and tiered execution with debugging enabled.
- `closure-weak-refs.qtest` passes 103 assertions: in-process checks, all four execution modes, and a
  source-stripped AOT module loaded from a directory containing only the qmod. Worker runs repeat the checks
  three times; ExecModeHarness verifies native compilation, and the tiered trace shows native promotion of
  all six object checks. Source-only qlib staging avoids stale compiled standard-library artifacts.
- The new checks cover live and destroyed targets, an uncaptured local, a direct read of a captured local,
  closure bodies, escaped closures, typed variables, reference parameters, pseudo-methods, ordinary methods,
  missing methods and copy. Destruction is checked before calls and after repeated escaped-closure calls.
- Nearby suites pass: weak-container-refs, closure-bound-locals, object-closure-self, pseudo-methods, object,
  ObjectClosureLifetime, closure_destruction_order, closure-destroyed-self and IRWeakReferenceLocalStore.
  Direct method/object/closure-destruction suites run in all four execution modes. Existing copy coverage is
  also checked with class_copy and ImplicitCopyDispatch in all four modes.
- Valgrind of the repeated worker in IR mode, with `-b`, `--enable-debug` and `QORE_PCRE2_NO_JIT=1`, reports
  zero errors and zero definitely, indirectly or possibly lost bytes. Normal process teardown is retained.
- Both changed doxygen templates and this report pass `qpp --table-strict`; `git diff --check` passes.
