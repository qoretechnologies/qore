# Audit: large elapsed-second durations (issue 5507)

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` to all uncommitted changes based on `737a6ef890`.
Reviewed the module structure, sandboxing, cancellation, DataProvider checklist and development guide.
The fix preserves elapsed seconds through Qore and C++ conversions and absolute-date addition. It retains
hours as elapsed hours across timezone transitions; the existing signed 32-bit hour-component range is
unchanged. Carrying hours into calendar days was explicitly ruled out by the user because a day may be 23 or
25 hours. Supporting a wider hour component would require a separate representation/API change.

All 71 checks were reviewed: 20 Pass, 51 N/A, no failures.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new user module, module registration, module layout or QPP class.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Core release notes describe the large-duration, fractional conversion and absolute-date arithmetic fixes, with the unchanged hour range and issue 5507.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new user module, module registration, module layout or QPP class.
|Module added to QMOD list in `CMakeLists.txt`|N/A|No new user module, module registration, module layout or QPP class.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new user module, module registration, module layout or QPP class.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new user module, module registration, module layout or QPP class.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new user module, module registration, module layout or QPP class.
|No `%include` usage (deprecated for modules)|N/A|No new user module, module registration, module layout or QPP class.
|Copyright 2026 on all new files|Pass|The new native test and this report carry copyright 2026; the modified Qore test also has a 2026 notice.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new user module, module registration, module layout or QPP class.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new user module, module registration, module layout or QPP class.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new user module, module registration, module layout or QPP class.
|`%modern` directive present|Pass|date_durations.qtest uses %modern.
|Executable permission set (`chmod +x`)|Pass|date_durations.qtest retains executable mode 755.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|The repository qlib path is prepended before requiring QUnit.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|QUnit and TimeZone are delivered by Qore; no external service or module dependency is added.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Production changes perform no filesystem I/O. Tests use the existing TimeZone API and inspect the native test executable path.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operations introduced.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new production filesystem or network operations.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|No raw File, Dir, Socket or HTTPClient operations. The test runs its own native fixture through a shell-quoted path and waits for it to exit.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|Production arithmetic has no loops. Native test loops are finite boundary-case lists, each with fewer than 100 iterations.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new cancellation point or deprecated cancellation API.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No production loops or expensive iterative operations.
|No blocking operations without cancellation support|Pass|No blocking production operation. The native test child completes synchronously; runner process-cleanup checks pass.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Response/output types use `private` Fields|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Password/secret fields have `"sensitive": True`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|No DataProvider actions, schemas, app metadata, factory registration, discovery, qualification, locales, packaging or Java dependencies changed.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Uses the common seconds setter and 64-bit quotient/remainder arithmetic. No stubs, TODOs, suppressions or calendar-day workaround.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|DateTime allocations use std::unique_ptr; DateTimeNode allocations use SimpleRefHolder. Returned ownership is documented. Native tests release every value before qore_cleanup; Valgrind is clean.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Only local date values are changed; no shared mutable state or caches are introduced.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Arithmetic retains int64 seconds until individual components are bounded. Explicit casts replace narrowing 32-bit intermediates; floating conversions subtract their int64 whole seconds. The existing int hour range remains a documented precondition.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time arithmetic, no loops or extra allocations. One normalization before splitting preserves fractions and signs without converting elapsed hours into days.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Tests cover both signs, zero, carries, INT_MIN/INT_MAX microseconds, hour endpoints and fractions that bring an input back into the valid hour range. No new I/O errors or widened input contract.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Factory API docs give params, return ownership, allocation exception, example and range/DST caveats. The setter, seconds() and release notes document elapsed-hour semantics. Strict QPP and documentation checks pass.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|Pass|seconds() and date(softfloat) retain CONSTANT and NAMED_ARGS: they remain deterministic conversions with no side effects.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No credentials, untrusted format strings or new unsafe buffers. Native fixture paths are shell quoted. Remainders and 64-bit products remove the demonstrated signed overflow; UBSan boundary checks pass.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Old Qore/native regressions fail and old setter triggers UBSan signed overflow. The fix passes 707 native cases, 24 Qore suite runs with 10,052 assertions, AOT conversion/literal checks, 146 UBSan cases and Valgrind with zero errors or lost allocations.

Validation:

- Rebuilt qore, qcc, qore-relative-seconds-test and qore-date-add-test. The new native executable is a default
  CMake build target and is invoked by date_durations.qtest in build-tree runs.
- The new Qore regression fails on the old runtime for large integers, fractions and elapsed-date arithmetic;
  the native test also fails on the original duration construction. The original shared setter reports signed
  integer overflow under UBSan. Fixed setter/addition boundary checks pass 146 cases without UBSan errors.
- The native API regression passes 707 cases, including both factories, both numeric value helpers, direct
  numeric constructors, the setter, relative-second addition and absolute-date addition. It covers mixed-sign
  fractions, full int microsecond arguments, and the normalized hour boundaries. Valgrind reports zero errors
  and zero definite, indirect or possible lost allocations; only process-lifetime caches remain reachable.
- Six suites pass in AST, IR, JIT and tiered execution: 24 invocations, 124 cases and 10,052 assertions. These
  cover duration constructors, native date addition, existing date/timezone parsing, UTC offsets, timezone
  errors and date formatting. The date suite's existing Windows-only case skips on Linux.
- An AOT executable of date_durations.qtest passes 5 cases and 253 assertions. The existing relative-literal
  source/AOT test passes 2 cases and 7 assertions, preserving unnormalized literal fields.
- The repository runner passes both selected date suites, including child-process cleanup checks.
- Strict QPP processing, release-note and audit tables, executable permissions and git diff --check pass.
