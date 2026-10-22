# Audit: explicit REST request targets (#5485)

Copyright 2026 Qore Technologies, s.r.o.

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` on 2026-10-22,
against the complete working-tree change based on `22fc48b5539b9179849c99ade531014ae21289cf`.
All applicable checks pass. No C++ sources or module registrations changed.
The changes comprise 40 Qore sources, 44 changed/new tests, the design document,
release notes, and this audit.

The implementation separates API-relative endpoints from origin-absolute targets,
retains released string APIs, snapshots Io targets and validators across retries,
and migrates schema dispatch, provider signing/routing, pings, streams, pagination,
and dependent test doubles together. The caller review also checked unchanged
Graph, FHIR, Constant Contact, and SendCloud pagination: those callers already
remove their service prefix or extract a cursor before making a relative request.
Provider defaults remain fallbacks; Fireworks and WooCommerce explicitly retain
their intentional namespace overrides.

The expanded embedding wire test exposed a request-body hash inferred as
`hash<list<string>>`; it now uses `hash<auto!>` so a model string can accompany the
input list. The new sync root lock is transient to preserve serialization.
The synchronous transport retains its existing URL-encoding options; the OAuth
regression explicitly selects `pre_encoded_urls` for its pre-encoded token URL.

|!Check|!Status|!Evidence
|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module; existing module registration and layout remain in place.
|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore module; existing module registration and layout remain in place.
|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore module; existing module registration and layout remain in place.
|Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore module; existing module registration and layout remain in place.
|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore module; existing module registration and layout remain in place.
|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore module; existing module registration and layout remain in place.
|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|Pass|No parse directives added to separated .qc sources.
|No `%include` usage (deprecated for modules)|Pass|No deprecated module includes introduced.
|Copyright 2026 on all new files|Pass|New test, design document, and this audit carry copyright 2026.
|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore module; existing module registration and layout remain in place.
|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore module; existing module registration and layout remain in place.
|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No C++ or QPP class added.
|`%modern` directive present|Pass|All 44 changed/new test files use %modern.
|Executable permission set (`chmod +x`)|Pass|All 44 changed/new test files are executable.
|Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|Module paths precede requires; migrated bundled dependencies load by module name.
|External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Bundled dependencies use hard requires; the optional external process dependency retains try-module.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++ filesystem or network changes.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No C++ filesystem or network changes.
|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem or network changes.
|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|Only a native HTTPClient path setter is added; requests still use sandboxed transports. Test Socket polling drives bounded operations.
|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ loops or blocking operations changed; REST deadlines, streaming cancellation, and cleanup regressions pass.
|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No C++ loops or blocking operations changed; REST deadlines, streaming cancellation, and cleanup regressions pass.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++ loops or blocking operations changed; REST deadlines, streaming cancellation, and cleanup regressions pass.
|No blocking operations without cancellation support|N/A|No C++ loops or blocking operations changed; REST deadlines, streaming cancellation, and cleanup regressions pass.
|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Response/output types use `private` Fields|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Password/secret fields have `"sensitive": True`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|No bare field/option names in prose — must use backticks|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Every producer publishes exact lightweight app/action inventory before schema materialization; inventory callbacks do not execute dynamic schema, connection, or action code|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Every caught registration/discovery/factory/scheme/publication failure reaches structured qualification state; logs or empty output are never treated as success|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Authoritative index publication requires a current authenticated single-use qualification token, and the revision check plus atomic writer share the catalog lock|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|External schemas are normalized once at a versioned boundary; absent/empty/null remain distinct and `AllowedValueInfo` conversion occurs only in declared choice positions|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Recursive presentation extraction uses only static metadata and declared fallback types; it never invokes dynamic type/default/example/network/connection callbacks|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Existing flat presentation IDs are unchanged; nested IDs cover structured fields, list elements, unions, and nested choices with cycle/DAG bounds|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Every shipped locale has exact root ID/source parity after regeneration; source-language fallback does not qualify a release|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|Installed-artifact checks use an empty prefix with source module paths excluded and cover normal/AST/AOT/fresh-process modes as applicable|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|JNI provider runtime JARs come from one declarative inventory with exactly one logging policy; committed checksums, staging parity, SLF4J API/provider compatibility, service entries, duplicate providers, and bridge cycles are validated|N/A|Transport dispatch and namespace binding only; no action/app/type/presentation/factory/qualification/JAR definitions changed.
|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Shared pure composer and typed modes replace prefix guessing; no added TODOs, FIXMEs, stubs, or temporary path clearing.
|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Qore owns object lifetimes; existing future/stream cancellation and on-error cleanup remain active. No native allocations changed.
|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Io snapshots target and validator under config_lock; retries retain request-local state. Sync override fields use a transient Mutex. Schema binding is immutable.
|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|RestPathMode enum and RestRequestTarget/RestRequestContext hashdecls define boundaries; new callbacks are typed. Embedding request bodies use hash<auto!> for mixed field types.
|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Composition and encoding are linear in target length and run once per Io request; retries reuse the result. No additional network round trips.
|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Invalid absolute targets, URL/fragment targets, query-bearing roots, and invalid deadlines are rejected; authentication errors and cancellation retain their existing paths.
|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Mode/root contracts, API parameters, return values, exceptions, inherited documentation, examples, migration notes, design, and release notes are present. Strict Qdx/QPP checks pass.
|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP methods changed.
|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Targets never change authority; provider origin checks remain active. Fixture credentials only. Existing redaction and signed-request tests pass.
|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|130 suites verified after corrective reruns; 533 target-matrix assertions cover wire paths, protocols, retries, signatures, schema snapshots, OAuth, serialization, and provider version roots.

Validation:

- Rebuilt all 37 affected module qmod targets with `cmake --build build --target
  <Module>-qmod -j8` before final verification.
- Ran `./run_tests.sh` for 130 selected HTTP, REST, schema, provider, tokenizer,
  reranker, and LLM suites. The broad run passed 127; after fixing its findings,
  the three failed suites and the expanded target suite passed together:

  ```sh
  ./run_tests.sh -d qlib/GoogleDataProvider/OfflineSchemas.qtest \
    -d qlib/QoreLlmUtils/LlmStreaming.qtest \
    -d qlib/RestClient/RestClient.qtest \
    -d qlib/RestClientIo/RequestTargets.qtest
  ```

- Final targeted results: Google offline schema dispatch 524 assertions; LLM
  streaming/cancellation 360; synchronous REST including serialization 171;
  request targets 533. No failures remain across the 130 suites.
- The request-target suite exercises HTTP/1.1, HTTP/2, HTTP/3, synchronous and
  asynchronous calls, raw bodies, polling, SSE readers/observables, concurrent
  modes, schema-owned roots, authentication retries, redirects, OAuth endpoint
  independence, serialization, and versioned provider endpoints.
- `OpenAiApiRoot.qtest` retains the #5484 regression coverage: 28 cases and 220
  assertions. Native `HttpClientRequestUrl.qtest` passes 13 cases and 189
  assertions, including RFC 3986 URI-reference behavior.
- Strict Qdx extraction passes for all 40 changed Qore sources. The final edits
  to RestClient, AnthropicRestClient, and EmbeddingModel were checked again.
  Strict QPP validation passes for the global release notes and this audit table.
- `git diff --check` and test directive/executable checks pass. Optional live
  provider checks skip when credentials are not configured; the existing AWS
  IoT Events live check skips its unavailable DNS endpoint. Local wire tests pass.
- Valgrind/native cancellation instrumentation is N/A: no native implementation
  changed. Existing deadline and cancellation suites cover the affected Qore paths.

Rebase audit onto `eb268d3c68` (2026-10-22):

Rechecked all 71 checklist items above against the rebased change: the 17 Pass
and 54 N/A classifications remain applicable. The validation above records the
original implementation run; the rebase checks are recorded separately below.
The five conflict resolutions affect documentation and test comments. They retain
the explicit-target implementation and both providers' complete-path assertions.
The incoming `text/x-yaml` serialization fix and revised module documentation
remain intact. RestClientIo keeps only `- initial release` in its release history;
its path contract remains documented in its introduction and API documentation.

|!Rebase check|!Status|!Evidence
|Module release-note policy|Pass|All 40 changed Qore sources pass the checker; all five checker unit tests pass.
|Conflict and whitespace checks|Pass|No conflict markers remain in the 87 changed files; git diff --cached --check passes.
|Test structure|Pass|All 44 changed tests retain executable permissions, %modern, and module-path setup before %requires.
|Scope and implementation review|Pass|No new native code, modules, registrations, dependencies, or placeholders; runtime path handling is unchanged by conflict resolution.
|Build and documentation extraction|Pass|Rebuilt RestClient, RestClientIo, FireworksRestClient, GroqRestClient, AnthropicRestClient, QoreTokenizerUtils, and QoreLlmUtils qmod targets with their dependencies. Strict Qdx extraction passes for both resolved REST modules; strict QPP accepts the release notes and audit table.
|Regression tests|Pass|All nine selected suites pass: RestClient, RestClientYamlMediaType, RestClientIo, RequestTargets, FireworksRestClient, GroqRestClient, OpenAiApiRoot, LlmStreaming, and HttpClientRequestUrl. RequestTargets retains all 533 assertions.
