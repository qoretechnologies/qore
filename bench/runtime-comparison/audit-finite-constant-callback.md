# Native constant callback scalar replacement audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes skill checks cover this incremental compiler, test and evidence/documentation change.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore library module.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore library module.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore library module.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore library module.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore library module.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore library module.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore library module.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore library module.
|9. Copyright 2026 on all new files|Pass|New debugger fixture, audit and evidence carry Copyright 2026; modified test headers retain it.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore library module.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore library module.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new QPP class.
|13. `%modern` directive present|Pass|The modified qtest and both executable Qore fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|The modified qtest and fixtures are executable, including the new debugger regression.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The test prepends the repository module path before bundled QUnit/FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled modules are required; no external test dependency is added.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operation is added to compiler lowering.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operation is added to compiler lowering.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem/network operation requiring a helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No new Qore I/O; existing test temporary-directory/compiler execution is unchanged.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Analysis walks check qore_check_cancel every100 iterations, including operand collection, assignment analysis and CFG helpers. Prevalidated commit and restoration finish coherently while continuing cancellation polls; constant target loops are bounded to two.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No cancellation check is removed and no deprecated interruption API is introduced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|New tight scans poll every100 iterations; CFG/dependency/assignment helper checks are retained. Native source-loop cancellation checks remain in all measured LLVM bodies.
|24. No blocking operations without cancellation support|Pass|No blocking runtime operation is added.
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
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete conservative native-only rewrite with an unchanged fallback for unsupported bodies; no stubs, layout padding, TODOs or incomplete representation changes.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Instructions and restoration records use unique_ptr. Complete proof and replacement allocation precede mutation. Original instructions/facts and the complete pre-rewrite slot map are restored before debug serialization; compilation errors destroy owned records without touching freed IR. O0/O3, EH, split and ownership controls pass.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|All rewrite state is per compilation/body; dependency metadata uses existing compiler APIs. No mutable shared runtime state, TLS or ABI changes. Alias/capture/weak/AST-visible/reentrant/handler cases are excluded.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Exactly two known user constant targets must match required code<int()> or code<float()> with no arguments/defaults/variadics. Strong typed-code and enum representations, static_cast and checked maps/indices are used. Native facts carry exact type and reference-free ownership.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|1578 optimized jobs/4734 windows are independently replayed; three layouts, independent confirmation, compile-disabled attribution, broader controls and fresh Java are reported. Indexed use/write tables avoid scanning the body per local; no-creation preflight avoids analysis allocation for ordinary functions. Existing CFG/assignment/dependency analyses are reused. There is no new runtime lookup.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Any failed proof leaves the entire original representation. The new compilation flag and existing finite-target/import controls restore prior lowering. Deferred/imported, generic/preloaded, reassigned, escaping, effectful, unsupported-arithmetic and observable callback cases retain objects.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Comments, design, release note, README and results describe admission, disabling control, native-only restoration and measurement limits. No public API changes.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|New diagnostic format strings are fixed literals, array access is bounded to two admitted targets, and CFG/map keys come from validated IR. No credentials or external resource access. Existing test runners quote paths and wait for children.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each pass 47 cases/7450 assertions and one standalone script; 2917 native assertions and 18 cases/70 fallback controls. Expanded fixture has896 assertions across source/AOT configurations, including debugger attach/detach and local visibility; focused checks cover720 source and1620 native references with9 LLVM verifications; broader images pass5130 native and795 source references; fresh Java passes180 references and12 C2 diagnostics. No runtime identity bypass.
