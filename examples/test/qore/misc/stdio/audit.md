# Standard descriptor startup audit

Copyright (C) 2026 Qore Technologies, s.r.o.

Reviewed on 2026-09-25 using every checklist item in the audit-changes skill, against
base commit ef5a2a70bf07d752ae7def10e8c9b731ab21a69d on develop.

Scope: C++ startup code/private header, public initialization documentation, CMake
native-test registration, Qore/native tests, and release/test documentation. No
Qore production module or DataProvider changes are included.

The runtime and executable entry points reserve missing descriptors before other
file operations. POSIX checks use F_GETFD; repairs use read/write null descriptors,
checked duplication, and temporary descriptor cleanup. Windows handles detached
CRT streams, reopens NUL in binary mode, and updates the repaired Win32 standard
handle. Existing descriptors and flags are preserved.

Sandbox/cancellation applicability was checked against
`design/module-sandboxing-audit-guide.md` and `design/cooperative-cancellation.md`.
This is fixed-resource process bootstrap before a QoreProgram or thread state
exists, not an operation performed on behalf of sandboxed code. Calling those
program-context APIs here would be invalid. Initialization must be serialized
against concurrent descriptor changes; an already reused descriptor cannot be
identified or repaired. These boundaries are documented at the implementation
and public API.

Validation completed before committing:

- `cmake --build build --target qore qcc qore-stdio-startup-test -j4`: passes.
- `QORE_BINARY="$PWD/build/qore" ./run_tests.sh -d qore/misc/stdio`: 4 cases,
  73 assertions, no failures or skips; includes all 18 native scenarios and V8.
- Repeating with only repository module directories and QORE_MODULE_DIR_ONLY=1:
  67 assertions pass, with the optional V8 case skipped as intended.
- Related backquote, File, and file-stream suites: 13 cases, 73 assertions pass.
- `build/qpp --table-strict --table=doxygen/lang/900_release_notes.dox.tmpl
  --output=/tmp/qore-stdio-release-notes.dox`: passes.
- `git diff --check`: passes.
- Before rebuilding, the interpreter suite reproduces the stdin/data-file failure
  and V8 libuv assertion; the native embedding test fails because fd 0 remains closed.

Windows execution is unavailable locally; its branch was reviewed against the CRT
and Win32 APIs. Linux/macOS CI is tracked separately after pushing the audited commit.

Changed files:

- `CMakeLists.txt`
- `command-line.cpp`
- `doxygen/lang/900_release_notes.dox.tmpl`
- `examples/test/qore/misc/stdio/README.md`
- `examples/test/qore/misc/stdio/audit.md`
- `examples/test/qore/misc/stdio/stdio-child.q`
- `examples/test/qore/misc/stdio/stdio-native.cpp`
- `examples/test/qore/misc/stdio/stdio-startup.qtest`
- `examples/test/qore/misc/stdio/stdio-v8-child.q`
- `include/qore/Qore.h`
- `include/qore/intern/qore_stdio.h`
- `lib/qore-main.cpp`
- `qcc-main.cpp`

Audit summary (the detailed table below covers all 62 checks):

|!Check|!Status|!Findings
|Module structure and registration|N/A|No production module changes; native POSIX test target is registered in CMake and built by default
|Release notes and public documentation|Pass|Startup behavior and embedding limitations documented
|Test conventions|Pass|Modern executable QUnit test, local module path, optional V8 dependency
|Sandboxing and cooperative cancellation|N/A|Documented process-bootstrap scope; no QoreProgram or cancellation state exists yet
|DataProvider checks and JARs|N/A|No affected files
|Workarounds, exception/thread/type safety, performance|Pass|Bounded shared helper, checked descriptor ownership, serialized startup contract
|Error handling, security, correctness|Pass|Regression, embedding, exec, AOT, V8, and injected-failure coverage
|QPP flags|N/A|No QPP methods changed

Detailed checklist:

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Qore 3.0 release notes document null-device repair, preserved redirects, and fatal startup failure.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|8. No `%include` usage (deprecated for modules)|Pass|No deprecated %include directives introduced.
|9. Copyright 2026 on all new files|Pass|All new code, test fixtures, README, and audit files carry copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No new Qore module or QPP class; no module layout, namespace, or registration changes.
|13. `%modern` directive present|Pass|stdio-startup.qtest and both Qore child fixtures use %modern.
|14. Executable permission set (`chmod +x`)|Pass|stdio-startup.qtest has executable mode 0755.
|15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|The source qlib path is prepended before requiring QUnit and FsUtil.
|16. External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|Pass|The suite probes v8 with %try-module and skips its case when unavailable. The child requiring v8 only runs behind that gate.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Fixed null-device process bootstrap runs before QoreProgram, sandbox manager, and Qore thread state exist; it cannot call program-context APIs. This scope is documented in qore_stdio.h and Qore.h.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No production network operations introduced.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|Fixed null-device process bootstrap runs before QoreProgram, sandbox manager, and Qore thread state exist; it cannot call program-context APIs. This scope is documented in qore_stdio.h and Qore.h.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|File operations belong to regression fixtures and isolated TmpDir data/input/output files. There is no new Qore production I/O.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|N/A|Production iteration is over three descriptors plus syscall EINTR retries before cancellation infrastructure exists. Native test subprocesses have 30-second alarms.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No cancellation API is usable at this startup point; no deprecated qore_check_io_interrupt calls introduced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No runtime processing loop added. The only retry loops are startup syscalls and test waitpid.
|24. No blocking operations without cancellation support|N/A|Null-device startup I/O precedes cancellation state. Qore test I/O uses existing cancellable APIs; native children are bounded by alarms.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|34. Response/output types use `private` Fields|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider action, app, data type, factory, or dependency JAR changes.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete startup repair, native/platform branches, documentation, and regressions; no TODOs, FIXMEs, or feature stubs. The UCRT handler intentionally returns to permit probing closed descriptors.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|The helper allocates no Qore or C++ heap objects. Successful temporary descriptors are closed; fatal paths use _exit so the OS releases descriptors without partial-runtime teardown.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|No production mutable globals added. Startup must be serialized against descriptor mutation, documented for embedders. UCRT handler changes are thread-local and restored; test injection state is private to each forked child.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Explicit integral casts, HANDLE reinterpretation, bounded descriptor indices, and typed test scenario enums; no untyped callbacks or result hashes added.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Three descriptor queries per helper invocation; no allocation or null-device open on the normal path. CLI plus library checks remain constant cost.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Only EBADF triggers POSIX repair. fcntl/open/dup2 retry EINTR; all other failures stop startup. close is never retried after uncertain EINTR. Tests cover fatal failures and nonzero successful dup2 returns. Diagnostics are best effort.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Qore.h documents the public startup contract and embedding boundary; release notes and README describe behavior and reproducible test commands. No new public methods need parameter/return/throw documentation.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP methods or flags changed.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Only literal null-device paths and fixed diagnostic formats in production; descriptor indices and message lengths are bounded. Test shell arguments are quoted; temporary files are isolated; no credentials added.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|POSIX behavior checked against syscall contracts, Node startup, and Go descriptor probing. Regressions fail on the old build and pass after repair: all masks, preserved flags, exec inheritance, native embedding, AOT, V8, EINTR, allocation/cleanup, and fatal failures. Windows is reviewed but not executed locally.
