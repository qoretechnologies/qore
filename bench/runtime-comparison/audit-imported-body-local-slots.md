# Imported AOT body-local storage audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover runtime, tests, documentation and evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new installed Qore library module or QPP class.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new installed Qore library module or QPP class.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new installed Qore library module or QPP class.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new installed Qore library module or QPP class.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new installed Qore library module or QPP class.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new installed Qore library module or QPP class.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new installed Qore library module or QPP class.
|8. No `%include` usage (deprecated for modules)|N/A|No new installed Qore library module or QPP class.
|9. Copyright 2026 on all new files|Pass|Copyright 2026 on both new regression files, audit, evidence and archived drivers.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new installed Qore library module or QPP class.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new installed Qore library module or QPP class.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new installed Qore library module or QPP class.
|13. `%modern` directive present|Pass|The executable qtest and generated Qore modules/hosts use %modern.
|14. Executable permission set (`chmod +x`)|Pass|The qtest is executable; it compiles and synchronously runs the native C++ probe.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib is prepended before bundled QUnit and FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled dependencies and test-generated modules are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No production or probe C++ filesystem operation is introduced.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operation is introduced.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem/network operation needing a helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|File/TmpDir operations create isolated compiler/module/probe fixtures. Shell arguments are quoted, subprocesses synchronously waited, and temporary files cleaned by TmpDir.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Production adds no loop. The native probe polls cancellation during its metadata scan; fixed fixture recursion is bounded at five levels and inner loops at five iterations.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|The probe uses qore_check_cancel; no deprecated API is added or existing runtime check removed.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|The metadata scan checks every 100 slots. Other probe loops and recursion are short and bounded; production adds no scan.
|24. No blocking operations without cancellation support|Pass|No new blocking primitive or Program transition elision. Existing stack/Program lifetimes, initialization and synchronization remain.
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
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|All three prepared-local families admit active imported execution. Unsupported locals retain null-handle fallback. No stub or special benchmark path.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Handles borrow storage only. Existing live evaluation, refcount/COW bookkeeping, cleanup, weak normalization and exception paths remain. Current-invocation instantiation dominates preparation, and nested stack growth preserves parent addresses.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|No shared mutable state is introduced. Immutable AOT metadata and the supplied live same-thread RuntimeConfig select invocation-local storage. The probe has unshared state and never mutates context ownership or native entries.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing index, declaration, exact LocalVar, finalized/boundary, captured/self/top-level and compiler body-membership checks remain. Runtime values and assignment acceptance are not cached. Fixture callbacks use code<int(int)>; casts follow native API types.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|All 2742 timing jobs / 8226 windows independently replay with all 29 confirmation controls and 15 same-binary attribution workloads. Nine LLVM pairs are byte-identical. Imported list mutation improves 33.03%, virtual calls 16.15–16.95%, member reads 9.39–14.85%. Local integer-member confirmation cost +1.31% and small repeated controls remain recorded. No weighted application, isolated-call-latency or Java parity claim.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Null context/config/Program, invalid index and unsupported storage retain fallback. Exact compiler instantiation dominance supplies current invocation identity; lookup alone crosses frame boundaries. Source mode, O0/O3, EH, disabled families and recursive restoration are tested.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Header, README, design, release notes and results document imported execution versus code ownership, borrowed slot lifetime, controls, measured costs and scope. No new public API or ABI.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No untrusted format strings, new unchecked production array accesses or credentials. Existing bounds/policy/access handling remains; fixture shell arguments are quoted. Archived environment fields use an explicit allowlist.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each pass 26 cases / 9790 assertions; Debug adds 2917 native assertions. The new 1735-assertion fixture passes unchanged baseline and candidate, including recursive admission counts, reflection/destructor reentry, Program-changing calls, class/weak/opaque/reference behavior and list mutation. Preparation passes 7020 native plus 2340 source checks; disabled controls add 14040. All 391 Release qmods restored. Targeted macOS qualification, not a full-project/sanitizer/other-platform claim.
