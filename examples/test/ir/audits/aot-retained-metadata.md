# AOT retained metadata: issue 5471

Copyright (C) 2026 Qore Technologies, s.r.o.

Date: 2026-09-25. Baseline: `5486431438da9fb4c134e62695e212c0515d83e3` on `develop`.
Issue: https://github.com/qoretechnologies/qore/issues/5471.

The runtime now borrows immutable metadata when both ends of the blob and its native function
descriptor table resolve to the same loaded binary image. The descriptors already have to outlive
registered code. Executables remain mapped, and `QoreBuiltinModule` deliberately does not call
`dlclose`, including during reinjection and shutdown. Linux and macOS use `dladdr` for this check;
other platforms conservatively retain the existing copy. No compiler change, artifact format change,
or recompilation of existing AOT programs is required.

Borrowed compressed input remains compressed until the retained reader is first needed. Its decoded
string pool and sections still have owned storage. The existing reader mutex and decoding of
`SLOT_MAPS`, `DEBUG_IR`, and `PLUGIN_IMPORTS` before publication are preserved. Heap inputs and inputs
whose image cannot be established retain the existing copying/compaction behavior. The trace's
`retained` field continues to count vector payload bytes; the appended `borrowed` field counts the
referenced image bytes, not allocated memory.

## Caller lifetime audit

|!Caller|!Input provenance and decision
|`qore_aot_run_v2`|Generated executable blob and descriptors: borrow if the image check succeeds; otherwise copy
|`qore_aot_run_v3`|Generated executable blob and descriptors: same check
|`qore_aot_module_init_v2`|Mapped qmod or statically linked module: same check; loader keeps successful modules mapped
|`qore_aot_module_init_v3`|Mapped qmod or statically linked module: same check
|`runAOTModuleInitForProgram`|Deferred initializer fallback owns a shared vector snapshot: copy remains the default
|`qore_aot_script_register_native_impl`|Linked per-file native metadata: same image check, also reached from batch processing
|`qore_aot_script_register`|Public host API may receive a transient buffer: same image check rejects heap input
|`qore_aot_script_end_batch`|Deferred host metadata remains valid through the batch; same image check before retaining it
|`installSourceParseIRFallbacks` (direct constructor)|Deserializer input may be temporary `.qo`/`.qoa` or compiler storage: default copy is unchanged

## Measurement

Six fresh processes per variant and application (24 runs), alternating before/after order on each
trial. The same existing September 19 Qorus binaries, module artifacts, environment, and arguments
were used for both variants. Only `LD_LIBRARY_PATH` selected a saved baseline or fixed release
`libqore.so.20`. `/proc/PID/maps` was checked at every checkpoint to verify the intended library.
The Qore build uses GCC 15.2.0, release `-O3`, LLVM 21.1.8, on x86-64 Linux/glibc.

An `LD_PRELOAD` wrapper calls the real `qore_aot_script_end_batch`, requires success, records
`mallinfo2` and `getrusage`, and raises `SIGSTOP` immediately after registration, before the host
runs its main Qorus class. The parent reads `/proc/PID/smaps_rollup` and resumes the child. The
child then calls `malloc_trim(0)` and stops again for a second snapshot, before exiting. Thus these
are live retained-memory measurements, not just peak RSS. Heap usage is `uordblks + hblkhd`, including
large allocations that glibc serves with anonymous mmap. No application request or service startup
is included; module initializers are part of registration.

The installed `qorus-core` has a file capability, which disables `LD_PRELOAD`. Measurements therefore
use the capability-free build binary under `/home/david/src/Qorus/git/qorus/build`, not the installed
copy. Its older native code expects pre-September-24 ProviderIndex APIs. For this benchmark only,
`ProviderIndex` and `ProviderIndexUtil` from `1060531f3^` were compiled at `-O0` into
`/tmp/qore-5471/qorus-env/qlib`, selected via `OMQ_DIR`; remaining modules come from the installed
Qorus qlib. Both variants use exactly these artifacts. No installed module or Qorus source was changed.
This local workload has 1,063 retained objects in core and 61 in qctl, so its byte totals differ from
the issue's 1,092/63-object run.

All figures below are medians. MiB means 1,048,576 bytes; procfs kB values are treated as KiB.

