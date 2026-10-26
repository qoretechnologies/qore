# Prepared immediate local reload

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes checks cover compiler, runtime, fixtures, documentation and evidence.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new distributed module, QPP class, registration or module layout.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new distributed module, QPP class, registration or module layout.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new distributed module, QPP class, registration or module layout.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new distributed module, QPP class, registration or module layout.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new distributed module, QPP class, registration or module layout.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new distributed module, QPP class, registration or module layout.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new distributed module, QPP class, registration or module layout.
|8. No `%include` usage (deprecated for modules)|N/A|No new distributed module, QPP class, registration or module layout.
|9. Copyright 2026 on all new files|Pass|New fixture, evidence and drivers carry 2026 copyright.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new distributed module, QPP class, registration or module layout.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new distributed module, QPP class, registration or module layout.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new distributed module, QPP class, registration or module layout.
|13. `%modern` directive present|Pass|New QUnit fixture uses %modern.
|14. Executable permission set (`chmod +x`)|Pass|New QUnit fixture is executable.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib path is prepended before %requires.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only core-delivered QUnit and FsUtil are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No production filesystem or network operation is added.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production filesystem or network operation is added.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No production filesystem or network operation is added.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|Test-only TmpDir and waited backquote children create compiler artifacts and run cross-engine comparisons; no production Qore I/O is added.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|No production loop or blocking operation is added. The new native fixture loop has two iterations; existing generated cancellation points remain unchanged.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No production loop or blocking operation is added. The new native fixture loop has two iterations; existing generated cancellation points remain unchanged.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No production loop or blocking operation is added. The new native fixture loop has two iterations; existing generated cancellation points remain unchanged.
|24. No blocking operations without cancellation support|N/A|No production loop or blocking operation is added. The new native fixture loop has two iterations; existing generated cancellation points remain unchanged.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action/schema/app/factory or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete versioned helper, compiler admission, full generic fallback and diagnostic controls; no TODO or stub.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Strong-list fast read acquires exactly one owned node under the ordinary ref rules. Null/other-type paths retain existing eval and loaded-reference ownership. No pending-sink early exit is added. Cache publication and old-deferred release keep their original order. Native allocations use holders.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Prepared storage belongs to the current invocation and excludes captured locals, parameters, self and top-level locals. Existing getter checks active Program and live body storage. No shared mutable value cache, lock elision or Program-entry change.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|static_cast to QoreListNode follows assigned QV_Node, nonnull node and actual NT_LIST checks. Weak/reference and all other representations retain evaluation. Private helper is versioned and registered; public calling convention unchanged.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Local lookup is removed for admitted body storage; actual strong-list storage also omits two evaluator calls while retaining ownership. Frozen optimized before/after runs cover three layouts, 67 screening workloads, confirmation controls and short invocations; repeated costs are disclosed.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Compiler-owned context/index and prepared slot follow existing admission. Null preparation keeps ordinary lookup. Reference/weak values, reflective replacement, block lifetime, pending exceptions and caller cleanup/cancellation retain semantics. Reading a stored invalid list retains ordinary load behavior.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Private helper documents index, borrowed slot, owned result and fallback semantics. Design, measurements and release notes explain scope and limits; no new public API example is required.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, user-controlled format string or credential. Context indices come from compiler AOTSlotMap; prepared storage admission retains identity/lifetime constraints and actual list type is checked before cast.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release 52 cases/6804 assertions with one documented Debug-only DGC skip; Debug 43 cases/8252 assertions, plus 2220 native checks. Standard 1110 and actual-module 7704 numerical references pass. All 391 Release aliases/hashes restored. Raw timing jobs/windows independently replayed. No full-suite or sanitizer claim.
