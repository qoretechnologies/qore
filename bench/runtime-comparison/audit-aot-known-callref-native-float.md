# Known callback native float argument audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks reviewed before commit. Evidence (`results-aot-known-callref-native-float-macos-arm64.json`).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped module, QPP class, module layout or build registration.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped module, QPP class, module layout or build registration.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new shipped module, QPP class, module layout or build registration.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped module, QPP class, module layout or build registration.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new shipped module, QPP class, module layout or build registration.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new shipped module, QPP class, module layout or build registration.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new shipped module, QPP class, module layout or build registration.
|8. No `%include` usage (deprecated for modules)|N/A|No new shipped module, QPP class, module layout or build registration.
|9. Copyright 2026 on all new files|Pass|New fixtures, audit and evidence carry 2026 copyright; source/test files include SPDX headers.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new shipped module, QPP class, module layout or build registration.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new shipped module, QPP class, module layout or build registration.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new shipped module, QPP class, module layout or build registration.
|13. `%modern` directive present|Pass|Both new fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new fixtures are executable (0755).
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The harness prepends the repository module path before QUnit, FsUtil and ExecModeHarness; the behavior fixture needs no modules.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only shipped modules and the local test harness are required; no external dependency.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++ filesystem or network operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No C++ filesystem or network operation.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem or network operation.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir owns temporary outputs; ExecModeHarness waits for subprocesses. Filesystem use is confined to the compiler regression test.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|No new loop. Existing argument analysis retains qore_check_cancel every 100 iterations; generated stack and cancellation checks remain.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new cancellation call or blocking operation; surrounding cancellation checks remain.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Existing every-100 argument scan checks are retained; added work per operand is bounded map/type lookup.
|24. No blocking operations without cancellation support|N/A|No new cancellation call or blocking operation; surrounding cancellation checks remain.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/JNI/action/type/app/factory or dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete conservative native-float proof with compile-disable control; no TODO, stub, runtime workaround or ABI change.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|No new allocation or runtime cleanup path. Sparse/optional boxed provenance keeps existing guards, argument cleanup and exception paths; observable-callee exception checks pass.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|The new flag is local to lowering one call. Existing per-lowering value maps are only read; no mutable shared runtime state or publication protocol changes.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only an already-lowered LLVM double matching NativeFloat is admitted, without NaN-boxed or preserved boxed provenance. Existing exact arity, no defaults/varargs/named/references and eligible same-program callee rules remain. Typed callbacks are used in fixtures.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time compile-time checks replace per-call boxing/dispatch for eligible arguments. Independent target confirmation gives 14.26x; broad 55-workload screening has no flags. Repeated default-layout control +5.8% / +0.44 ns is retained and disclosed under the authorized overall tradeoff.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Unrecognized/boxed/optional/sparse arguments retain the existing binding/fallback checks. Only all-proven native floats avoid the NOTHING guard; mixed arguments still use guards. Assignment, mutation and exception fixtures pass.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|No public API/ABI change. Source rationale, scope, disable control, measurements, adverse results and limits are documented; strict documentation table checks are recorded separately.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new I/O, user-controlled formatting, credentials or unvalidated index access. The patch only inspects existing operands and types; benchmark evidence contains no environment dump.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release rebuilds 391 qmods and passes 18 fixtures: 15 QUnit / 3 standalone, 6,137 assertions, eight Linux RSS skips. 1,110 IR/AOT, 3,510 target and 7,047 broad references pass; all 2,394 timed processes / 7,182 windows are independently replayed. Native selection, actual fast execution, O0/O3/EH/split and disabled control are verified. No full-suite, Debug, sanitizer or cross-platform claim.
