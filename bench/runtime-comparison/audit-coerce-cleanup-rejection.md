# Cleanup-pointer annotation rejection audit (2026-10-31)

Copyright 2026 Qore Technologies, s.r.o.

All 62 checks individually resolved for the retained evidence-only diff. The production patch was rejected and restored.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new or modified product .qm/.qc module.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new or modified product .qm/.qc module.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new or modified product .qm/.qc module.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new or modified product .qm/.qc module.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new or modified product .qm/.qc module.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new or modified product .qm/.qc module.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new or modified product .qm/.qc module.
|8. No `%include` usage (deprecated for modules)|N/A|No new or modified product .qm/.qc module.
|9. Copyright 2026 on all new files|Pass|New evidence and audit carry Copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new or modified product .qm/.qc module.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new or modified product .qm/.qc module.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No retained QPP class or method change.
|13. `%modern` directive present|N/A|No retained test change; the candidate test diff is preserved only as rejected evidence.
|14. Executable permission set (`chmod +x`)|N/A|No retained test change; the candidate test diff is preserved only as rejected evidence.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|No retained test change; the candidate test diff is preserved only as rejected evidence.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|No retained test change; the candidate test diff is preserved only as rejected evidence.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|24. No blocking operations without cancellation support|N/A|No retained C++ or Qore source change; compiler annotation and test changes were restored.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider, factory, application, data type or JNI change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider, factory, application, data type or JNI change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider, factory, application, data type or JNI change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider, factory, application, data type or JNI change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider, factory, application, data type or JNI change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider, factory, application, data type or JNI change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider, factory, application, data type or JNI change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider, factory, application, data type or JNI change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider, factory, application, data type or JNI change.
|34. Response/output types use `private` Fields|N/A|No DataProvider, factory, application, data type or JNI change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider, factory, application, data type or JNI change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, factory, application, data type or JNI change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, factory, application, data type or JNI change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider, factory, application, data type or JNI change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider, factory, application, data type or JNI change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider, factory, application, data type or JNI change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, factory, application, data type or JNI change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, factory, application, data type or JNI change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, factory, application, data type or JNI change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider, factory, application, data type or JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, factory, application, data type or JNI change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider, factory, application, data type or JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, factory, application, data type or JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, factory, application, data type or JNI change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, factory, application, data type or JNI change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider, factory, application, data type or JNI change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, factory, application, data type or JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, factory, application, data type or JNI change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Retained change is an experiment record; rejected production/compiler/test edits are restored to 3d84. No stubs or runtime workaround retained.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|N/A|No retained implementation change; data/documentation only. Candidate contract review is recorded separately.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|N/A|No retained implementation change; data/documentation only. Candidate contract review is recorded separately.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|N/A|No retained implementation change; data/documentation only. Candidate contract review is recorded separately.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|468 optimized processes / 1404 windows retained, including symmetric confirmation. Indirect callback regression repeats in all layouts; reject instead of shipping a smaller but slower caller. Static counts are not cycle counts.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Actual waited process and stage statuses retained, including two nonzero control-review stages. The first wrongly used object-only compilation; the second omitted normalization of the explicit runtime identity descriptor. Third comparison passes, without bypassing runtime identity.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Document scope, all 18 pilot workloads, four paired controls, calibration limits, rejected candidate and compile-only ABI lead. No public API change.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No retained QPP class or method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Evidence contains no credentials or changed access checks; stored commands use fixed local paths. Production code restored.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Independently replay all468 process outputs/1404 windows and summaries, verify stream hashes, Release 2430 assertions, 4698 reference outputs per variant, disabled/pre-change code match and actual artifact disassembly. No Debug/short runtime qualification or Java parity claimed.
