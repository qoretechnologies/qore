# Pre-entry function-reference decoding reuse

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover runtime, documentation and evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new distributed Qore module, QPP class, registration or module layout.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new distributed Qore module, QPP class, registration or module layout.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new distributed Qore module, QPP class, registration or module layout.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|8. No `%include` usage (deprecated for modules)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|9. Copyright 2026 on all new files|Pass|New evidence and drivers carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new distributed Qore module, QPP class, registration or module layout.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new distributed Qore module, QPP class, registration or module layout.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new distributed Qore module, QPP class, registration or module layout.
|13. `%modern` directive present|N/A|No test source is edited; existing Qore and native fixtures are reused.
|14. Executable permission set (`chmod +x`)|N/A|No test source is edited; existing Qore and native fixtures are reused.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|No test source is edited; existing Qore and native fixtures are reused.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|No test source is edited; existing Qore and native fixtures are reused.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem/network operation or Qore source edit.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem/network operation or Qore source edit.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem/network operation or Qore source edit.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No production filesystem/network operation or Qore source edit.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new production loop, blocking operation or cancellation point.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new production loop, blocking operation or cancellation point.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new production loop, blocking operation or cancellation point.
|24. No blocking operations without cancellation support|N/A|No new production loop, blocking operation or cancellation point.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider schema/action/app/factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete internal refactoring removes duplicate pre-entry reference decoding. No stub, padding, new diagnostic flag or eligibility workaround.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|The helper only reads existing binding facts. No allocation, ownership or fallible operation is added; Program/frame/parse-option RAII lifetimes and exception paths remain unchanged.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Reference and captured Program identity are borrowed from the same live argument. No Qore execution or Program transition occurs between exact decoding and reused pre-entry validation. Mutable debugger/body facts remain checked, including full validation after entry. No shared mutable cache is introduced.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|The helper accepts an already exact FunctionCallReferenceNode and captured QoreProgram pointer. Generic type, Program and RTTI checks retain their order; no new cast or public ABI.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Static inspection verifies removal of the repeated pre-entry decoder while preserving the complete post-entry decoder and avoiding an extra outlined helper in the general decoder. Paired optimized measurements and control costs determine retention, not instruction counts.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Global fast-call flag, native entry, owner/current Program restriction, typed signature/entry, captured/body-owner debugger checks and current native-body identity remain. Pending exceptions, stack limits, lazy materialization, fallback and context restoration are unchanged.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Design, release notes and evidence explain the reuse scope, measured gains, repeated control costs and limits. No public method or ABI is added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new I/O, credentials, format string, array indexing or compiler eligibility rule.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release {'fixtures': 21, 'cases': 52, 'assertions': 7498}; Debug {'fixtures': 18, 'cases': 30, 'assertions': 7388} plus native and non-QUnit checks. Standard 1110 and actual-module 6360 references pass. All 391 Release aliases restored. All 1962 common-module timing jobs/5886 windows and 1812 initial jobs/5436 windows independently replayed. No full-suite or sanitizer claim.
