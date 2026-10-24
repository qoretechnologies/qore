# AOT native scalar snapshot audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks reviewed before commit. [Evidence](results-aot-native-refself-macos-arm64.json).

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped module, QPP class, module layout or build registration.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped module, QPP class, module layout or build registration.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new shipped module, QPP class, module layout or build registration.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped module, QPP class, module layout or build registration.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new shipped module, QPP class, module layout or build registration.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new shipped module, QPP class, module layout or build registration.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new shipped module, QPP class, module layout or build registration.
|8. No `%include` usage (deprecated for modules)|N/A|No new shipped module, QPP class, module layout or build registration.
|9. Copyright 2026 on all new files|Pass|Both new executable test fixtures have 2026 copyright and SPDX headers; new audit and evidence have 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new shipped module, QPP class, module layout or build registration.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new shipped module, QPP class, module layout or build registration.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new shipped module, QPP class, module layout or build registration.
|13. `%modern` directive present|Pass|Both new test fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new test fixtures are executable 0755.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|AOTNativeRefSelf.qtest prepends the repository module path before shipped QUnit/FsUtil and local ExecModeHarness; the qr requires no module.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only shipped modules/local test harness; no external dependency introduced.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir and ExecModeHarness manage temporary compiled artifacts and waited subprocesses. No new external I/O.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
|24. No blocking operations without cancellation support|N/A|No new C++ I/O, loop, blocking operation or cancellation point. Existing runtime cancellation remains.
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
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete AOT-only native float/bool snapshot implementation with physical LLVM type proof and a compile-disable control. No TODO, stub or runtime workaround.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Native scalar SSA values own no references and preserve the evaluated value across cleanup. Existing boxed boundaries create their own ownership; Return materializes before cleanup. No new owning allocation or fallible operation.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Only per-compilation values map state changes; no mutable shared runtime state or ABI added.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Eligibility uses physical double/i1 rather than declared types alone. Native i64 and boxed i64 remain unchanged because existing consumers distinguish ownership differently. Typed callbacks in tests; no new cast.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|One bounded compile-time condition eliminates unnecessary boxing/refcount work; no added runtime check. Optimized before/candidate/disabled timings, control costs and broad 55 results are retained.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing missing-operand/getVal failures and return validation remain. Tests cover optional values, catch/on_exit cleanup, thrown cleanup, reference mutation, owned-object destruction, signed zero, tag-colliding finite doubles, infinities, NaNs, wide integers and boxed values.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|No public API or ABI change. Internal rationale, release notes, RESULTS/current-state, compile control, units and limits are documented; four strict Qore-table checks pass.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new format-string inputs, buffer operations, credentials, array indexing or external access. Archived outputs omit environment dumps; benchmark inputs are fixed and validated.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release and Debug each pass 38 selected fixtures: 26 QUnit fixtures, 12 standalone, 18,330 QUnit assertions and 8 Linux RSS skips per build. All 391 qmods/build, 2,220 IR/AOT references and 11,756 native Debug assertions pass. 1,950 target and 7,047 broad artifact references pass; all fixed timed processes/windows are independently replayed.
