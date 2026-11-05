# Finite callback adapter binding audit

Copyright 2026 Qore Technologies, s.r.o.

All 62 audit-changes skill checks cover this incremental compiler, test and evidence/documentation change.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore library module.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore library module.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore library module.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore library module.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore library module.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore library module.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore library module.
|8. No `%include` usage (deprecated for modules)|N/A|No new Qore library module.
|9. Copyright 2026 on all new files|Pass|New audit and evidence carry Copyright 2026; existing test copyright remains.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore library module.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore library module.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new QPP class.
|13. `%modern` directive present|Pass|The modified AOTFiniteCallbackTargets.qtest uses %modern.
|14. Executable permission set (`chmod +x`)|Pass|The modified test remains executable.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The test prepends the repository module path before bundled QUnit/FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only bundled modules are required; no external test dependency is added.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operation is added to compiler lowering.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operation is added to compiler lowering.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No C++ filesystem/network operation requiring a helper.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|N/A|No new Qore I/O; existing test temporary-directory/compiler execution is unchanged.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|The new compiler group scan checks cancellation every100 groups; its inner binding loop is bounded to8, and the new PHI-edge loop to2. Existing source-loop cancellation remains.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|No cancellation check is removed and no deprecated interruption API is introduced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|The new group scan checks every100 iterations; bounded inner loops need no additional check. Existing source-loop backedge checks remain.
|24. No blocking operations without cancellation support|Pass|No blocking runtime operation is added.
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
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete late removal of unused callback adapters after every admitted call has actually inlined; no TODO, stub or partial path.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|New compiler maps/vectors have automatic ownership; recorded LLVM call pointers are non-owning and cleared per function. Only void binding calls are erased after complete body lowering. Original creation, target resolution, boxed PHI/store, ownership and cleanup remain; O0/O3, EH and fallback behavior are tested.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|New metadata belongs to one compiler/function and is reset at both existing reset points; no shared runtime state is added. The finite-target no-reentry proof excludes reflective replacement, alias/capture/ref/weak/opaque locals and arbitrary callees. AOT debug hooks remain no-ops; JIT is unchanged.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing exact target/binding/dominance/lifetime proof is retained. Each recorded creation has only its PHI use, counted in both the operand and incoming-edge lists; emitted LLVM binder calls are strongly typed. Native scalar lowering is unchanged.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|1758 optimized jobs/5274 windows are independently replayed; three layouts, independent confirmation, compile-disabled attribution, broader controls and fresh Java are reported. New maps and counters add linear compiler work with bounded per-group cleanup; there is no new runtime lookup.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|The existing finite-target admission is unchanged. A group retains its generated adapters unless every expected call was actually inlined; additional creation uses decline deletion. A compilation-only disabling flag restores all binders.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Comments, design, release note, README and results describe proof, disabling flag, ownership and measurement scope. No public API changes.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new format string, unchecked index, credential, runtime ABI or external access. Existing test commands quote paths and wait for children.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each pass 38 cases/5196 assertions and one standalone script; 2917 native assertions and 18 cases/70 fallback controls. Expanded fixture has470 assertions across source/AOT configurations. Six pilot images pass360 references; focused checks cover720 source and1620 native references with9 LLVM verifications; broader images pass5130 native and795 source references; fresh Java passes180 references and12 C2 diagnostics. Attribution adds1845 references and6 LLVM verifications; no runtime identity bypass.
