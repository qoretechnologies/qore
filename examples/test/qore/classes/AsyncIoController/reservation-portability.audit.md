# Audit: portable dual-stack test reservation

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` before the follow-up commit based on `ba3792c7ed`.
The previous reservation held two identical wildcard bindings using SO_REUSEADDR. macOS requires SO_REUSEPORT
for that temporary setup. Both reservation sockets now enable it, then the reservation closes and the test socket
clears SO_REUSEPORT before asserting that only the first socket may listen. The specific socket never enables it.

The macOS rule is verified against Apple's `in6_pcbbind()` implementation:
https://github.com/apple-oss-distributions/xnu/blob/main/bsd/netinet6/in6_pcb.c
The option is cleared by the socket option API before either listen call. Linux stress compiles the exact fixture
and runs both listen orders 200 times with 2,000 IPv4 ports occupied: 4,800 checks pass. `git diff --check` and
strict QPP table processing pass. Native macOS execution remains part of the ensuing CI pipeline.

All 71 checks: 14 Pass, 57 N/A, no failures.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Module added to QMOD list in `CMakeLists.txt`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No `%include` usage (deprecated for modules)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Copyright 2026 on all new files|Pass|The new audit report carries copyright 2026.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`%modern` directive present|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Executable permission set (`chmod +x`)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operations added.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|Only trusted run_unit_tests setup changes socket options; debug hooks retain QDOM_PROCESS and QDOM_UNCONTROLLED_API gating.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production network or filesystem path changes; the internal fixture intentionally uses native sockets.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|No loop added; the existing fixture has exactly two listen-order variants.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No cancellation point added or changed.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No unbounded iteration added.
|No blocking operations without cancellation support|Pass|setsockopt is nonblocking; no new blocking operation.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Response/output types use `private` Fields|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Password/secret fields have `"sensitive": True`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|No bare field/option names in prose — must use backticks|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|Only existing native socket-test setup changes; no module, Qore test, provider, API or packaging change.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Temporary port reuse makes reservation setup portable and is explicitly cleared before the assertions; no skipped check or retry.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Existing descriptor RAII handles early exits; setsockopt result is asserted.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|All socket descriptors and option values are local to the fixture.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Uses an int option value and sizeof(zero); no new casts or type conversions.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant work per fixture; one additional setsockopt call.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Failure to clear SO_REUSEPORT fails the test. Both expected listen outcomes remain asserted.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Inline comment explains why reservation sockets temporarily share ports and why reuse is cleared.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No production privilege change. SO_REUSEPORT is disabled before testing that the second listener is refused.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Exact native Linux fixture stress passes 4,800 checks with 2,000 IPv4 ports occupied; Apple in6_pcbbind source confirms identical-address reservation binds require SO_REUSEPORT on both sockets.
