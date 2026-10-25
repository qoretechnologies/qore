# Shared AOT callback inlining audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks reviewed before commit. [Evidence](results-aot-shared-callback-inline-macos-arm64.json).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration, directive or layout change.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration, directive or layout change.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module, QPP class, registration, directive or layout change.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new module, QPP class, registration, directive or layout change.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module, QPP class, registration, directive or layout change.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new module, QPP class, registration, directive or layout change.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new module, QPP class, registration, directive or layout change.
|8. No `%include` usage (deprecated for modules)|N/A|No new module, QPP class, registration, directive or layout change.
|9. Copyright 2026 on all new files|Pass|New tests, audit, report and reproduction drivers carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new module, QPP class, registration, directive or layout change.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new module, QPP class, registration, directive or layout change.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration, directive or layout change.
|13. `%modern` directive present|Pass|Both new fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new fixtures have executable permission.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib path precedes QUnit, FsUtil and local ExecModeHarness requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only shipped modules and the in-repository test harness are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new production filesystem or network operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new production filesystem or network operation.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new production filesystem or network operation.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir and ExecModeHarness own temporary artifacts and waited children. Workers are cancelled and awaited; a failed bounded wait exits the test process.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|No new compiler loop. Existing bounded admission remains at 32 blocks/512 instructions per target and 64 direct targets; inlining preserves generated cancellation checkpoints.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No deprecated cancellation API added. Existing generated explicit-context cancellation helpers remain.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Inlining leaves live cancellation loads, atomic ordering and checks intact; tests observe the target frame before cancelling and verify recovery.
|24. No blocking operations without cancellation support|N/A|No new blocking production operation.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/app/action/schema/factory/JNI or dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete call-site policy and minimum alignment, with compile-time disable control. No TODO, stub or unconditional callee-wide inlining.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|LLVM owns the call and function objects. No new runtime allocation or borrowed lifetime. Existing fallback, frame pop, argument cleanup and exception tests remain.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|The one-site budget is compiler-instance state, reset on every lowerFunction. No shared runtime cache or new synchronization.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Both callback and direct admission flags plus a proven callback parameter/target are required. Existing same-Program zero/unary native-float proofs and exclusions remain. Self-recursive call sites are excluded.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|At most one forced site per private specialization; direct callers retain LLVM cost policy. Final callback gain 5.48%; repeated integer-layout and broad control costs remain disclosed under the authorized broader tradeoff. Code size/stack growth recorded.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|No new fallible runtime operation. A declared companion is checked before calls; alignment follows successful declaration. Disabled, null-context, EH and split controls pass.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Design, release notes and results describe standalone AOT scope, private compiler layout, disable control, alignment limits, measurements and remaining Java gap. No public API added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, user-controlled format, credentials or array indexing. Subprocess arguments are quoted; existing parameter/type bounds remain.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release all-target build and 391 qmods; 15 fixtures (14 QUnit plus standalone), 35 cases, 1827 assertions, zero skips and 1110 IR/AOT references. Final target/broad/Java preparations check 3510/7047/90 values. All 2538 jobs/7614 windows from initial, diagnostic and final trials replayed. No full-suite, Debug, sanitizer or cross-platform claim.
