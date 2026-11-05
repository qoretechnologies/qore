# Fast AOT method floating conversion audit (2026-10-31)

Copyright 2026 Qore Technologies, s.r.o.

All 62 checks individually resolved: {"N/A": 41, "Pass": 21}.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new or modified product .qm/.qc module.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new or modified product .qm/.qc module.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new or modified product .qm/.qc module.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new or modified product .qm/.qc module.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new or modified product .qm/.qc module.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new or modified product .qm/.qc module.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new or modified product .qm/.qc module.
|8. No `%include` usage (deprecated for modules)|N/A|No new or modified product .qm/.qc module.
|9. Copyright 2026 on all new files|Pass|Existing fixtures and new evidence/audit files carry Copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new or modified product .qm/.qc module.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new or modified product .qm/.qc module.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No QPP class/method change.
|13. `%modern` directive present|Pass|Both modified native conversion fixtures use %modern; no deprecated redundant mode directives added.
|14. Executable permission set (`chmod +x`)|Pass|Both fixtures retain executable Git mode 100755.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The qtest prepends repository qlib before requiring QUnit/FsUtil; the qr fixture has no module dependency.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only Qore-delivered QUnit/FsUtil dependencies are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Compiler diff adds no filesystem operation; object emission uses its existing checked path.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operation added.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No added C++ filesystem/network operation.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|The test uses an owned FsUtil TmpDir for compiler outputs and the existing quoted, waited child runner; no new File/Dir/Socket/HTTPClient operation in product code.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|The only added C++ loop walks helper uses and checks qore_check_cancel every 100 visits, including skipped uses.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|Uses qore_check_cancel(nullptr, operation); no deprecated polling function.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Tight linear compiler-use traversal checks every 100 iterations. No expensive nested or blocking loop added.
|24. No blocking operations without cancellation support|Pass|No new blocking runtime/compiler operation; regression child commands are synchronous and waited.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action, data type, app, factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete destination-aware lowering with an explicit source enum and fast-entry compiler state. No TODO/stub, symbol-name heuristic or benchmark-specific dispatch. Broader candidate scopes remain rejected.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|LLVM owns inserted instructions/declarations; the use iterator advances before erasure and replacement has the same i64 type. No new Qore allocation, ownership transfer or xsink operation. Cancellation sets the emission error and releases the existing target machine before returning.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|The pass modifies only its compilation-owned module before parallel object splitting. NativeCallSource is per-lowering-function state. No mutable shared runtime state or new cache.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Strong enum distinguishes method and callback results; helper signature is validated as non-vararg double-to-i64, and only direct CallInst users are replaced. Musttail and operand-bundle calls are excluded.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Linear compilation pass; no added runtime guard/allocation. Seven epochs retain 2070 processes/6210 windows, including 216/648 identical-binary A/A controls. Final scope 774/2322: local floating method-result gain 6.90% pilot and 6.37% paired, all layouts; 32-item target improves 1.38%. Paired floating callback control costs 1.67%, one-item target 0.66%, one-item wide integer 1.12%; all controls retained. No weighted application-wide gain claimed.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Unsupported targets and malformed/nondeclaration helper signatures preserve existing lowering. Cancellation propagates an explicit compilation error. Every qualification/timing process has a waited receipt; harness failures are preserved separately.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|No public API added. Document AArch64 fast-entry scope, exceptional-value semantics, compile-time disable control, rebuild requirement, rejected scopes and all control costs. Strict Qore table checks required before commit.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP class/method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No credentials, variable format strings, unchecked array indexing or access-boundary bypass. Existing Program/debugger/class/context/stack/ownership checks are unchanged.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release: six fixtures/2025 assertions. Debug: native conversion fixture/1182 assertions. AST/IR/JIT/tiered/AOT O0/O3, boxed fallback, exceptions, destruction and foreign Programs covered. 100037 bit patterns match actual Release/Debug FCVTZS helpers and the saturating reference. Cross-target ARM/x86 objects verified at O0/O3; no foreign-target execution claimed. 36 artifacts pass 4698 reference outputs per variant. All 2070 processes/6210 windows replay, including 216/648 identical-binary A/A controls; 774/2322 belong to the final candidate. Short count 32 is timing-only, not an additional reference check.
