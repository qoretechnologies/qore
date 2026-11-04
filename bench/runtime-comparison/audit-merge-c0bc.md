# Develop integration through c0bca49308

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the two incoming files and merge evidence. Evidence (`results-merge-c0bc.json`).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|8. No `%include` usage (deprecated for modules)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|9. Copyright 2026 on all new files|Pass|New merge evidence and audit carry 2026 copyright; incoming files retain their existing notices.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|Existing Qore module only; no new module, registration, QPP class or layout.
|13. `%modern` directive present|Pass|The modified Vertex fixture has %modern.
|14. Executable permission set (`chmod +x`)|Pass|The modified fixture remains executable (git mode 100755).
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|%prepend-module-path precedes the module requirements; the existing branch module-name imports are preserved to exercise AOT modules.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Requirements are delivered Qore modules, including the bundled json binary module; no new external dependency.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new filesystem/network primitive; the fixture reuses its existing localhost server and cleanup.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new filesystem/network primitive; the fixture reuses its existing localhost server and cleanup.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new filesystem/network primitive; the fixture reuses its existing localhost server and cleanup.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No new filesystem/network primitive; the fixture reuses its existing localhost server and cleanup.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ change, new blocking primitive or cancellation policy change; normal Qore loop checkpoints remain.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No C++ change, new blocking primitive or cancellation policy change; normal Qore loop checkpoints remain.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++ change, new blocking primitive or cancellation policy change; normal Qore loop checkpoints remain.
|24. No blocking operations without cancellation support|N/A|No C++ change, new blocking primitive or cancellation policy change; normal Qore loop checkpoints remain.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/app/schema/factory registration or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Missing terminal events now raise the complete intended error. The comment-only fake SSE body models finite EOF because an empty HttpServer SSE response selects a live socket handler; it is a fixture model, not a production workaround.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Partial output remains delivered before the incomplete-stream exception. Existing stream cleanup is preserved; on_exit resets fixture stream state, and globalTearDown removes the fake service-account file and deletes the local server.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|No new production shared mutable state. Fixture requests are sequential and request counters are checked after each completed HTTP operation; existing callback/event ownership is preserved.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Events use hash<ChatStreamEvent> and typed callback parameters/lists. Provider-native terminal hashes remain heterogeneous raw protocol data. Exception metadata uses the existing structured error convention.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|No new per-chunk traversal, copy or algorithm. Only existing EOF and transport-marker branches change from fabricated completion to an error; core call optimization sources remain byte-identical to the performance parent.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|EOF, empty streams, usage-only chunks and transport markers are covered with partial Unicode output and no silent retry; MAX_TOKENS and safety-block terminal events remain valid.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Public streaming documentation, @throw LLM-ERROR text and module release notes describe incomplete streams. No new public method requiring new examples or parameter docs.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags changed.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Static format strings receive provider labels through %s; no new credentials or external endpoints. The fixture reuses its local fake server and test key; live credential variables are excluded from qualification.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Optimized all-target build verifies 391 qmods and recompiles GeminiBodyShape. Eight QUnit fixtures report 83 cases, 82 executed, 1167 assertions; only the credential-dependent live Vertex case is skipped. All 1110 IR/AOT references pass. Core sources are unchanged; no new timing claim.
