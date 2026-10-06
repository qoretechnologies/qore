# Runtime comparison and tracing cache audit

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

Pre-commit `audit-changes` review, 2026-10-06, on `feature/performance` against
`564d868fd8cc258797035a3ed419624853168d2f`. All applicable checks pass; conditional
checks are explicitly marked N/A below. No outstanding failure was found in
the reviewed scope. This is not a full-repository regression or sanitizer run.

## Scope

- C++: `lib/QoreIRInterpreter.cpp`; one immutable diagnostic flag replaces
  eight environment lookups. No headers, ABI, public methods, or QPP classes.
- Documentation: environment-variable reference, release notes, and the
  benchmark README, results, evidence, and this audit.
- Tests: `IRExceptionTrace.qtest`, its `.qr` fixture, and Python runner tests.
- Standalone benchmark sources: Qore, Java/JMH, and Python. No installed Qore
  modules, DataProviders, CMake targets, or JNI modules are added.

The review applied the repository guidelines, `audit-changes` checklist,
[sandboxing guide](../../design/module-sandboxing-audit-guide.md),
[cancellation guide](../../design/cooperative-cancellation.md), and
[documentation table rules](../../design/doc-tables.md).

## Registration and structure

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module added; runtime change has a release note
|qore_user_module or qore_external_user_module registration|N/A|Standalone benchmark, no installed module
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm file
|Module %modern and redundant directives|N/A|No .qm file
|No parse directives in separated .qc files|N/A|No .qc file
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|All new code, documentation, and JSON evidence carry a 2026 notice
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm file
|QPP namespace matches namespace constructor|N/A|No QPP class
|Test %modern directive|Pass|New Qore test and fixture use %modern; Python tests use Python syntax
|Test executable permission|Pass|New .qtest, .qr, and Python test script are executable
|Module path precedes %requires|Pass|IRExceptionTrace.qtest prepends the in-repo qlib before requiring QUnit
|External modules use %try-module|N/A|Only QUnit, delivered with Qore, is required; the benchmark and fixture import no Qore modules

## Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|Changed C++ lines only read a diagnostic environment flag and reuse the resulting bool
|No unchecked C++ network operations|Pass|No network operation added
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such operation added to the runtime
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|None in the added Qore sources; the test intentionally starts trusted child fixtures under %no-child-restrictions, quoting every path/value
|C++ loops over 100 iterations check cancellation|N/A|No loop added or modified; all existing cancellation points remain
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|N/A|No cancellation call added or modified
|Cancellation frequency of 100 tight or 10 expensive iterations|N/A|No loop/polling-frequency change
|No uncancellable blocking operation|Pass|No runtime blocking operation added; diagnostic fprintf behavior is unchanged; runner subprocesses have timeouts

The standalone Python runner performs explicit local builds and downloads
checksum-pinned JMH dependencies from Maven Central. It is development tooling,
not a Qore sandbox API or an installed runtime module. The benchmark does not
disable Qore cancellation or sandbox checks to improve scores.

## DataProvider checklist

|!Check|!Status|!Evidence
|Action display_name, short_desc, desc|N/A|No action registration
|Action options from getActionOptionFromFields|N/A|No action registration
|Typed action output_type|N/A|No action registration
|DPAT_API supports_request and doRequestImpl|N/A|No provider
|DPAT_FIND SearchOptions and getRecordTypeImpl|N/A|No provider
|Scheme app actions use path without cls|N/A|No app registration
|Single-key hash slices have trailing comma|N/A|No hash slice added
|Request/response HashDataType classes, Fields, addQoreFields, public constants|N/A|No DataProvider types
|Input Fields public|N/A|No DataProvider types
|Output Fields private|N/A|No DataProvider types
|Field display_name, type, desc|N/A|No DataProvider fields
|Input example_value where useful|N/A|No DataProvider fields
|Finite values use AllowedValueInfo value/display_name|N/A|No DataProvider fields
|Secret fields use sensitive=True|N/A|No credential fields
|AppGroup enum for app groups|N/A|No app registration
|Separate logo loaded in module-level Priv namespace|N/A|No app registration
|App description includes capabilities, website, business value|N/A|No app registration
|Friendly app display_name|N/A|No app registration
|Plain-text short_desc below 80 characters|N/A|No action/app descriptions
|Markdown desc formatting and caveats|N/A|No action/app descriptions
|Business language explains why/when|N/A|No action/app descriptions
|Backticks around True, False, NOTHING in desc|N/A|No action/app descriptions
|Backticks around field/option names in desc|N/A|No action/app descriptions
|Long descriptions use section headings and lists|N/A|No action/app descriptions
|FactoryMap cross-repository registration|N/A|No DataProvider factory
|Exact getRecordTypeImpl signature|N/A|No provider override
|JNI dependency JARs committed|N/A|No JNI module; JMH JARs are checksum-pinned development dependencies downloaded into ignored build output
|JAR install rules in CMake|N/A|No installed Java component

