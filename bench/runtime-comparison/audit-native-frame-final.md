# Native frame destructor devirtualization

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks apply to the single production declaration change, documentation and benchmark evidence. Baseline is merged performance commit `b83c2cdac1`, not pure develop. Raw results and provenance (`results-native-frame-final-macos-arm64.json`).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|9. Copyright 2026 on all new files|Pass|New evidence and audit drivers carry 2026 copyright. The production change modifies one existing declaration.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new Qore module, QPP class, module directive, registration or layout change.
|13. `%modern` directive present|N/A|No qtest is added or modified; existing targeted fixtures exercise the changed helper.
|14. Executable permission set (`chmod +x`)|N/A|No qtest is added or modified; existing targeted fixtures exercise the changed helper.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|No qtest is added or modified; existing targeted fixtures exercise the changed helper.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|No qtest is added or modified; existing targeted fixtures exercise the changed helper.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No filesystem or network operation is added or changed.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No filesystem or network operation is added or changed.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No filesystem or network operation is added or changed.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No filesystem or network operation is added or changed.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No loop or blocking operation is added or changed. Existing frame removal synchronization is preserved.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No loop or blocking operation is added or changed. Existing frame removal synchronization is preserved.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No loop or blocking operation is added or changed. Existing frame removal synchronization is preserved.
|24. No blocking operations without cancellation support|N/A|No loop or blocking operation is added or changed. Existing frame removal synchronization is preserved.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action, app, schema, field, factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Exactly one production change: mark the internal concrete native-frame class final. No conditional bypass, TODO, stub or benchmark-specific runtime branch.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|The destructor still restores the caller and then runs the unchanged default base destructor. No allocation, ownership or exception-path changes. Exception location and unwind fixtures pass.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Frame publication and removal retain the release store and active-reader synchronization; the generated removal helper contains both. Concurrent stack-inspection tests pass in IR and JIT execution. No mutable cache added.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|The class is defined only in JITRuntime.cpp and has no subclasses; final prevents future derivation. Native frame size remains 80 bytes and alignment 8 on this host. All exported helper signatures stay unchanged.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Actual machine code removes two vtable loads and the indirect branch, incorporating the existing restoration body. Release paired confirmation improves direct/callback loop kernels by 3.58%/3.56%. 52-workload screen and selected confirmation retain every sample; no repeated regression flag. No general throughput claim.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing null assertions, call-target fallback, stack checks and pending-exception/cancellation behavior are unchanged. Frame-location, stack-guard and callback tests pass; 1110 IR/AOT references and 4680 identical-executable before/candidate references match.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Results, current-state design notes and release notes describe the runtime-only change and measurement limits. No public method is added. A copied historical Java-plan status label is clarified by the publication note; actual goals/artifact paths/settings identify this experiment.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new input, indexing, format string, I/O or credential. The exact one-line change preserves existing bounds and policy behavior.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release -O3 -DNDEBUG all-target build and 391 qmods; seven focused QUnit processes, 17 executed cases, 451 assertions, no skips. 1110 IR/AOT reference results, 2340 references per frozen runtime, 90 Java references, C2/hot-inlining checks, and all 1134 timing jobs/3402 windows replayed.
