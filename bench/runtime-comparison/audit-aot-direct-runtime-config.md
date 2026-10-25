# Same-program AOT direct RuntimeConfig audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks reviewed before commit. [Evidence](results-aot-direct-runtime-config-macos-arm64.json).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration, module directive or layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration, module directive or layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module, QPP class, registration, module directive or layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new module, QPP class, registration, module directive or layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module, QPP class, registration, module directive or layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new module, QPP class, registration, module directive or layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new module, QPP class, registration, module directive or layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No new module, QPP class, registration, module directive or layout change.
|9. Copyright 2026 on all new files|Pass|New tests, audit, evidence and reproduction files carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new module, QPP class, registration, module directive or layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new module, QPP class, registration, module directive or layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration, module directive or layout change.
|13. `%modern` directive present|Pass|Both new standalone and QUnit fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new fixtures have executable permission, checked before publication.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib path precedes shipped QUnit/FsUtil and local ExecModeHarness requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only shipped modules and the in-repository test harness are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem or network operation introduced.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem or network operation introduced.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem or network operation introduced.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir and the harness own and clean temporary AOT artifacts. The fixture observes a target frame, cancels a bounded worker, waits for completion and checks recovery. A failed wait exits the whole test process.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|New compiler indexing/candidate/block/instruction scans check qore_check_cancel every 100 visits. Companion body inspection has at most 32 blocks; parameter vectors have at most three original arguments. Generated loop checkpoints remain.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|New scans use qore_check_cancel; generated calls retain qore_rt_check_cancel_v2. No deprecated cancellation API.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Only live addresses are forwarded. Every stack check, cancellation checkpoint, atomic ordering and exception test remains. Cancellation after observing the target frame and subsequent recovery are tested.
|24. No blocking operations without cancellation support|N/A|No new blocking production operation.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/app/action/schema/factory/JNI or build dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete bounded admission and shared lowering, with disable controls and null-context fallback. No stub or unfinished runtime contract.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|LLVM owns generated functions/storage; vectors and unique_ptr own compiler temporaries. Borrowed context and descriptor remain valid through synchronous calls. Existing frame pop, argument cleanup and exception paths remain.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Compiler-local maps and admission fields. Runtime state is borrowed from the same thread and program; no new shared mutable cache or retained caller pointer.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Exact zero/unary native float, single-variant, context-independent admission excludes methods, generics, preloaded entries and reference arguments. Lowering checks the resolved target and same Program. Null context uses ordinary ABI.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Linear indexing and scans with a 64 direct-target budget; existing callback budget remains separate. At most one companion per target. Final direct loop time decreases 47.57%; repeated callback, integer-layout and broad indirect-call costs are retained and disclosed under the authorized workload tradeoff.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Audit reproduced a compiler null dereference for late-resolved optional/auto locals. Lowering now uses aot_direct_variant. Baseline and fixed compiler succeed; prototype failed with exit139. Regression fixture covers all three forms; final source rebuilt and remeasured.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal helper and flags document borrowed state/admission. Design, release notes and results cover standalone AOT scope, limits, controls, private C++ layout, measurements and remaining Java gap. No public API was added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags changed.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, credentials or user-controlled format. Admission bounds types/count; descriptor indices remain fixed 0..5. Tests quote subprocess arguments.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Final Release all-target build and 391 qmods; 14 fixtures, 34 QUnit cases, 1736 assertions, zero skips, 1110 IR/AOT references. Eighteen additional mode/control executions include audit regressions. Target/broad/Java checks:3510/7047/90 per campaign. All 2988 jobs/8964 windows replayed, including fresh final source timings. No full-suite, Debug, sanitizer or cross-platform claim.
