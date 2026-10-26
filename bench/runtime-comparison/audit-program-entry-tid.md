# Explicit thread ID at Program entry

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover the retained runtime/test changes, documentation and evidence. The earlier snapshot-caching candidate is rejected and preserved separately in the report.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module, QPP class, module registration or module layout.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore module, QPP class, module registration or module layout.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore module, QPP class, module registration or module layout.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore module, QPP class, module registration or module layout.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore module, QPP class, module registration or module layout.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore module, QPP class, module registration or module layout.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore module, QPP class, module registration or module layout.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore module, QPP class, module registration or module layout.
|9. Copyright 2026 on all new files|Pass|New audit, evidence and experiment drivers carry 2026 copyright; modified C++ files already do.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore module, QPP class, module registration or module layout.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore module, QPP class, module registration or module layout.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new Qore module, QPP class, module registration or module layout.
|13. `%modern` directive present|N/A|New regression assertions are C++ native tests in the existing Debug-only test source; no new or edited .qtest file.
|14. Executable permission set (`chmod +x`)|N/A|New regression assertions are C++ native tests in the existing Debug-only test source; no new or edited .qtest file.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|New regression assertions are C++ native tests in the existing Debug-only test source; no new or edited .qtest file.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|New regression assertions are C++ native tests in the existing Debug-only test source; no new or edited .qtest file.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem/network operation or Qore source I/O change.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem/network operation or Qore source I/O change.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem/network operation or Qore source I/O change.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No production filesystem/network operation or Qore source I/O change.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No new runtime loop, blocking operation or cancellation API. Native fixture loops have 16 combinations; existing locks and cancellation behavior are preserved.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No new runtime loop, blocking operation or cancellation API. Native fixture loops have 16 combinations; existing locks and cancellation behavior are preserved.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new runtime loop, blocking operation or cancellation API. Native fixture loops have 16 combinations; existing locks and cancellation behavior are preserved.
|24. No blocking operations without cancellation support|N/A|No new runtime loop, blocking operation or cancellation API. Native fixture loops have 16 combinations; existing locks and cancellation behavior are preserved.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action, type, app, factory or JNI/JAR dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Two complete internal overloads and their current-thread call site; no stub, TODO, workaround or partial feature.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Existing Program lock, error paths, entry setup and caller-first deferred teardown remain. Native tests use scoped Program/context owners and check sinks before proceeding, including pending-exception restoration.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|The explicit ID comes from the same live current-thread ThreadData already used for entry. No new retained state or cache; parse_tid atomic semantics and plock-protected counters are unchanged. Ancestor reentry and balanced counts are tested.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|The internal int ID matches q_gettid and ThreadData::tid. No cast, user-visible type, public header, native helper signature or object-layout change.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Two redundant TLS discoveries are removed without changing counts or locks. Optimized identical-module three-layout screen and independent confirmation establish the target benefit; repeated control costs and short calls are reported.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Generic callers retain q_gettid and its zero-ID shutdown fallback. The explicit path starts with existing live ThreadData/Program assertions and preserves deletion, parse-lock and initialization behavior.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal overload comments, design note, release note and results describe scope, evidence and limits. No public API or QPP method is introduced.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new filesystem/network operation, credential, format string, unchecked buffer or user-supplied identifier. The explicit ID is obtained internally from the already-live current thread.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Selected Release/Debug suites pass 8679/8879 assertions with no skips, plus two standalone fast-entry passes. Original and candidate each pass 2906 native checks. Standard 1110, common-module 8424 and rebuilt-compiler 1053 reference results pass; LLVM is identical. All 391 Release aliases are restored. 2394 timing jobs/7182 windows independently replayed; no full-suite or cross-platform claim.
