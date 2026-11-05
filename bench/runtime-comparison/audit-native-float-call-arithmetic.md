# Native float callback arithmetic audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 checks cover compiler continuation, regression tests and evidence/documentation.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore library module.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore library module.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore library module.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore library module.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore library module.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore library module.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore library module.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore library module.
|9. Copyright 2026 on all new files|Pass|All new test, audit and evidence files carry Copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore library module.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore library module.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new QPP class.
|13. `%modern` directive present|Pass|Both new test files use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new test files are executable.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Fixture prepends the repository module path before requiring QUnit/FsUtil.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled QUnit/FsUtil are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operation is added to compiler lowering.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operation is added to compiler lowering.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No filesystem/network operation in the C++ change requiring a sandbox helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No File::, Dir::, Socket:: or HTTPClient:: use; the test uses an owned TmpDir and waits for its quoted compiler/test commands.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Existing cancellation checks cover the unchanged analysis-loop structure; one local callback set is added.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No cancellation check is removed or deprecated API introduced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Both existing compiler analysis loops retain qore_check_cancel checks every 100 analyzed blocks/instructions.
|24. No blocking operations without cancellation support|Pass|No blocking runtime operation is added.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|34. Response/output types use `private` Fields|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action, type, application, factory or JNI dependency changes.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete AOT float continuation using existing native-result machinery and generic fallback; no stub, TODO or partial ABI.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|The handled native path carries a raw double. The arbitrary boxed fallback keeps existing conversion, owned cleanup and exception handling. Callback exceptions, owned/borrowed results, throwing destructors and Program transitions are verified.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Only per-function/per-block compiler state changes. Existing runtime Program/binding/frame/context/stack guards and local invalidation remain; no shared runtime state is added.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Matching typed float callback, earlier producer in the same IR block and a single result use are required. Signature alone does not prove an actual float: the existing handled flag selects native data, otherwise getAsFloat conversion applies. Existing integer matching is unchanged.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|All 2076 optimized jobs/6228 windows are independently replayed. Three layouts and separate confirmation cover controls, mixed workloads and short cases under the explicit broader-workload tradeoff.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|The lowerer retains operand checks and ordinary ensureFloatType fallback when no native side channel exists. Pending exceptions and fallback ownership retain existing behavior.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Comments, design, release notes and results describe admission, conversion, ownership and measurement limits. No public runtime API is added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags changed.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled format string, unchecked index, credential or ABI change. Fixture commands quote paths and own temporary directories.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each pass 87 cases/8281 assertions plus two standalone scripts, with 2917 native assertions, fallback/context and lifetime coverage. Initial qualification has 32 source and 32 AOT executions. All six images pass 720 opaque references/probes; broader preparation passes 4770 native and 795 source references.
