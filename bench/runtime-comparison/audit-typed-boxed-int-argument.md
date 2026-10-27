# Checked boxed integer arguments for typed AOT callbacks

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the compiler/runtime/tests and measured evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No shipped Qore module/QPP class, registration or layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No shipped Qore module/QPP class, registration or layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No shipped Qore module/QPP class, registration or layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No shipped Qore module/QPP class, registration or layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No shipped Qore module/QPP class, registration or layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No shipped Qore module/QPP class, registration or layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No shipped Qore module/QPP class, registration or layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No shipped Qore module/QPP class, registration or layout change.
|9. Copyright 2026 on all new files|Pass|New measurement report, audit and archived diagnostic drivers/fixtures carry Copyright 2026; existing source copyrights retained.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No shipped Qore module/QPP class, registration or layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No shipped Qore module/QPP class, registration or layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No shipped Qore module/QPP class, registration or layout change.
|13. `%modern` directive present|Pass|Both changed shipped fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both shipped fixtures retain git mode100755 (checked).
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit/FsUtil are required after repository qlib prepend.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only Qore-delivered test modules required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No added production filesystem/network operation or test I/O.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No added production filesystem/network operation or test I/O.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No added production filesystem/network operation or test I/O.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No added production filesystem/network operation or test I/O.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new production loop or blocking operation; existing runtime cancellation checks remain.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new production loop or blocking operation; existing runtime cancellation checks remain.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new production loop or blocking operation; existing runtime cancellation checks remain.
|24. No blocking operations without cancellation support|N/A|No new production loop or blocking operation; existing runtime cancellation checks remain.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|34. Response/output types use `private` Fields|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency changes.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete exact integer and callable-capability guards; no stub or new TODO. Legacy v1/v2 and compiler-disabled path retain original routing.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|New helper borrows values, allocates nothing and cannot evaluate Qore code. It writes zero on miss. Original arg cleanup and exception paths remain. Shared output slot is loaded into SSA before the typed call overwrites it.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Cached AOT context and closure native entry use acquire loads. Concrete function-reference entries are immutable after publication. No new shared state, retained node or cache. All final Program/body/debugger/context checks remain.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only prepared exact int(int) sites with unknown representation emit the helper. NT_INT only, no coercion. NT_FUNCREF uses exact RTTI; runtime closure variant follows existing concrete invariant with debug assertion. Other types use ordinary binding.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Fixed-cost non-coercive admission, no allocation or new runtime loop. Wide gains repeat in three layouts and five-fork confirmation. Inline/effectful regressions, all broad/short/populated/disabled controls and superseded failures are disclosed. Retention uses the authorized broader workload tradeoff.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Unsupported values, missing/disabled entries, nonconcrete references and cold closures retain ordinary binding. Native-attempt misses retain existing typed fallback. No IO error handling changes.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal helper documents all parameters, return value, borrowed ownership and preliminary admission. Results, design and release notes document scope, control, benefits, costs and limits. No new public Qore method.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No added production IO, user format string, buffer/index manipulation or credentials. No context checks removed.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each: 33 QUnit runs, 216 cases, 8491 assertions plus three standalone fixtures; native 2917 and standard references 1110. Boundary/effect/ownership/EH/legacy cases pass. All 391 Release aliases restored. Independent replay verifies 4848 current jobs/14544 windows, 432 superseded jobs/1296 windows and 108 rejected alignment jobs/324 windows.
