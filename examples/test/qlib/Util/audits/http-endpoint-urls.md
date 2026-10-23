# Audit: HTTP endpoint URLs (qore)

Copyright (C) 2026 Qore Technologies, s.r.o.

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` on 2026-10-23.

The changes add fixed-endpoint selection to Util, use it in both constructor forms
of JsonRpcClientIo and YamlRpcClientIo, and document the HTTP/REST/RPC addressing
contracts in the migration guide. Direct target extraction now removes a fragment
before parsing the authority, including a URL with a bare port and no path.

All 71 audit-changes checks were reviewed against the working tree based on
`67cb54bc56`; 17 Pass, 54 N/A, no failures. No native code or new modules were added.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|Module added to QMOD list in `CMakeLists.txt`|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|Pass|Parse directives remain in .qm entry points; .qc files have none outside documentation examples.
|No `%include` usage (deprecated for modules)|Pass|No deprecated module includes were added.
|Copyright 2026 on all new files|Pass|Every new test and this report carry copyright 2026.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new module; existing registrations, introduction sections, modern directives and layout remain in place.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|`%modern` directive present|Pass|All three changed/new tests use %modern.
|Executable permission set (`chmod +x`)|Pass|All three changed/new tests have executable permission.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|The module search path is prepended before requires in all three tests.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only dependencies delivered with Qore or the XML project are hard-required; no optional external dependency was introduced.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|Production changes perform pure string/URL operations and continue using the sandboxed HttpClientIo transport; tests start local HttpServer peers with on_exit cleanup.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|No blocking operations without cancellation support|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Response/output types use `private` Fields|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|No DataProvider action/app/type/factory/discovery/qualification/presentation/locale/JAR definitions changed.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete endpoint selection and request dispatch; no TODO, FIXME, stub, or alternate disabled implementation.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Only managed Qore values; no native allocations. Input errors occur before transport creation; local request state unwinds on failure.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Endpoint URLs and request targets are selected during construction. Shared helper functions have no mutable global state; no request changes another request target.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|URL components use UrlInfo; typed strings and header hashes define the new boundaries. No untyped production callbacks or native casts were introduced.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|URL splitting, encoding, fragment insertion, and header filtering are linear; no extra network round trips. Header overrides use a lowercase-key set.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Absolute HTTP(S) endpoints and host presence are checked; invalid authorities, fallback query/fragment/scheme syntax and NUL are rejected without echoing credentials.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|API parameter, result, exception, examples and caveats are documented. Strict Qdx/QPP and release-note policy checks pass; examples distinguish the addressing operations.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No C++/QPP implementation, native I/O, loops, allocations or QPP flags changed.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Authorities cannot come from fallback paths. User information is preserved only in endpoint URLs and masked in diagnostics. Invalid authority errors are sanitized; fixtures use synthetic values.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|The 14 selected Qore suites pass. Three new suites also pass using compiled modules: 62 helper assertions and 180 wire assertions each for JSON and YAML. Existing REST targets, HTTP URL resolution and redirect suites remain green.

Validation:

- Rebuilt Util, JsonRpcClientIo, and YamlRpcClientIo qmod targets before testing.
- `./run_tests.sh` passed all 14 selected suites: Util/http_endpoint_url,
  Util/uri_query, Util/get_file_from_url, both JSON and YAML Io suites and their
  new EndpointUrls suites, RestClient, RestClientYamlMediaType, RestClientIo,
  RequestTargets, HttpClientRequestUrl, HttpRedirectFollower, and HttpClientIoRedirect.
- New helper and RPC suites passed again with build/qlib-qmod preferred. Test
  support modules were rebuilt to remove unrelated stale-AOT source fallbacks.
- The wire matrix covers both constructors, HTTP and both RPC scheme aliases,
  default roots with/without leading slashes, explicit /, no path, query-only and
  fragment-only URLs, empty queries, escapes, Unicode, internal/trailing slashes,
  and dot segments. Pure tests also cover IPv6, credentials, Unix socket spelling,
  invalid fallbacks/authorities, and NUL.
- Strict Qdx extraction passed all changed Qore sources; strict QPP passed both
  changed language documentation files. Module release-note policy, executable/
  directive checks and git diff --check passed.
- The sibling module-xml uses the same helper for XmlRpcClientIo and SoapClientIo.
  Its separate audit covers reference resolution, query serialization, and origin
  boundaries. The earlier caller review found no migration needed in module-grpc,
  module-git, or the other scanned module repositories; native synchronous RPC and
  released RestClient string contracts remain unchanged.
