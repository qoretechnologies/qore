# Guarded same-list publication audit (2026-10-31)

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks resolved. Optimized before/after timing, Release and Debug regressions, native ownership/error checks and raw-output replay pass.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No Qore product module added or changed.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No Qore product module added or changed.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No Qore product module added or changed.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No Qore product module added or changed.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No Qore product module added or changed.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No Qore product module added or changed.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No Qore product module added or changed.
|8. No `%include` usage (deprecated for modules)|N/A|No Qore product module added or changed.
|9. Copyright 2026 on all new files|Pass|Both new Qore fixtures and the evidence drivers carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No Qore product module added or changed.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No Qore product module added or changed.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No QPP class or method changes.
|13. `%modern` directive present|Pass|AOTSameListPublication.qtest and its .qr fixture use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new fixtures have executable mode.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The qtest prepends the in-repository qlib path before QUnit and FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled QUnit and FsUtil are required; no external module dependency added.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Runtime changes perform no filesystem operations.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|Runtime changes perform no network operations.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No runtime I/O, new cancellation point or unbounded C++ loop.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|The test uses TmpDir and waited backquote children to compile/run temporary fixtures; no product I/O added.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Runtime selection/helper paths add no loops; the new native unit loop has exactly three declarations. Language-loop cancellation and existing cleanup paths remain intact.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No runtime I/O, new cancellation point or unbounded C++ loop.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No runtime I/O, new cancellation point or unbounded C++ loop.
|24. No blocking operations without cancellation support|Pass|No blocking operation added to runtime C++ code. Test subprocesses finish synchronously and temporary artifacts have scoped cleanup.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, data-type or JNI changes.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider, app, factory, data-type or JNI changes.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider, app, factory, data-type or JNI changes.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|34. Response/output types use `private` Fields|N/A|No DataProvider, app, factory, data-type or JNI changes.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, data-type or JNI changes.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider, app, factory, data-type or JNI changes.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, data-type or JNI changes.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, data-type or JNI changes.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, data-type or JNI changes.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider, app, factory, data-type or JNI changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, data-type or JNI changes.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, data-type or JNI changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, data-type or JNI changes.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, data-type or JNI changes.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider, app, factory, data-type or JNI changes.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, data-type or JNI changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, data-type or JNI changes.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete guarded helper with the original ordinary assignment fallback; no TODO, stub, or workaround retained. Existing reflection type differences are recorded separately.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Input remains borrowed; same-root success changes no ownership. Replacement/weak normalization uses the original holder-based helper. Pending sinks prevent publication and native-EH wrapper throws with sink intact. Coercion and caller cleanup/error edges match prior LLVM.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Prepared slot is acquired after body instantiation and scoped to the same thread/invocation; exact local identity and live flags are checked. No shared mutable cache or retained AOT context introduced.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Opaque and weak/reference inputs cannot pass strong identity. Integer-list declaration and live container type are checked. New casts use static_cast; test closure and exception types are explicit.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time guarded publication removes generic lvalue setup/refcount traffic. 1044 optimized processes/3132 windows; broad mutation -19.29% local/-19.75% imported, callback pipeline -15.46%; ordinary indirect +1.12% weighed against broader gains.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Compiler-controlled indices retain the existing wrapper assertion contract. Null prepared storage, mismatched identity, owning replacement, nonadmitted declarations, cleared/invalid storage and pending exceptions retain ordinary handling.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal helper parameters, borrowed ownership, invocation lifetime and fallback behavior documented in JITRuntime.h/LocalVar.h; release notes and benchmark methodology/results describe the behavior. No public language method or QPP API added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP class or method changes.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled format, unchecked external index, credential, or new external resource access. The helper validates prepared identity and actual representation before dereferencing the list.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release: 7073 assertions; Debug AOT regressions: 5731; native Debug suites: 4033 and 5718 (overlapping suites). Each optimized variant passes 4698 reference outputs. All 1044 timing processes/3132 measured windows replay; 864 short workload warmup/measured checksums match independent references. Final optimized runtime is byte-identical to the measured candidate.
