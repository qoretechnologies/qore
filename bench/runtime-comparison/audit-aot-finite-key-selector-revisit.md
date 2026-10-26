# Finite literal hash-key projection and selector elision

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover compiler, fixtures, documentation and evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new distributed module, QPP class, registration or module layout.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new distributed module, QPP class, registration or module layout.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new distributed module, QPP class, registration or module layout.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new distributed module, QPP class, registration or module layout.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new distributed module, QPP class, registration or module layout.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new distributed module, QPP class, registration or module layout.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new distributed module, QPP class, registration or module layout.
|8. No `%include` usage (deprecated for modules)|N/A|No new distributed module, QPP class, registration or module layout.
|9. Copyright 2026 on all new files|Pass|New semantic fixture, QUnit harness, evidence and drivers carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new distributed module, QPP class, registration or module layout.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new distributed module, QPP class, registration or module layout.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new distributed module, QPP class, registration or module layout.
|13. `%modern` directive present|Pass|Both new Qore fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both fixtures are executable and have the Qore shebang.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit harness prepends repository qlib before bundled module dependencies; semantic fixture has no module dependencies.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled QUnit and FsUtil are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem or network operation added.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem or network operation added.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem or network operation added.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir and controlled temporary compiled artifacts are justified regression scaffolding. Child arguments are quoted and children are awaited.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|All new unbounded key/root/use/local/merge loops call qore_ir_analysis_cancelled; bounded predecessor/literal loops have at most 4 elements and forwarding has at most 9 visits.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|The existing IR analysis cancellation helper invokes qore_check_cancel, not the deprecated interrupt check.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|The existing helper checks every 100 iterations; no new blocking operation or runtime loop polling change.
|24. No blocking operations without cancellation support|N/A|No new blocking operation.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete conservative proof with ordinary fallback for ineligible graphs; no stub, placeholder, padding or alignment tuning.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Per-analysis standard containers use C++ RAII. No new Qore node allocation or runtime exception boundary. Candidate replacements merge only after complete aggregate acceptance, retaining existing snapshots and cleanup.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|New analysis state is local to a function pass. No shared mutable runtime cache, thread state or lifetime synchronization change.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Exact plain integer-valued hash, assigned/non-NOTHING values and inline integer bounds; native integer PHIs carry explicit facts. Selector strings are exact, private and single-store. Current otherwise-written exclusions remain.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Key sets validated once per candidate, bounded edge/forwarding proof, exclusive literal uses avoid rescanning shared constants. All roots use the same eliminated set. Optimized three-variant randomized timings and control costs are retained.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Reject incomplete keys, NUL keys, mismatched predecessor sets, backedges, exceptional reentry, unsupported forwarding and observable/escaped selector uses. Missing or unsupported cases retain ordinary allocation and lookup.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Design and release notes explain bounded finite-key projection, selector-use proof, compiler disable controls, measurements and limits. No new public API or ABI.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, credentials or external format string. Operand counts, predecessor identities and key presence are checked; existing container bounds and provenance proofs remain.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release {'fixtures': 14, 'cases': 36, 'assertions': 5291}; Debug {'fixtures': 9, 'cases': 9, 'assertions': 5053} plus non-QUnit/native checks. Focused 512 assertions in each build; AST/IR/JIT/tiered/O0/O3/EH/stripped/split/control variants. Standard 1110 references and 9540 prepared module references pass; all 391 Release aliases restored. 2574 broad timing jobs/7722 windows and 24 diagnostic jobs/72 windows independently replayed. Historical rejection preserved. No full-suite or sanitizer claim.
