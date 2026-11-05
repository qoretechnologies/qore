# Execution-context merge audit (2026-10-28)

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the merge of origin/develop 6becf04083 into feature/performance 553a8a8dd1, including the three conflict resolutions and callback-probe setup adjustment. No performance numbers from before this merge are presented as measurements of the merged runtime.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore library module or QPP class.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore library module or QPP class.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore library module or QPP class.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore library module or QPP class.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore library module or QPP class.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore library module or QPP class.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore library module or QPP class.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore library module or QPP class.
|9. Copyright 2026 on all new files|Pass|All new upstream qtests and this audit carry Copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore library module or QPP class.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore library module or QPP class.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new Qore library module or QPP class.
|13. `%modern` directive present|Pass|All four added/modified qtests use %modern.
|14. Executable permission set (`chmod +x`)|Pass|All four qtests have executable permission; C++ test source is compiled by its existing executable harness.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit, FsUtil and ExecModeHarness requirements follow the repository module-path directive.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled modules and the repository harness are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No new C++ filesystem operation; class/name resolution and module-loading policy use the existing loader.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No new C++ network operation.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem/network operation needing a new helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|New Qore filesystem calls create isolated TmpDir fixtures and exercise denied module I/O intentionally; children are synchronously waited through ExecModeHarness.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|The merged production changes add no unbounded scan. Existing loops and cancellation checks remain; the added metadata section reads/writes two words.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No deprecated cancellation API is introduced and no cancellation check is removed.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No new loop requires a polling interval; existing intervals remain.
|24. No blocking operations without cancellation support|Pass|No blocking operation is introduced by the merge resolution.
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
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|No stubs, TODOs or incomplete dispatch paths. Exact constructor signatures and parsed reference slots are preserved; deferred names adopt upstream context resolution.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Constructor ownership remains in ReferenceHolder/unique_ptr. Load-time and call-time parse options use scoped RAII. Optional argv retains its existing ownership; native helper signatures match every lowering path.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|New override/loading context is thread-local and restored by RAII. Program domain metadata is written under existing parse/load contexts before publication. Native cached proofs and ownership guards are unchanged.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing strongly typed native callback bindings remain. Untyped callbacks in upstream regressions intentionally exercise generic dispatch. The C++ probe preserves differing caller and body-owner options by enabling the caller option after module loading.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|No new quadratic scan or copy on a hot path. Upstream adds bounded context setup required for AST-equivalent semantics. Existing scalar optimizations remain; no new merged-runtime speedup is claimed before fresh measurements.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Unresolved builtin variants retain runtime fallback; explicit serialized constructor signatures never fall back to another variant. FUNCTIONAL_DOMAIN size is validated before reads. Test option changes check the exception sink.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal helper comments document lookup Program versus executing Program, scoped option restoration, and inherited module restrictions. No new public Qore language API; old AOT artifacts are rebuilt for the matching runtime identity.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Fixed diagnostic format strings, bounded metadata reads, unchanged object/provenance/stack checks, no credentials. Tests quote paths and wait for child processes.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release: 42 cases/3630 assertions plus one standalone fixture. Debug: 21 cases/1346 assertions; 2917 native assertions. All 391 Release qmod aliases restored. Incoming module-context regressions and branch constructor/callback/debugger/Program/ownership controls pass. Initial stale test assumption and correction are archived.
