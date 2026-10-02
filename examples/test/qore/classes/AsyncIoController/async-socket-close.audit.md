# Async socket close and completion ownership audit

Copyright 2026 Qore Technologies, s.r.o.

Scope: the seven-file native fix based on ef781b791. The RSS fix in that commit belongs to another session and is preserved. Regression additions live in lib/ql_debug.cpp.

Validation: debug and optimized builds pass. Runtime and descriptor checks pass. The user explicitly approved the standalone-reproduced GCC/Valgrind debug-information diagnostic on 2026-10-02; evidence is in qore-packaging/evidence/core-close-rpm-qualification-20261002.json.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — must be all lowercase (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|9. Copyright 2026 on all new files|Pass|No new source files; changed C++ headers/sources and this audit carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new Qore module or QPP class; module layout, registration, and namespace rules do not apply.
|13. `%modern` directive present|N/A|Regression additions are C++ tests invoked by the existing executable %modern QUnit wrapper; no Qore test source changed.
|14. Executable permission set (`chmod +x`)|N/A|Regression additions are C++ tests invoked by the existing executable %modern QUnit wrapper; no Qore test source changed.
|15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|N/A|Regression additions are C++ tests invoked by the existing executable %modern QUnit wrapper; no Qore test source changed.
|16. External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|Regression additions are C++ tests invoked by the existing executable %modern QUnit wrapper; no Qore test source changed.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No new filesystem access. Event-loop/notifier cleanup releases descriptors through existing ownership APIs.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|New network activity is confined to native regression fixtures binding loopback port zero; production changes only close and wake existing sockets.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|Pass|Socket binding in the regression uses the existing QoreSocketObject API; no new production sandbox entry point or bypass.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No Qore-language implementation changes.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Wake traversal is mandatory close completion over live controller contexts, not interruptible user work. Resize visits the configured finite context set; test loops have at most five iterations.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No deprecated cancellation API introduced. Existing controller cancellation, completion delivery, and generation validation remain active.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No new tight computation or data-processing loop. Close completion cannot stop halfway through waking affected contexts.
|24. No blocking operations without cancellation support|Pass|Notification writes are nonblocking. Regression synchronization uses processing barriers and bounded queue/event waits, with unconditional stopClear cleanup.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider, action registration, application, or data-type changes.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider, action registration, application, or data-type changes.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider, action registration, application, or data-type changes.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider, action registration, application, or data-type changes.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider, action registration, application, or data-type changes.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider, action registration, application, or data-type changes.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider, action registration, application, or data-type changes.
|32. Typed data type classes exist for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider, action registration, application, or data-type changes.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider, action registration, application, or data-type changes.
|34. Response/output types use `private` Fields|N/A|No DataProvider, action registration, application, or data-type changes.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider, action registration, application, or data-type changes.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, action registration, application, or data-type changes.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, action registration, application, or data-type changes.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider, action registration, application, or data-type changes.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider, action registration, application, or data-type changes.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider, action registration, application, or data-type changes.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, action registration, application, or data-type changes.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, action registration, application, or data-type changes.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, action registration, application, or data-type changes.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `bold` for caveats|N/A|No DataProvider, action registration, application, or data-type changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, action registration, application, or data-type changes.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider, action registration, application, or data-type changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, action registration, application, or data-type changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, action registration, application, or data-type changes.
|49. Factory registration in Qore repo: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, action registration, application, or data-type changes.
|50. `getRecordTypeImpl()` signature: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider, action registration, application, or data-type changes.
|51. Dependency JARs committed (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No JNI or JAR packaging changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No JNI or JAR packaging changes.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Root causes fixed directly: missing close wakeup, unowned context resources, and descriptor-owner lifetime. No sleeps, polling workaround, warning suppression, or relaxed assertions.
|54. Exception safety: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|ReferenceHolder protects Qore allocations and retrieved notifier objects. unique_ptr owns replacement contexts; swap occurs only after all fallible setup succeeds. Test cleanup always stops controllers.
|55. Thread safety: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Controller m protects context lifetime and queue publication; only the actual owning Qore thread mutates its wake set. HTTP action object references are immutable until action cleanup; existing serialized action execution is preserved.
|56. Type safety: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Typed native APIs, static_cast for action polymorphism, typed submission hashdecl, and explicit error-state assertions.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Close wakeup is O(live I/O contexts); it reuses per-context socket indexes rather than scanning all operations. No periodic polling added.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Notifier retrieval failures release pending connection reservations; resize retains prior state on setup failure. Running resize is rejected. Existing notifier implementation handles nonblocking writes.
|59. Documentation: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Implemented ownership and close ordering documented in design/async-socket-io.md with a late-accept example; release notes updated; new internal constructor parameters documented.
|60. QPP flags: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or public QPP flag changes.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Loopback ephemeral fixtures only; no credentials, external endpoints, uncontrolled format strings, or new unchecked indexes.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|866 native assertions; 12 functional suites; 900 optimized shutdown cycles at 1/2/4 I/O threads; AsyncSocketIo at 2/4 threads; seven Valgrind suites with zero memory/descriptor errors or lost allocations. Compiler metadata diagnostic is tracked separately.

Checklist: 17 Pass, 45 N/A, 0 code-review failures. The only diagnostic exception is the explicitly approved compiler metadata warning.
