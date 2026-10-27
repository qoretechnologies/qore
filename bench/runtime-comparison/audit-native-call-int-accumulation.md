# Native callback integer results in compound addition

Copyright 2026 Qore Technologies, s.r.o.

All 62 checks from audit-changes were reviewed for the two compiler files, two new regression fixtures, measurements, results, design state and release note: 18 Pass, 44 N/A, no Fail. This is the final review; the raw-data retention decision intentionally preserves its earlier pre-audit status.

The change admits only an earlier, exact-int typed callback whose result is consumed once as the RHS of AddAssignInt in the same IR block. Complete use counting includes dedicated Return/Phi uses. The native scalar side channel removes temporary integer boxing; fallback conversion, owned cleanup and live runtime admission checks remain. Ordinary addition, multiple uses and cross-block arithmetic stay on their original paths. The private compiler method signature changes; runtime helpers and object/frame ABI do not.

Optimized five-fork confirmation: wide closure accumulation 28.619360 to 13.570241 ns/item (52.584% less time), function references 21.462216 to 8.076496 (62.369% less). Small inline results have no consistent all-layout gain. Effectful controls cost 1.985–2.759%; broader confirmation includes polymorphic floating expressions +4.100%, object-closure creation +3.797% and mixed workloads from -0.653% to +1.449%. Most larger costs persist with the optimization disabled, but their cause remains unproven. Retention follows the user's broader-tradeoff instruction, with all costs preserved.

Independent publication replay verifies raw process completion, output hashes, samples, job order, layout/aggregate medians, 7367 dependency records, 13 dependency manifests, source hashes and all 12 phases. Published evidence SHA256: `785016e1a4f89090e3422d6c66a59dead5bfd5fbfe0457a6c1685599aa2d486a`. Reviewer SHA256: `5fc9f6078b53c1645dec6d45802920d66df2df1c8cd839e0091024033062cf26`.

[Full measurements and limits](RESULTS.md#native-integer-callback-results-in-compound-addition-2026-10-27); [raw evidence](results-native-call-int-accumulation-macos-arm64.json). Shared-host whole-kernel ns/item; no new Java/develop ratio, isolated physical call latency, global performance ceiling, complete-project suite, sanitizer/Linux or external binary-module claim.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped Qore module or QPP class.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped Qore module or QPP class.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new shipped Qore module or QPP class.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new shipped Qore module or QPP class.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new shipped Qore module or QPP class.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new shipped Qore module or QPP class.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new shipped Qore module or QPP class.
|8. No `%include` usage (deprecated for modules)|N/A|No new shipped Qore module or QPP class.
|9. Copyright 2026 on all new files|Pass|New executable fixtures, raw measurement data and this audit carry Copyright 2026; existing source notices retained.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new shipped Qore module or QPP class.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new shipped Qore module or QPP class.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new shipped Qore module or QPP class.
|13. `%modern` directive present|Pass|Both .qr/.qtest use %modern.
|14. Executable permission set (`chmod +x`)|Pass|Both new regression fixtures have executable mode 100755.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The QUnit/FsUtil harness prepends repository qlib before requires; the standalone fixture has no module requirements.
|16. External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|QUnit and FsUtil ship with Qore; no external dependency.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new production filesystem/network/blocking operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new production filesystem/network/blocking operation.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new production filesystem/network/blocking operation.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|TmpDir and quoted subprocess commands are test infrastructure; no new production File/Dir/Socket/HTTP operation.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|The existing block/instruction traversal checks cooperative cancellation every100 iterations; no new unbounded runtime loop.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|The touched traversal retains qore_check_cancel; no deprecated interrupt API.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Constant-time facts/type/use-count lookups retain the tight-loop interval of100.
|24. No blocking operations without cancellation support|N/A|No new production filesystem/network/blocking operation.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider or JNI/JAR change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI/JAR change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider or JNI/JAR change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider or JNI/JAR change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider or JNI/JAR change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider or JNI/JAR change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider or JNI/JAR change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider or JNI/JAR change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider or JNI/JAR change.
|34. Response/output types use `private` Fields|N/A|No DataProvider or JNI/JAR change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider or JNI/JAR change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI/JAR change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI/JAR change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider or JNI/JAR change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider or JNI/JAR change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider or JNI/JAR change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI/JAR change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI/JAR change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI/JAR change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider or JNI/JAR change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI/JAR change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider or JNI/JAR change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI/JAR change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI/JAR change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI/JAR change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider or JNI/JAR change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI/JAR change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI/JAR change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete same-block single-use int AddAssignInt consumer implemented; no TODO/FIXME/stub introduced.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Native scalar result has no owned node; boxed fallback and cleanup remain unchanged. LLVM owns new blocks/PHIs; no manual Qore allocation added.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Only per-lowering-instance sets/maps change; no new shared mutable runtime state.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Exact bigIntTypeInfo proof, checked operand counts and complete use count. Regression callbacks have code<return(args)> signatures; auto return cases intentionally test supported fallback.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Additional set operations are in an existing linear compiler pass. Confirmed wide-result gains of 52.584% and 62.369% repeat in all layouts. Repeated broader and effectful costs are disclosed and accepted under the authorized workload tradeoff; no weighted overall speedup is claimed.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Unsupported signatures, multiple uses, different blocks and ordinary AddInt retain current lowering. Typed admission misses retain ordinary conversion/ownership.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Private compiler comments, design state, release notes and full phase tables describe scope, independent controls, preserved fallback/ownership, gains and accepted costs. No new public Qore method. Strict Qore table validation is required before commit.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags change.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, credentials, user-controlled formatting or raw buffer operations. Context/lifetime/stack guards are preserved.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release and Debug each pass 36 fixtures (33 QUnit runs, 202 cases, 10080 assertions and 3 standalones); Debug adds 70 control assertions and 2917 native checks. All 391 Release aliases restored. New regression covers boundaries/wrap, generic fallback, owned cleanup, exception ordering, excluded consumers, O0/O3, EH/split and disabled/legacy paths. Independent publication replay verifies 4575 paired jobs/13725 windows and 72 initial jobs/216 windows, all raw receipts and 13 reconstructed dependency manifests.
