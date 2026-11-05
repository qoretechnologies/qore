# Shared-context develop integration audit (2026-10-30)

Copyright 2026 Qore Technologies, s.r.o.

All 62 checks completed: {"N/A": 44, "Pass": 18}.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new module or QPP class; existing registration and layout unchanged.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module or QPP class; existing registration and layout unchanged.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module or QPP class; existing registration and layout unchanged.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new module or QPP class; existing registration and layout unchanged.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module or QPP class; existing registration and layout unchanged.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|Pass|Existing QLS and VscDebugAdapter retain %modern; no redundant directives added.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|Pass|QLS/Files.qc has no separated-source parse directives.
|8. No `%include` usage (deprecated for modules)|Pass|No %include added in either existing module.
|9. Copyright 2026 on all new files|Pass|New evidence, drivers and audit carry Copyright 2026; existing tests retain 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new module or QPP class; existing registration and layout unchanged.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new module or QPP class; existing registration and layout unchanged.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new module or QPP class; existing registration and layout unchanged.
|13. `%modern` directive present|Pass|Both changed test files use %modern.
|14. Executable permission set (`chmod +x`)|Pass|git ls-files -s confirms mode 100755 for both changed tests.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Both tests prepend the in-tree qlib directory before module requirements; Release qmods rebuilt first.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|The external process >=2.2 dependency uses %try-module; bundled QUnit/QLS/FsUtil/json use %requires. Final runs have no skips.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++ change in this merge.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No C++ change in this merge.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ change in this merge.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|QLS filesystem reads implement source discovery through checked Qore APIs; regular-file admission avoids FIFO blocking and prefix reads are capped at 256 bytes. Test files and FIFOs are owned by TmpDir. No new native I/O bypass.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No C++ change in this merge.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No C++ change in this merge.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++ change in this merge.
|24. No blocking operations without cancellation support|N/A|No C++ change in this merge.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider registration, data type, application, factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete upstream fixes plus an observed output-order test correction; no new workaround/stub/TODO. Diagnostic v1 excluded every case and is explicitly rejected as evidence; v2 ran all cases.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|ReadOnlyFile and TmpDir retain automatic cleanup. Every started adapter has on_exit termination and normal wait. Pause-state changes use on_error cleanup; output-forwarding errors are caught and reported.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Debug requests and event handlers serialize through existing m_lock; pause/breakpoint state is modified under that protocol. QLS directory candidates are invocation-local; interpreter size is immutable.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|New directory results use list<string>, uniqueness uses hash<string,bool>, and DAP predicates are code<bool(hash<auto>)>. Heterogeneous DAP message hashes preserve the existing protocol representation.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|QLS file probing is bounded to 256 bytes instead of reading an entire line byte by byte. Candidate directories are deduplicated in one pass; bounded breakpoint/stack scans remain. No C++ runtime change or new performance ratio is attributed to this merge.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Coverage includes unreadable/nonexistent/empty/binary files, FIFOs, directories, symlinks, CRLF, long interpreter lines and duplicated module directories; debugger tests cover rejected breakpoints and filtered exceptions.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|New QLS source-discovery API documents inputs, ordering, return value and installed/in-tree behavior. Debug adapter module and release notes document stop reasons, stack names, background threads and breakpoint errors. The output-wait helper is test-only.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|New printf/sprintf formats are constants; stack and prefix accesses retain explicit bounds. No credentials or raw native resource access added.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Optimized full build and 391 Release aliases verified. Eight fixtures pass: 30 QUnit cases/2118 assertions plus three exact/silent scripts. The complete 11-case/176-assertion debug-adapter suite passes three additional fresh processes. Failure and diagnostic outputs retained. Prior Release/Debug C++ qualification remains applicable to unchanged C++ sources; this is not a new Debug or timing campaign.
