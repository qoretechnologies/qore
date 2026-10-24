# Audit: optimized object member assignments (issue 5510)

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` to the complete uncommitted change based on `5420f8f24f`.
Reviewed module structure, sandboxing, cooperative cancellation and DataProvider guidelines.
The IR interpreter and JIT/AOT hash-key store helpers bypassed the notification registration in
`LValueHelper::doObjLValue()`. They now use the same AutoVLock notification mechanism and class-context rule.
The callback sees the stored value and runs after the lock is released; its exception reaches the assignment.

All 71 checks: 21 Pass, 50 N/A, 0 Fail.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|New MemberNotificationChecks is a test fixture, not a shipped module.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Qore 3.0.0 release notes describe the fix and link issue 5510.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|The existing test discovery finds the new qtest; its fixture is compiled in a temporary directory by the harness.
|Module added to QMOD list in `CMakeLists.txt`|N/A|No shipped qmod added; source-stripped AOT fixture creation is tested explicitly.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|Test-only module is not a generated documentation entry point.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|Pass|New fixture uses %modern without redundant legacy directives.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No separated .qc file.
|No `%include` usage (deprecated for modules)|Pass|No %include directives.
|Copyright 2026 on all new files|Pass|All new fixture, worker, test and audit files carry Copyright 2026.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|The fixture belongs beside its qtest, outside qlib.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|Only one entry point for the new test fixture.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new C++ QPP class.
|`%modern` directive present|Pass|Both drivers and the module use %modern.
|Executable permission set (`chmod +x`)|Pass|The qtest and worker are executable (100755).
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|The qtest prepends qlib and its fixture path before requires. The worker uses the explicit QORE_MODULE_DIR so AOT tests cannot load the source.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|All dependencies come from this repository and use hard %requires.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operations added to C++.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operations added to C++.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new external C++ operation needs a sandbox helper.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|The fixture uses in-memory values; the harness uses TmpDir for isolated compilation and synchronous workers whose output is collected before return.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ loops added.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No cancellation points or deprecated interrupt calls added.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|The C++ changes have constant cost; worker repetition is bounded to three runs.
|No blocking operations without cancellation support|Pass|Notification registration uses the existing deferred dispatch; callbacks run after the object lock is released.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Response/output types use `private` Fields|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Password/secret fields have `"sensitive": True`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|No DataProvider actions, schemas, app metadata, qualification, locales, registration, packaging or Java dependencies change.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Both optimized store helpers now register the notification through AutoVLock, matching the shared AST lvalue path. No workaround or suppression.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Existing LValueHelper RAII releases the lock, dispatches notifications and propagates their exceptions. No new allocations or ownership transfers in the helpers.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|The write retains the existing object lock. AutoVLock invokes callbacks after unlocking; internal writes remain suppressed by the same class-context check as AST.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Fixture results are hash<string, int>; event values are list<string>. No new casts or untyped callbacks.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|One context branch per optimized object write; existing notification storage and callback dispatch are reused.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Tests check values seen by callbacks, exact callback order, internal-write suppression, and a throwing callback after the value is stored.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Release notes document the fix; test helpers explain their contracts. No public API changes. Strict documentation processing passes.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP flags or signatures change.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No credentials, buffer arithmetic or untrusted format strings. Test processes use fixed repository fixtures.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|The object-variable and throwing-callback regressions fail before the fix. The fixed suite passes 31 assertions across AST, IR, JIT, tiered and stripped AOT. classes.qtest passes 167 assertions in each mode; Valgrind reports zero errors and lost allocations.

Validation:

- `cmake --build build --target qore qcc -j12` passes.
- The issue script and the new object-variable/callback-exception checks reproduce the bug before the fix.
- `member-notification.qtest` passes 31 assertions, covering typed, object and auto receivers; plain and
  compound assignments; static and computed keys; internal writes; callback order, values and exceptions.
  It runs source in AST, IR, JIT and tiered modes with native compilation checked by ExecModeHarness,
  and loads a stripped AOT module from a directory containing only its qmod. Workers repeat three times.
- Existing `classes.qtest` passes 167 assertions in each of the four execution modes (668 assertions).
- Source-only qlib staging avoids stale standard-library qmods. No qlib source was edited.
- Valgrind of the repeated IR worker with `-b`, `--enable-debug` and `QORE_PCRE2_NO_JIT=1` reports zero
  errors and zero definitely, indirectly or possibly lost bytes.
- The release notes and audit report pass `qpp --table-strict`; `git diff --check` passes.
