# Audit: optional MySQL service prerequisite in serialization regression

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` to changes based on `d625ae2966`.
Reviewed module structure, sandboxing and cancellation guidance; provider checks are inapplicable to this test-only change.

Pipeline 61363 job 214007 could not reach MySQL. The existing BulkMysqlSqlUtil and trigger suites also skipped
for connection errors. SerializationConfig previously attempted the same unavailable service in each case;
the first attempt raised DBI:MYSQL:CONNECT-ERROR and the next crashed in libmariadb's mysql_real_connect_start.

Probe the optional service once before setting the typed option. Cache only an initial connection error and
skip the five live cases consistently when unavailable. Do not catch serialization or option errors. Successful
probes reuse the same datasource in the first case. No runtime or module source changes.

All 71 checks reviewed: 15 Pass, 56 N/A, no failures.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Module added to QMOD list in `CMakeLists.txt`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|No `%include` usage (deprecated for modules)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Copyright 2026 on all new files|Pass|New audit has copyright 2026; existing test retains its copyright.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`%modern` directive present|Pass|SerializationConfig.qtest uses %modern.
|Executable permission set (`chmod +x`)|Pass|Existing executable mode 0755 is preserved.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|Repository qlib path precedes all module requirements.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|External mysql is optional with %try-module; repository modules use %requires.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++ or direct native I/O changes.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No C++ or direct native I/O changes.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ or direct native I/O changes.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|No new File, Dir, Socket or HTTPClient operations. One existing database connection is opened earlier to probe the explicitly configured optional service.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ changes, loops or blocking primitive added; existing Datasource open retains driver cancellation.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No C++ changes, loops or blocking primitive added; existing Datasource open retains driver cancellation.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++ changes, loops or blocking primitive added; existing Datasource open retains driver cancellation.
|No blocking operations without cancellation support|N/A|No C++ changes, loops or blocking primitive added; existing Datasource open retains driver cancellation.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Response/output types use `private` Fields|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Password/secret fields have `"sensitive": True`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|No bare field/option names in prose — must use backticks|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Only the initial DBI:MYSQL:CONNECT-ERROR marks the optional service unavailable; serialization, typed-option and subsequent connection failures are not caught. Existing SQL integration tests already skip unavailable database services.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Datasource lifetime is managed by Qore. TestSkip unwinds the failed probe; the cached unavailable state prevents repeated attempts. Callback cleanup is unchanged.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Two booleans belong to a single test instance; QUnit runs these cases sequentially. No shared state added.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Availability flags are bool and the caught exception uses hash<ExceptionInfo>; existing callback and datasource types are unchanged.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|At most one availability probe; on success the opened datasource is reused by the first test, without an extra connection.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Only the exact DBI:MYSQL:CONNECT-ERROR from the initial open is treated as missing infrastructure. Invalid options still exit with an error. No credential-bearing exception text is logged in skip messages.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Inline comment explains the prerequisite boundary; this audit records the CI evidence and positive/negative validation. No public API changed.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|Test fixture and audit only; no module, native class, packaging, provider, catalog or Java changes.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No credentials added; probe uses the existing environment connection. Skip messages contain no connection string or exception description.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|16 source/AOT runs across AST, IR, JIT and tiered pass: eight live runs execute all 26 assertions, eight unavailable-service runs skip all five cases without a crash. Restoring the old lossy serializer and supplying an invalid typed option both still fail with MYSQL-OPTION-ERROR. No AOT fallback.

Validation:

- Source and AOT modules in AST, IR, JIT and tiered modes: eight live MariaDB runs execute all five cases and
  26 assertions (208 assertions total); eight refused-connection runs skip five cases without crashing.
- A source-only mutation restoring the lossy serializer still fails with MYSQL-OPTION-ERROR, exit 2.
- An invalid typed driver option also fails with MYSQL-OPTION-ERROR, exit 2; it is not treated as a missing service.
- No AOT source fallback. Executable/directive checks, strict audit table validation and git diff --check pass.
- The disposable database container is stopped after validation. No production module changed or needed rebuilding.
