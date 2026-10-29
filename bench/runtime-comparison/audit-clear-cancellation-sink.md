# Clear cancellation sink audit

Copyright 2026 Qore Technologies, s.r.o.

Applied `/Users/david/.codex/skills/audit-changes/SKILL.md` to the three compiler files, three fixtures and documentation/evidence. Every check is assessed below. [Full evidence](results-clear-cancellation-sink-macos-arm64.json).

|!Number|!Check|!Status|!Evidence
|1|Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|2|Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|3|`qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|4|Module added to QMOD list in `CMakeLists.txt`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|5|`.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|6|`%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|7|No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|8|No `%include` usage (deprecated for modules)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|9|Copyright 2026 on all new files|Pass|All three new fixture sources and evidence/audit carry Copyright 2026; both Qore fixtures are executable.
|10|Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|11|No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|12|`ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|13|`%modern` directive present|Pass|Both Qore fixture sources use %modern.
|14|Executable permission set (`chmod +x`)|Pass|New .qtest and .qr have executable permissions.
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit/FsUtil module path is prepended before %requires.
|16|External module dependencies use `%try-module` — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|Only shipped QUnit and FsUtil are required; no external Qore dependency.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Compiler CFG transformation adds no production filesystem operation.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|Compiler CFG transformation adds no network operation.
|19|If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new runtime filesystem/network operation.
|20|No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|File::readTextFile reads CMakeCache in the unsandboxed native-compiler test setup; TmpDir owns generated test files.
|21|All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|Function, instruction, edge and user traversals check cancellation every100 steps; simplification every10 functions. Recognizer loops bounded by8; native test walks check cancellation. No PHI input traversal/copy remains.
|22|Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|Pass|Only qore_check_cancel is used; existing emitted atomic polling/helper semantics remain.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|100 tight analysis operations,10 expensive simplifications; no reduced runtime polling frequency.
|24|No blocking operations without cancellation support|Pass|No blocking production operations; test backquotes are synchronous and the outer qualification driver times out and reaps only owned process groups.
|25|Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|26|Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|27|Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|28|DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|29|DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|30|Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|31|Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|32|**Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|33|Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|34|Response/output types use `private` Fields|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|35|Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|36|Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|37|Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|38|Password/secret fields have `"sensitive": True`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|39|`groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|40|App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|41|App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|42|`display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|43|`short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|44|`desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|46|No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|47|No bare field/option names in prose — must use backticks|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|49|**Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|50|**`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|51|**Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No new module, QPP class, DataProvider registration or JNI integration.
|53|**No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete narrowly admitted transform with fallback by leaving CFG unchanged, a disable control, meaningful native proof tests and generated-code comparison; no TODO/stub.
|54|**Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|LLVM owns generated instructions; RAII containers own analysis state. Unknown calls/writes invalidate facts. Unproved entry paths and every slow-helper path retain full sink predicates. Compiler cancellation fails object emission.
|55|**Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|All pass state is invocation-local; existing thread/program seq_cst polls retain ordering. Valid ExceptionSink state is changed by its executing thread; diagnostic fields belong to distinct ThreadData storage.
|56|**Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|LLVM dyn_cast plus exact pointer/i8 predicate shape and constant offset checks; single function-argument sink. Strongly typed Qore fixture methods. No representation/memory attributes added.
|57|**Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Linear dirty propagation, bounded shape checks, no PHI expansion. All32screen,9predeclared confirmation and5adaptive adverse controls retained. Floating-loop accumulation improves 1.69% in the 32-workload screen (5.1333 to 5.0467 ns/item) and 2.29% in independent confirmation (5.1720 to 5.0537). The same-runtime disabled/enabled comparison improves 1.55% (5.1333 to 5.0537). This is a narrow kernel benefit; direct/simple and indirect calls gain no established additional speed.
|58|**Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Rejects missing provenance, multiple sink roots, stale checks, volatile reads, escaping values, unknown calls/writes and duplicate/unsafe edges. LLVM verifier failure prevents object emission; child exit statuses checked.
|59|**Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Internal API documents parameters, return/failure and caveat. Cooperative-cancellation design, current-state design, all results and62audit checks updated. Strict qpp table validation passes all four Markdown files.
|60|**QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method or flags changed.
|61|**Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new buffers, I/O protocols, credentials or user-controlled format strings; constant-size shape checks, LLVM typed queries and local containers.
|62|**Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/Debug each32fixtures:56cases10371assertions plus3silent; new fixture includes37native proof cases. Debug3237focused/4899broader assertions overlap.391Release aliases restored.15180final artifact reference outputs;15candidate LLVM modules and45artifact/runtime combinations verified.954final measured processes2862windows independently replayed;1008broad prototype processes3024windows preserved.