|!Metric|!qorus-core before|!qorus-core after|!Reduction
|Retained vector payload|212,889,618 bytes|0 bytes|203.027 MiB
|Allocated heap, including mmap|1,016,889,304 bytes|803,582,752 bytes|203.425 MiB (20.98%)
|Anonymous resident memory|988.305 MiB|784.900 MiB|203.404 MiB (20.58%)
|Private dirty memory|1,005.027 MiB|801.656 MiB|203.371 MiB
|Anonymous after allocator trim|983.578 MiB|780.188 MiB|203.391 MiB
|RSS|1,457.064 MiB|1,131.412 MiB|325.652 MiB
|Minor faults through registration|267,065|213,336.5|53,728.5 (20.12%)

|!Metric|!qctl before|!qctl after|!Reduction
|Retained vector payload|9,935,553 bytes|0 bytes|9.475 MiB
|Allocated heap, including mmap|124,417,552 bytes|114,461,336 bytes|9.495 MiB (8.00%)
|Anonymous resident memory|125.561 MiB|116.066 MiB|9.494 MiB (7.56%)
|Private dirty memory|141.092 MiB|131.480 MiB|9.611 MiB
|Anonymous after allocator trim|124.953 MiB|115.469 MiB|9.484 MiB
|RSS|234.115 MiB|221.068 MiB|13.047 MiB
|Minor faults through registration|34,495.5|32,019|2,476.5 (7.18%)

Core anonymous-memory ranges were 1,012,020–1,012,040 KiB before and 803,732–803,744 KiB after;
qctl ranges were 128,568–128,580 and 118,848–118,852 KiB. The heap reduction closely matches the
eliminated payload plus allocation/reader overhead. The extra RSS reduction includes fewer clean
file-backed pages touched by copying; it must not be advertised as additional per-process private
heap savings. The small elapsed-time differences are not used to claim a startup speedup.

Core borrows 303,650,274 blob bytes across 932 uncompressed, 74 zstd, and 57 sectioned-zstd inputs.
Qctl borrows 11,367,656 bytes across 25 uncompressed, 17 zstd, and 19 sectioned-zstd inputs. Each run
has identical object counts and retained-byte totals within its variant. No retained vector payload
is reported for any mapped input after the change; decoded compressed storage is included in the
allocator and procfs measurements.

Local evidence and reproduction aids are in `/tmp/qore-5471`: `measure.py`, `checkpoint.c`,
`checkpoint.so`, `measurements.json`, per-process traces, both saved libraries, and test logs.
The measurement command is `python3 /tmp/qore-5471/measure.py 6`; that script verifies library
selection and registration success and alternates the variants. Its process environment is:

```sh
LD_LIBRARY_PATH=/tmp/qore-5471/before # or /tmp/qore-5471/after
LD_PRELOAD=/tmp/qore-5471/checkpoint.so
OMQ_DIR=/tmp/qore-5471/qorus-env
QORE_MODULE_DIR=/home/david/src/Qorus/test/qlib
QORE_AOT_TRACE_RETAINED_METADATA=1
# Execute /home/david/src/Qorus/git/qorus/build/{qorus-core,qctl} --help
```

Library SHA-256:

- Before: `4104c89d1555ccaadd0d002b28614cf85363d0b77d12d3844cff2545084676f0`
- After: `c7274ac8f75a3cbcb85116499e58c38fa5bbe5aaa9bd4bad886cafad34835aea`

## Validation

`cmake --build build --target qore qcc -j4` passed without compiler warnings.
`AOTRetainedMetadata.qtest` fails on the baseline's nonzero retained copy and passes with the fix:
70 assertions covering first lazy calls, captured closures, module reuse after the first importing
Program is destroyed, uncompressed/zlib/zstd/sectioned-zstd modules and executables, and immediate
and batched host registrations with either static metadata or deliberately overwritten heap input.

All 17 selected suites pass: `AOTRetainedMetadata`, `AOTMetadataCompression`, `AOTLazyFunctionContext`,
`AOTLazyMethodFastArgs`, `AOTModuleSharedClosureLifetime`, `AOTNativeClosure`, `AOTStoredCapturedClosure`,
`AOTScriptAggregateNativeRegister`, `AOTTopLevelSourceLocations`, `AOTRuntimeLocationCache`,
`AOTModuleContextPath`, `AOTPrivateStaticClosureContext`, `AOTRecursiveClosureFrame`, `AOTSmoke`,
`AOTConstructorClassContext`, `aot-module-reinjection`, and `aot-binary-symbol-resolution`.
Together they report 265 QUnit cases / 851 assertions, plus standalone regression scripts.
The smoke suite reports 235 successful cases / 614 assertions; its static-libqore case is unavailable
in this shared-library build. The lazy-function suite covers 32 concurrent first calls.