## Code review

|!Check|!Status|!Evidence
|No workarounds, TODOs, FIXMEs, stubs|Pass|All eight diagnostic guards use the same cached value; runner, kernels, and tests execute end-to-end; future optimization targets are documentation, not partial runtime implementations
|Exception safety|Pass|No C++ allocation or new fallible Qore operation; return/cleanup/xsink logic unchanged; failed benchmark/build subprocesses fail the runner
|Thread safety|Pass|Function-local static initialization is synchronized by C++; cached bool is immutable thereafter; mutable benchmark state is process-local or JMH Scope.Thread
|Type safety|Pass|C++ uses const bool without casts; Qore uses typed integers/lists/hashes; Java uses long values and typed collections; parser validates numerical records
|Performance|Pass|Removes repeated environment scans; interleaved comparison shows 23.3%/19.5% lower call-kernel time with separated startup; no complexity increase
|Error handling|Pass|Bounds and duplicate modes/kernels checked; checksums/record counts/units validated; timeouts and subprocess errors propagate; invalid CLI tests and deliberate compiler failure verified
|Public method Doxygen params, returns, throws, examples, caveats|N/A|No public runtime API added; standalone scripts have usage, workload, measurement, and interpretation documentation
|Documentation and release notes|Pass|Read-once diagnostic behavior, empty/zero values, benchmark commands, results, caveats, and follow-up invariants documented; strict Qore table validation passes
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP method
|Security|Pass|No user-controlled format strings; fixed benchmark indices are bounded; child arguments use shell quoting or shell-free argv; dependency hashes checked; committed evidence excludes environment values and local paths
|Correctness and edge cases|Pass|480 independent cross-mode reference checks include empty/singleton and boundary seeds; tracing tests cover all presence values and late changes; focused ownership/call/exception regressions pass

Review corrections included preserving the existing release-note paragraph's
reference to its preceding changes, narrowing recorded environment variables
to runtime/JVM settings, removing unrelated environment fields from earlier
local metadata, and spacing the optional before/after output columns correctly.
The six harness tests and final four-mode smoke run pass after those changes.

## Verification evidence

Commands were run from the repository root. `QORE_LIBDIR` selected the rebuilt
library for tests that start subprocesses; the executable's macOS rpath also
points to `build`. No installation, push, or external repository change was
needed.

|!Verification|!Result
|cmake --build build --target qore qcc -j 3|Pass; rebuilt runtime and tools, with existing compiler/linker warnings
|python3 bench/runtime-comparison/test_runner.py|Pass; 6 tests, including malformed/duplicate/missing records, reference values, timing validation, and invalid CLI arguments
|python3 bench/runtime-comparison/run.py --check-only|Pass; 120 checks in each of four modes
|Final smoke: --forks 1 --warmup 1 --samples 2 --time-ms 50 --startup-samples 2|Pass; all 32 kernel/mode jobs and all 480 reference checks
|Baseline default runner invocation|Pass; 96 jobs, 480 measurement samples, 480 reference checks
|Interleaved --baseline-libdir comparison documented in RESULTS.md|Pass; 36 jobs, 180 measurement samples, 240 reference checks
|IRExceptionTrace.qtest, AST and IR test-driver modes|Pass; 4 cases, 41 assertions per run
|IRExceptionTrace.qtest against preserved old library|Expected failure; all 4 cases detect late environment changes, validating the regression test
|IRReturnBorrowedSlot.qtest, IR test-driver mode|Pass; 2 cases, 10002 assertions
|IRIteratorExceptionHandler.qtest, IR test-driver mode|Pass; 2 cases, 7 assertions, including execution-mode and AOT comparisons
|IRNativeIntLeafInline.qtest|Pass; exit 0, including mode/optimization/AOT checks
|ParameterDirectCall.qtest, IR test-driver mode|Pass; 4 cases, 31 assertions
|Runner with --modes qore-aot --qcc /usr/bin/false --check-only|Expected exit 1 with compiler failure and log location; no invalid result accepted
|build/qpp --table-strict --table=FILE --output=/dev/null|Pass for both changed .dox.tmpl files and README.md, RESULTS.md, AUDIT.md
|git diff --check and staged equivalent|Pass; no whitespace errors

The runtime test invocation pattern was:

```sh
DYLD_LIBRARY_PATH="$PWD/build" QORE_LIBDIR="$PWD/build" \
    build/qore --exec-mode=ir examples/test/ir/IRExceptionTrace.qtest
```

No qlib source changed, so no qmod rebuild was required. No allocation/ownership
logic changed, so sanitizer/leak testing was not added for this diagnostic-only
runtime change. Larger AOT changes require the dedicated semantic and
cancellation tests listed in [RESULTS.md](RESULTS.md).
