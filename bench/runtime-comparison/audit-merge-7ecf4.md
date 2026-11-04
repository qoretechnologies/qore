# Develop merge after direct-call context forwarding

Copyright 2026 Qore Technologies, s.r.o.

Merge origin/develop `7ecf4fcbab` into `b933fda977` cleanly. Gemini alternative enum shaping, UTC build-helper timestamps and provenance fixtures are validated against the optimized build. No performance timing claim for this merge. Evidence (`results-merge-7ecf4.json`).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped module, QPP class, registration or module layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped module, QPP class, registration or module layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new shipped module, QPP class, registration or module layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped module, QPP class, registration or module layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new shipped module, QPP class, registration or module layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new shipped module, QPP class, registration or module layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new shipped module, QPP class, registration or module layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No new shipped module, QPP class, registration or module layout change.
|9. Copyright 2026 on all new files|Pass|New audit/evidence drivers carry 2026 copyright; upstream edits add no new source file.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new shipped module, QPP class, registration or module layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new shipped module, QPP class, registration or module layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new shipped module, QPP class, registration or module layout change.
|13. `%modern` directive present|Pass|Changed executable QUnit tests use %modern; SandboxProvenanceFixture remains a module fixture.
|14. Executable permission set (`chmod +x`)|Pass|All three changed qtests retain executable permission (100755).
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Changed QUnit tests prepend repository qlib before requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Tests use shipped modules, existing local harnesses and the protobuf module under test.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem or network operation added; changed C++ helper is test-only.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem or network operation added; changed C++ helper is test-only.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem or network operation added; changed C++ helper is test-only.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|File timestamp operations use test-owned build fixtures with quoted paths. Localhost socket binding is an intentional policy fixture. Schema tests intercept requests with local capture objects; protobuf tests exercise policy with temporary files.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new C++ loop or blocking operation. New Qore schema loops retain automatic cancellation checkpoints.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new C++ loop or blocking operation. New Qore schema loops retain automatic cancellation checkpoints.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new C++ loop or blocking operation. New Qore schema loops retain automatic cancellation checkpoints.
|24. No blocking operations without cancellation support|N/A|No new C++ loop or blocking operation. New Qore schema loops retain automatic cancellation checkpoints.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|34. Response/output types use `private` Fields|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider registration, schema type, action, app, factory or JNI change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Clean automatic merge, no unresolved conflicts or incomplete implementation. Both incoming commits retained.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Test-only context helper adds manager references owned by QoreSandboxContext; its destructor releases them. Schema shaping uses Qore copy-on-write values and leaves caller input unchanged.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Sandbox fixture context is local to the test invocation and captures the current Program ID. No new shared mutable production cache. Schema shaping operates on local values.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Schema alternatives, properties, enum and items kinds are checked before recursive shaping. The test context records the Program ID needed for existing provenance semantics.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Schema relaxation visits actual alternative/property combinations and uses the existing enum merge helper; no new runtime call-path work. Call optimization sources are unchanged by this merge. No merge timing claim.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Regression coverage includes unconstrained/constrained union order, scalar and nested fields, arrays, closed/open/additional-properties alternatives, parent restrictions, allOf and unchanged input. UTC file timestamps avoid ambiguous DST hours.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Private schema helper documents parameters/return and parent constraints; UTC timestamp contract and test context ownership are documented. Incoming release notes retained; strict tables checked.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new external request path or production format string. Fixture credentials are dummy values, subprocess paths quoted, and socket target is localhost.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Optimized all-target build, 9 passing QUnit fixtures (95 cases/1651 assertions), 1 Release-only skip, 391 rebuilt qmods and 1110 IR/AOT reference checks. Waited processes and source/binary/qmod hashes verified. No Debug or full-suite claim.
