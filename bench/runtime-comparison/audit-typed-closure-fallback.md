# Typed closure fallback after prepared native admission

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the compiler/runtime/test changes and measured evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|9. Copyright 2026 on all new files|Pass|New report, audit and archived drivers carry Copyright 2026; existing source copyrights retained.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new shipped Qore module, QPP class, module registration or layout change.
|13. `%modern` directive present|Pass|Both changed executable fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both changed fixtures retain mode 100755.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit and FsUtil resolve through the existing prepended repository qlib path before %requires.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only Qore-delivered modules are required; no new external dependency.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No added production filesystem/network operation or test I/O change.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No added production filesystem/network operation or test I/O change.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No added production filesystem/network operation or test I/O change.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No added production filesystem/network operation or test I/O change.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new production loop/blocking operation; existing cancellation checkpoints remain. Duplicated template body retains the existing bounded argument loop.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new production loop/blocking operation; existing cancellation checkpoints remain. Duplicated template body retains the existing bounded argument loop.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new production loop/blocking operation; existing cancellation checkpoints remain. Duplicated template body retains the existing bounded argument loop.
|24. No blocking operations without cancellation support|N/A|No new production loop/blocking operation; existing cancellation checkpoints remain. Duplicated template body retains the existing bounded argument loop.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/type/app/factory or JNI/JAR dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete versioned fallback helpers and compiler routing; no stub/TODO. Argument misses and legacy v1/v2 retain the original helper. The earlier incorrect performance routing is superseded and documented.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Same boxed ABI, ReferenceHolder list ownership, cleanup arrays and sink checks. Both throwing variants mirror existing QoreJITException wrappers. LLVM PHIs use actual post-invoke predecessor blocks. Owned invalid argument destruction is checked exactly once.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|No new shared state or retained closure pointer. Existing captured-context RAII, acquire loads, capture synchronization, Program counts/locks/TLPD and caller identity remain. Compiler booleans and blocks are local.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Restricted to prepared int(int) helpers v3 and later after an actual scalar attempt. Compatibility false retains original binding/native wide-integer path. Strongly typed callbacks and existing boxed ABI; four additive internal C exports, no object/frame layout change.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Both primary repeated-call targets pass screening and five-fork confirmation in every layout. All costs, short-call failures, generic/populated controls and the superseded 2.62x wide-integer regression are disclosed. Fixed dispatch cost, no new runtime loop/allocation. Retention uses the authorized broader tradeoff.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|All original type/signature, stack, debugger, exception, context and generic fallback checks remain. Skip only the redundant optional native attempt after prepared admission misses; full Program entry and its native retry remain.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Header and implementation document the internal helper precondition and boxed ownership ABI. Results, design and release notes explain scope, additive exports, compiler control, measured benefits/costs and limitations. No new public Qore method.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, format string, buffer/index manipulation or credential. Full invocation context remains. Publication checks high-confidence credential patterns without echoing values.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each: 33 QUnit runs,216 cases,8403 assertions plus three standalone fixtures. Native 2917, standard 1110, paired 9024, generic 3780, populated 3780 and boxed 432 references pass. All 391 Release aliases restored. All 3636 candidate jobs/10908 windows and 288 superseded jobs/864 windows independently replayed. New cases cover argument miss, wide int, invalid values, cleanup, EH and legacy helpers.
