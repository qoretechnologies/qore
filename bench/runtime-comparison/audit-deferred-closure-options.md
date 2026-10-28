# Deferred imported closure runtime options audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the runtime, regression, documentation and evidence changes.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new installed Qore library module or QPP class.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new installed Qore library module or QPP class.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new installed Qore library module or QPP class.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new installed Qore library module or QPP class.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new installed Qore library module or QPP class.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new installed Qore library module or QPP class.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new installed Qore library module or QPP class.
|8. No `%include` usage (deprecated for modules)|N/A|No new installed Qore library module or QPP class.
|9. Copyright 2026 on all new files|Pass|New audit, evidence and archived drivers carry Copyright 2026; expanded fixtures retain their existing copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new installed Qore library module or QPP class.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new installed Qore library module or QPP class.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new installed Qore library module or QPP class.
|13. `%modern` directive present|Pass|Expanded qtest and its generated module/host retain %modern.
|14. Executable permission set (`chmod +x`)|Pass|Expanded qtest remains executable; the harness compiles the C++ fixture.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib is prepended before bundled QUnit/FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled dependencies and the test-generated module are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No C++ filesystem operation is introduced.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operation is introduced.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem/network operation needing a helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|Existing File/TmpDir fixture operations create isolated source/module/probe files; quoted child commands are synchronously waited and TmpDir cleans up.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|No production loop is added. Expanded fixture loops are bounded to six controls; existing cancellation points remain.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No deprecated interruption API or removed cancellation check.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No new unbounded scan requiring a polling interval.
|24. No blocking operations without cancellation support|Pass|No new blocking runtime operation. Existing full Program transitions, capture locks and reader handshake remain.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/type/app/factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete lazy option setup/restoration under the existing v2 lowered-body proof; eager attribution is a complete alternative. No stubs, TODOs or skipped transitions.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Both potentially allocating variable markers are installed before option mutation. Optional snapshots are allocation-free and noexcept; later materialization operations do not throw. Materialized state alone enables restoration. Caller override is restored before applying saved runtime options. Context destruction follows frame unpublication and reader completion. Expanded tests cover overflow, pending exceptions, native rejection, foreign Program restoration and replacement of RuntimeConfig Impl.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Option state and optional storage are invocation/thread private. Compiler metadata remains immutable, entry loads retain acquire ordering, and live debugger checks remain. Diagnostic readers use the immutable frame, not the mutable options. Test-only entry wrappers run with an unshared context.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only existing fully proven int(int) v2 bodies qualify. Lowered-body proof permits the dominating stack guard, fixed three-pointer location slots and locked non-evaluating scalar capture reads; none observe runtime options on success. No new eligibility or native ABI; std::optional tracks snapshot lifetime explicitly.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|1122 jobs / 3366 windows independently replay across broad screen, fresh confirmation, same-binary eager attribution and supplemental same-Program controls. All 15 matched LLVM pairs are byte-identical. Successful imported calls avoid option snapshot/setup/restoration; no new heap allocation. Optional adds 40 bytes to all closure-v2 contexts, so its small control costs are reported, not assumed absent.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing body identity, owner, debugger, stack, capture, nullable metadata and fallback checks remain. Cold imported option setup precedes exception work; caller options restore even after actual Program transition. Eager control nests correctly and preserves native admission.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|README, design, release note and results explain the cold option contract, unchanged proof/ABI, eager runtime control, measured tradeoff and shared-host limits. No public API added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new format strings, unchecked indexing or credentials. Provenance, Program lifetime and debugger guards remain; fixture shell arguments are quoted.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release: 29 cases / 1513 assertions plus standalone. Debug: 10 cases / 1414 assertions plus standalone and 2917 native assertions. Expanded 382-assertion fixture also passes unchanged baseline, and covers O0/O3 stripped AOT, source modes, EH, five controls, wide arguments, capture fallback, body mismatch, both debugger Programs, real Program transitions and live option restoration. Benchmarks validate 10845 native/disabled and 3060 source references, with 30 LLVM verifications. All 391 Release aliases restored. Targeted macOS ARM64 validation; no full-project, sanitizer or other-platform claim.