Tests use `build/qore`, `QORE_BIN` set to its absolute path, `QORE_LIBDIR` and `LD_LIBRARY_PATH` set
to the absolute build directory, and `QCC=build/qcc` as an absolute path. No qlib source was edited.
The temporary compatibility modules for measurement were compiled before use.

Valgrind runs of the new host in both `copy batch` and `borrow batch` modes report zero errors and
zero definitely, indirectly, or possibly lost bytes, with no suppressions. Command options:
`QORE_PCRE2_NO_JIT=1 valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite
--errors-for-leak-kinds=definite`. The common 162,586 still-reachable bytes are runtime/library globals.

## Audit-changes checklist

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` before the commit, with the module-structure,
module-sandboxing, cooperative-cancellation, and DataProvider checklist/development references.
Scope: runtime C++, public API parameter documentation, the qtest and its C++ host fixture, release
notes, and this audit/measurement report. No module, DataProvider, QPP class, or dependency is added.

All 62 items resolved: **21 Pass / 41 N/A / 0 Fail**.

|!Check|!Status|!Evidence
|1. Entry exists in `doxygen/lang/120_modules.dox.tmpl` (for modules in the Qore repo; N/A for external module repos)|N/A|No installed module added.
|2. Entry exists in `doxygen/lang/900_release_notes.dox.tmpl` (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe mapped metadata reuse and lazy decompression.
|3. `qore_user_module()` or `qore_external_user_module()` call in `CMakeLists.txt`|N/A|No installed module or new CMake target; qtest compiles its host fixture.
|4. Module added to QMOD list in `CMakeLists.txt`|N/A|No module added.
|5. `.qm` file has `@section <lowercasemodname>intro` as first doc section — **must be all lowercase** (e.g., `avrodataproviderintro`, not `AvroDataProviderintro`)|N/A|Only temporary test modules are generated.
|6. `%modern` in `.qm` file — no redundant `%new-style`, `%require-types`, `%strict-args`, `%enable-all-warnings`|Pass|Generated fixtures and qtest use %modern without redundant directives.
|7. No parse directives (`%requires`, `%modern`, `%new-style`) in separated `.qc` files (check OUTSIDE of `@code` blocks only)|N/A|No separated .qc files changed.
|8. No `%include` usage (deprecated for modules)|Pass|No %include introduced.
|9. Copyright 2026 on all new files|Pass|New qtest, C++ fixture, and report have copyright 2026.
|10. Directory layout: `.qm` inside `qlib/<ModuleName>/` directory (not at `qlib/<ModuleName>.qm` for multi-file modules)|N/A|No installed module added.
|11. No second `.qm` for the same module at `qlib/<ModuleName>.qm`|N/A|No installed module added.
|12. `ns=Qore::XX` matches the QoreNamespace constructor path|N/A|No QPP class added.
|13. `%modern` directive present|Pass|AOTRetainedMetadata.qtest has %modern.
|14. Executable permission set (`chmod +x`)|Pass|qtest executable mode is 100755.
|15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)|Pass|qlib prepend precedes QUnit and FsUtil requirements.
|16. External module dependencies use `%try-module` — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard `%requires`|N/A|Only shipped QUnit and FsUtil are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Production change performs no filesystem operations; dladdr queries already loaded images.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operations introduced.
|19. If filesystem/network ops exist, verify `QoreSandboxManagerHelper` usage|N/A|No new external-resource operations in runtime.
|20. No `File::`, `Dir::`, `Socket::`, `HTTPClient::` usage without justification|Pass|Test File and mkdir calls create fixtures under TmpDir; justified test setup only.
|21. All `for`/`while` loops that could iterate >100 times have `qore_check_cancel()` checks|Pass|No runtime loop introduced; existing retained-section loops are bounded to three. Host fixture visits one input buffer.
|22. Uses `qore_check_cancel()` (NOT deprecated `qore_check_io_interrupt()`)|N/A|No cancellation point added or replaced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No unbounded runtime iteration added.
|24. No blocking operations without cancellation support|Pass|No blocking I/O added; existing mutex/publication protocol preserved.
|25. Every action has `display_name`, `short_desc` (plain text, <80 chars), `desc` (markdown)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|26. Every action has `options` populated via `getActionOptionFromFields()` — without this, the action shows an empty, unusable form|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|27. Every action has `output_type` set to a typed data type constant (e.g., `MyResponseDataType`) — not omitted|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|28. DPAT_API actions: provider has `"supports_request": True` and implements `doRequestImpl()`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|29. DPAT_FIND actions: every option exists in `SearchOptions`, `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|30. Scheme-based apps (with `"scheme"` in registerApp): actions use `"path"` and do NOT use `"cls"` — having both `scheme` and `cls` causes a runtime error|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|31. Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|32. **Typed data type classes exist** for request and response types — inherit `HashDataType`, have `const Fields` hash, call `addQoreFields(Fields)` in constructor, export public constant at bottom (e.g., `public const MyDataType = new MyDataType();`)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|33. Request/input types use `public` Fields (enables `ClassName::Fields` in action registration)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|34. Response/output types use `private` Fields|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|35. Each field in data types has `display_name`, `type`, and `desc` (markdown-formatted)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|36. Input fields have `example_value` where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|37. Fields with finite allowed values use `allowed_values` with `AllowedValueInfo` containing both `value` and `display_name` (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|38. Password/secret fields have `"sensitive": True`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|39. `groups` uses `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|40. App `logo` stored as separate file, loaded at module level in `Priv` namespace|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|41. App `desc` uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|42. `display_name` is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|43. `short_desc` is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|44. `desc` uses markdown: backticks for code/field refs (`` `field_name` ``, `` `True` ``, `` `pdf` ``), `\n\n` for paragraphs, `- ` bullet lists for enumerations, `**bold**` for caveats|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|46. No bare `True`/`False`/`NOTHING` — must be backtick-wrapped in `desc`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|49. **Factory registration in Qore repo**: every factory name registered in `qlib/DataProvider/DataProvider.qc` → `FactoryMap` (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|50. **`getRecordTypeImpl()` signature**: must be `private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options)` — NOT returning `*AbstractDataProviderType`|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|51. **Dependency JARs committed** (for JNI modules): JAR files in `qlib/*/jar/` may be gitignored — use `git add -f` to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app registration, data type, factory, or JNI dependency change.
|53. **No workarounds**: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete borrow/copy policies with regression coverage; no TODO, FIXME, stub, or source fallback introduced.
|54. **Exception safety**: C++ uses `ReferenceHolder` for Qore allocations, `std::unique_ptr` for C++ allocations, `*xsink` checked after every fallible operation|Pass|Vectors, shared_ptr, and unique_ptr retain RAII ownership; reader open/decode errors checked before publication; host checks C API failures and destroys its Program.
|55. **Thread safety**: All mutable shared state protected by `std::lock_guard<std::mutex>` or documented as immutable-after-construction|Pass|Borrowed pointer/length are immutable after construction; retained reader still uses reader_mutex and eager section decode before publication.
|56. **Type safety**: Strongly-typed `code<return(args)>` instead of untyped `code`; `static_cast` instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Scoped storage enum, const byte pointers, typed closure and hash in test; dlsym uses an explicit function-pointer reinterpret_cast.
|57. **Performance**: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Three address lookups per blob replace payload copies; 24 process measurements show matching heap and anonymous-memory savings.
|58. **Error handling**: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Null/nonpositive input falls back safely; normal reader validation unchanged; unknown or mismatched images copy; tests verify registration and execution.
|59. **Documentation**: Doxygen `@param`, `@return`, `@throw` on all public methods; `@par Example` with realistic business scenarios; `@note` for important caveats|Pass|Host metadata lifetime documented in existing @param; release notes and this caller/measurement audit included. No public method signature added.
|60. **QPP flags**: `[flags=CONSTANT]` on methods that never throw; `[flags=RET_VALUE_ONLY]` on methods that throw but have no side effects|N/A|No QPP method changed.
|61. **Security**: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No credentials or new user-controlled format strings; both blob endpoints checked after normal reader validation; no new parsing or external-resource access.
|62. **Correctness**: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Baseline regression fails as expected; all compression modes, overwritten heap input, module lifetime, concurrent lazy calls, native aggregates, source locations and reinjection pass; both Valgrind modes clean.
