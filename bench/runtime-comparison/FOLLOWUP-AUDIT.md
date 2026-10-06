# Cancellation and indexed-mutation audit

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

Pre-commit `audit-changes` review, 2026-10-06, on `feature/performance`
against `125ca95e48cb648460397bcf6f1ed891d2b94ede`. This records every checklist
item, including conditional N/A items. It is a focused review, not a full
repository regression or sanitizer run.

All applicable checks pass after corrections; no outstanding failure remains
in the retained scope. Performance conclusions and mixed control results are
reported explicitly, not treated as universal improvements.

## Scope

- Runtime: conservative pending-interrupt count and negative-path cancellation
  check; allocation-free native/JIT/AOT root/index compound-path metadata.
- Internal header/API only; no public ABI/layout, QPP, module, or CMake changes.
- Tests: native interruption fixture/wrapper, indexed-mutation fixture, and
  bounded-pair ownership fixture, checked across execution modes.
- Harness: compatible baseline libraries now compare AOT compiler/runtime
  changes as well as IR.
- Documentation: cancellation/lvalue invariants, release notes, measurement
  instructions, follow-up results and sanitized evidence.

The aggregate scalar-replacement prototype was rejected before committing.
Its first version failed full-width integer ownership checks; its conservative
version passed but did not eliminate the measured kernel's list. No optimizer
change remains in the diff. The semantic fixture is retained for future work.

The interpreter-specific indexed-path specialization was also rejected: its
target improved, but several unrelated IR controls regressed by 3-5%. The native
path is retained and the final narrowed change is measured separately.

The repository instructions and complete sandboxing/cancellation design guides
were read and applied. No qlib source changed; qmod rebuilding is not required.

## Registration and structure

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module added; both runtime changes have a release note
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
|Test %modern directive|Pass|Both new Qore tests and both fixtures use %modern
|Test executable permission|Pass|Both new .qtest files and both .qr fixtures have executable permission
|Module path precedes %requires|Pass|Both wrappers prepend the in-repo qlib before requiring QUnit and FsUtil
|External modules use %try-module|N/A|Only QUnit and FsUtil, delivered with Qore, are required

## Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|Runtime changes add no filesystem access; native fixture uses public runtime APIs and stdout only
|No unchecked C++ network operations|Pass|No network operation added
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such operation added; fast path affects interruption lookup, not policy checks
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|Wrappers use FsUtil temporary directories and trusted child processes under explicit %no-child-restrictions; all shell arguments are quoted
|C++ loops over 100 iterations check cancellation|Pass|No new runtime loop; new native stress loops call qore_check_cancel every 100 iterations or every iteration
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|New fixture and modified fast path use the unified API
|Cancellation frequency of 100 tight or 10 expensive iterations|Pass|Existing runtime polls are unchanged; native stress loops poll at least every 100 iterations
|No uncancellable blocking operation|Pass|No runtime blocking operation added; fixture joins bounded worker loops that poll cancellation

The count is an atomic conservative upper bound, not a cached manager pointer.
Request increments before publishing its flag; repeat requests relinquish their
provisional increment. Clear resets the flag before decrementing; destruction
relinquishes any outstanding flag. Sequential consistency is preserved across
all count/flag operations and the existing waiter registration/wake protocol.
Transient false positives perform the normal context lookup. A zero cannot
hide a flag already set in that ordering. Refcounted manager lifetime remains
the existing caller contract; copying a manager still does not copy interrupt
state. Thread cancellation, scopes, deferral, and manager priority are unchanged.

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
|No workarounds, TODOs, FIXMEs, stubs|Pass|Two complete runtime fast paths; unsafe/unmeasured compiler prototype excluded, not left as disabled production code
|Exception safety|Pass|RHS ValueHolder, lvalue lock lifetime, cache invalidation, xsink checks and cleanup remain unchanged; no new Qore allocation
|Thread safety|Pass|Global count uses documented sequentially consistent atomics; index override is per invocation, never writes shared IR metadata; concurrent request and dynamic-index tests included
|Type safety|Pass|int64_t index and uint64_t atomic count; existing getAsBigInt conversion and navigation/type validation retained; typed Qore closures
|Performance|Pass|Interleaved measurements establish cancellation and indexed-mutation benefits; individual and cumulative results retained with controls and caveats
|Error handling|Pass|Only the proven two-step root/ListIndex shape uses the override; general paths retain old code; benchmark/test subprocess exit codes are checked
|Public method Doxygen params, returns, throws, examples, caveats|N/A|No public method added; internal navigatePath parameter and its invariant documented
|Documentation and release notes|Pass|Cancellation ordering, metadata-only mutation optimization, compatible AOT baseline compilation and rejected prototype explained
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP changes
|Security|Pass|No user-controlled formats, new unchecked array indexing or credentials; dyn_vals[0] used only for the one dynamic index of a valid root/index path; evidence excludes environment/local paths
|Correctness and edge cases|Pass|AST reference comparisons cover aliases, references, captures, member/global roots, concurrent indices, negative bounds, auto-vivification, float elements and exceptions; full-width/escaping/mutated/boxed aggregate fallbacks retained

Review corrections included maintaining the exception-tracing release-note
paragraph, correcting the old path comment's index field, clarifying nonzero
cancellation overhead, converting five existing cancellation-document tables to
the repository format, and rejecting the unsafe/unproven aggregate optimization.
The `navigatePath` override never bypasses locking, COW, reference resolution,
type checks, bounds/negative-index handling, auto-vivification or cleanup.

## Verification

Commands run from the repository root; runtime tests used
`DYLD_LIBRARY_PATH="$PWD/build" QORE_LIBDIR="$PWD/build" build/qore --exec-mode=ir FILE`.
The scalarizer and IR-dispatch experimental changes are absent from the final
diff. The final library is rebuilt and measured after both reversions.

|!Verification|!Result
|cmake --build build --target qore qcc -j 3|Pass; final release build with existing warnings
|python3 bench/runtime-comparison/test_runner.py|Pass; 6 harness tests
|run.py --modes qore-aot --baseline-libdir SAVED --check-only|Pass; standalone AOT before/after selection, 240 reference checks
|IRPerformanceSemantics.qtest|Pass; 2 cases, 33 wrapper assertions, AST/IR/JIT/tiered and stripped-source AOT; pair fixture repeated with scalarization disabled at compilation/execution
|program-interrupt-fast-path.qtest|Pass; 1 case, 3 wrapper assertions plus native context/priority/deferral/concurrency checks
|thread-cancel.qtest|Pass; 47 cases, 81 assertions
|thread-cancel-cleanup.qtest|Pass; 18 cases, 89 assertions
|thread-cancel-lifecycle.qtest|Pass; 3 cases, 18 assertions
|sandbox-manager.qtest|Pass; 66 cases, 392 assertions, including blocking interruption/races and manager lifetime
|policy-barrier.qtest|Pass; 4 cases, 6 assertions
|LValuePathSmoke.qtest|Pass; 16 cases, 32 assertions
|LValueSlice.qtest|Pass; 14 cases, 56 assertions
|LocalVarLValueLookup.qtest|Pass; 4 cases, 36 assertions
|IRReferencedContainerCow.qtest, IRListPushLocalCow.qtest|Pass; both standalone checks exit 0
|IRFixedListScalarReplacement.qtest, AOTWideIntExpressionSummary.qtest|Pass; both standalone checks exit 0
|build/qpp --table-strict --table=FILE --output=/dev/null|Pass for all changed documentation after table normalization
|git diff --check and staged equivalent|Pass
|Committed evidence validation|Pass; 522 unique jobs, 2610 positive timing samples, expected fork/sample counts and independently recomputed summary medians; no local paths or environment values
|Final interleaved comparison, 1024 elements, 5 forks|Pass; 160 jobs, 800 timing samples, 480 reference checks
|Larger-batch comparison, 4096 elements, 3 forks|Pass; 48 jobs, 240 timing samples, 480 reference checks
|Focused AOT hash repeat, 4096 elements, 5 forks, 500 ms windows|Pass; 10 jobs, 50 timing samples, 240 reference checks; earlier small slowdown did not consistently reproduce

Total: 175 QUnit cases, 746 assertions, plus four standalone test scripts.
The native fixture's internal assertions and fixture expect() checks are additional.
No sanitizer/full-suite run was performed. Timing results are microbenchmarks on
a shared workstation, not a universal speedup or a production latency guarantee.

## Container follow-up audit (2026-10-06)

This additional `audit-changes` review covers changes after merge
`6c87c18cf17599bcafd62b1240c08fdd551b39b4` on `feature/performance`. The preceding
audit remains a record of the earlier cancellation/indexed-path changes.

Scope: identical-type metadata comparison, inline storage for the first
`LValueHelper` deferred reference, two semantic fixtures and their all-mode
wrapper, a sandbox-context thread-pool benchmark, harness tests, design notes,
release notes, and measured evidence. No QPP, public API, installed module,
parser, or qlib source changes. No agent delegation was used.

### Registration, structure, and test checks

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module added; runtime changes have a release note
|qore_user_module or qore_external_user_module registration|N/A|No installed module added
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm source changed
|Module %modern and redundant directives|N/A|No .qm source changed
|No parse directives in separated .qc files|N/A|No .qc source changed
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|Both fixtures, both benchmark files, and JSON evidence carry a 2026 notice
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm added
|QPP namespace matches namespace constructor|N/A|No QPP class added
|Test %modern directive|Pass|Both new fixtures, the benchmark, and the changed wrapper use %modern
|Test executable permission|Pass|Both fixtures and both benchmark scripts are executable; wrapper retains its executable mode
|Module path precedes %requires|Pass|The wrapper prepends in-repo qlib before QUnit/FsUtil; fixtures and benchmark need no module
|External modules use %try-module|N/A|Only Qore-delivered QUnit and FsUtil are required

### Sandboxing and cancellation checks

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|Runtime diff adds no filesystem operations
|No unchecked C++ network operations|Pass|Runtime diff adds no network operations
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such operation added
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|Fixtures/benchmark add no such calls; existing wrapper uses trusted FsUtil temporary directories and quoted child commands
|C++ loops over 100 iterations check cancellation|Pass|No new loop; existing scan and mandatory deferred-release loops are retained, with one inline element handled outside them
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|Neither API is changed or added; existing runtime polling remains
|Cancellation frequency of 100 tight or 10 expensive iterations|N/A|No new C++ loop or work proportional to input size; cleanup must drain all owned references even with cancellation pending
|No uncancellable blocking operation|Pass|No runtime blocking operation added; benchmark Queue reads have a 30-second timeout and subprocess execution has a timeout

The complete sandboxing and cooperative-cancellation guides were applied.
The pool benchmark attaches a manager to its submitting Program; it does not
bypass checks or claim to validate enforcement. Existing ThreadPool,
HttpServerListenerSandbox, sandbox-manager, interruption, and cancellation
regressions were run separately. No qlib module rebuild is needed.

### DataProvider checks

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
|JNI dependency JARs committed|N/A|No JNI or installed Java component
|JAR install rules in CMake|N/A|No JAR dependency added

### Code review checks

|!Check|!Status|!Evidence
|No workarounds, TODOs, FIXMEs, stubs|Pass|Both complete runtime changes preserve general fallbacks; no partially implemented compiler optimization
|Exception safety|Pass|No new allocation; inline ownership transfers exactly once, locks and scans precede release, overflow cleanup continues after destructor exceptions; move clears the source slot
|Thread safety|Pass|Inline storage is invocation-local; metadata identity is read-only; existing object locks, recursive scans, and acquire load of recursive-set membership remain
|Type safety|Pass|Existing AbstractQoreNode storage and static_cast pattern; both hasType guards retained before identity shortcut; benchmark closures have explicit return/argument types
|Performance|Pass|Separate identity-only and inline-storage experiments, then interleaved combined comparison and controls; no quadratic loop or extra container copy introduced
|Error handling|Pass|Null/auto metadata keep old behavior; unequal types use structural comparison; benchmark validates arguments, every task result, process status, and exact sample/check counts
|Public method Doxygen params, returns, throws, examples, caveats|N/A|No public method or API added; internal lifetime invariant and measurement limitations documented
|Documentation and release notes|Pass|Design notes describe type-check boundaries, lock/scan/release order, overflow, and move transfer; README defines pool timing units and semantics
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP changes
|Security|Pass|No format-string or unchecked indexed-access change; no credentials; evidence strips environment and local paths; no policy bypass
|Correctness and edge cases|Pass|All-mode AST reference comparisons cover empty/optional/heterogeneous/nested containers, soft conversions, full-width integers, aliases, rejected writes, destructor re-entry, exceptions, and overflow; debug DGC counters remain correct

The inline slot holds the same first reference that previously occupied
`tvec[0]`. It participates in the insertion-only predicate, remains held during
the scan, and is released before the remaining vector entries. It does not
change COW eligibility or the recursive graph. General overflow remains a
vector, not a fixed limit. The internal helper gains one pointer; no public
class layout or runtime helper ABI changes. Saved-library comparisons use
compatible qore/qcc tools and independently compiled AOT executables.

### Correctness verification

Release commands used `DYLD_LIBRARY_PATH="$PWD/build" QORE_LIBDIR="$PWD/build"
QORE_BIN="$PWD/build/qore" QCC="$PWD/build/qcc" build/qore --exec-mode=ir FILE`.
Debug commands substituted `build-debug`. Subprocess wrappers also exercise
their own AST/IR/JIT/tiered/AOT or compiled-module variants.

|!Verification|!Result
|Release qore/qcc build|Pass; existing compiler warnings
|Debug qore/qcc build|Pass with -DDEBUG -g -Og; stale SDK cache updated to installed SDK 27.0 and HAVE_PIPE2=0 for deployment target 26.3, no tracked build change
|Harness unit tests|Pass; 8 tests
|New semantics against saved baseline|Pass; 4 cases, 66 assertions across AST/IR/JIT/tiered/stripped AOT
|IRPerformanceSemantics, release and debug|Pass; 4 cases, 66 assertions each
|list, hash, hashdecl|Pass; 18 cases, 454 assertions
|lvalue-delete-semantics, lvalue-delete-cycle-scan|Pass; 15 cases, 45 assertions
|DGC deferred/removed/shared/graph/closed/unchanged/concurrent/deref suites|Pass; all eight files
|DGC scan avoidance, release|Pass; 7 cases, 552 assertions
|DGC scan avoidance, debug|Pass; 7 cases, 1482 assertions including scan counters
|Weak container references, release and debug|Pass; 2 cases, 61 assertions each
|TypedConversionTiers|Pass; 17 cases, 46 assertions
|Lvalue operand temporaries and assignment ownership, release and debug|Pass; 4 cases, 182 assertions each build
|IRReferencedContainerCow, IRListPushLocalCow, IRInPlaceListPush, AOTFreshContainerInit|Pass; four standalone checks
|Program interruption, cancellation cleanup/lifecycle|Pass; 22 cases, 110 assertions
|sandbox-manager|Pass; 67 cases, 408 assertions
|ThreadPool and HttpServerListenerSandbox|Pass; 7 cases, 14 assertions
|Pool before/after reference checks|Pass; 8 checks in each of IR/AOT before/after
|OptionalMapTypeFolding wrapper|Known baseline/environment failure; 2 of 3 cases pass, AOT compile exits 0 but wrapper rejects macOS deployment-version linker warning, identically before and after
|OptionalMapTypeFolding stripped AOT, direct execution|Pass with normal and checked append paths; actual program output correct despite the pre-existing link warning

The successful release/debug suites total 249 QUnit cases and 3909 assertions,
plus four standalone scripts. The known warning-sensitive wrapper and its
partial assertions are excluded from that total. No sanitizer or full-suite
run was performed. This audit does not claim every repository test passes.

### Measurement and final audit gates

|!Verification|!Result
|Refreshed Qore/Java baseline|Pass; 96 jobs, 480 timing samples, 480 reference checks
|Isolated type-identity comparison|Pass; 96 jobs, 480 timing samples, 480 reference checks
|Isolated inline-reference comparison|Pass; 60 jobs, 300 timing samples, 480 reference checks
|Final combined comparison, 1024 elements|Pass; 160 jobs, 800 timing samples, 480 reference checks
|Larger-batch comparison, 4096 elements|Pass; 72 jobs, 360 timing samples, 480 reference checks
|Focused IR construction/pair repeat|Pass; 20 jobs, 100 timing samples, 240 reference checks
|Thread-pool comparison|Pass; 96 jobs, 480 timing samples, 32 reference checks
|Independent evidence validation|Pass; 600 unique jobs, 3000 positive timing samples; all process means/summary medians/ranges/fork and sample counts recomputed; local paths and environment removed
|Strict Qore documentation table validation|Pass for every changed documentation file with build/qpp --table-strict
|Working and staged git diff --check|Pass

The retained changes improve targeted AOT container operations and IR mutation,
including sandbox-context task workloads, but do not improve every control.
The larger-batch IR construction medians were 2.8% and 1.8% slower in successive
runs with overlapping ranges; AOT arithmetic/direct calls were around 1% slower.
These are documented tradeoffs rather than hidden failures. Runtime builds,
Qore regression suites, and profiling were separate from timing; lightweight
validation and ordinary workstation background activity were not eliminated.
No production latency guarantee is inferred from these microbenchmarks.

## Call-frame and call-boundary audit (2026-10-06)

Review scope: changes after `b7de8f1f92274e4d5ce9e5ab76983c4b318d772f` on
`feature/performance`. The complete `audit-changes` checklist and the sandboxing,
cooperative-cancellation, and documentation-table guides were applied. No agent
delegation was used. No qlib source changed; qmod rebuilding is not required.

Runtime changes are private to `IRCallFrame`: pooled scope watermarks and a
dense local-owned-slot bitmap. Other changes are benchmark kernels/harnesses,
one regression test, release notes, and measurement/review records. No context
switching, public ABI, QPP, parser, compiler lowering, or installed module changed.

### Registration, structure, and tests

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module; runtime change has a release note
|qore_user_module or qore_external_user_module registration|N/A|No installed module
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm file changed
|Module %modern and redundant directives|N/A|No .qm file changed
|No parse directives in separated .qc files|N/A|No .qc file changed
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|New test, benchmark scripts, and evidence carry 2026 notices
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm added
|QPP namespace matches namespace constructor|N/A|No QPP class added
|Test %modern directive|Pass|New regression and benchmark fixtures use %modern
|Test executable permission|Pass|New .qtest, .qr, and benchmark driver are executable
|Module path precedes %requires|Pass|Regression prepends in-repo qlib before requiring QUnit
|External modules use %try-module|N/A|Only Qore-delivered QUnit is required

### Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|None added
|No unchecked C++ network operations|Pass|None added
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such C++ operation added
|Qore File, Dir, Socket, HTTPClient usage justified|N/A|No such Qore operation added
|C++ loops over 100 iterations check cancellation|Pass|No loop added; existing dispatch, scope cleanup, and DGC traversal retain their control flow
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|No polling API changed or added
|Cancellation frequency of 100 tight or 10 expensive iterations|N/A|No new explicit loop; contiguous byte initialization joins existing frame-array reset operations
|No uncancellable blocking operation|Pass|No runtime blocking operation; benchmark subprocesses have duration-derived timeouts

### DataProvider checklist

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
|JNI dependency JARs committed|N/A|No JNI module; existing checksum-pinned JMH development dependencies remain in ignored build output
|JAR install rules in CMake|N/A|No installed Java component

### Code review

|!Check|!Status|!Evidence
|No workarounds, TODOs, FIXMEs, stubs|Pass|Complete storage change; future explicit-context/native-entry work is documentation, not disabled production code
|Exception safety|Pass|Only POD metadata storage changes; reference cleanup and ScopeExitGuard run before FrameGuard pops the pool; no new Qore allocation or fallible runtime helper
|Thread safety|Pass|Pool is thread-local; recursive/reentrant calls use distinct stable heap frames even when the pool vector grows
|Type safety|Pass|uint8_t ownership flags, size_t scope watermarks, strongly typed callbacks and regression containers; no new C-style casts
|Performance|Pass|Interleaved before/after measurements at 1024 and 4096 elements and a five-fork program-boundary probe; modest IR gains, direct-call repeat near noise, and mixed AOT controls explicitly reported
|Error handling|Pass|Array accesses are bounds-checked; UINT32_MAX init sentinels ignored as before; harness validates ranges, mode, record sets, checksums, and subprocess exit status
|Public method Doxygen params, returns, throws, examples, caveats|N/A|No public runtime method added
|Documentation|Pass|Frame lifetime/invariants, retained-call semantics, context limitations, reproduction commands, and release notes documented
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP changes
|Security|Pass|No credentials or user-controlled format strings; Python launches argument arrays without a shell; no program/sandbox transition bypass
|Correctness and edge cases|Pass|Regression covers normal/exceptional recursive scope exit, callback reentry, scalar/container frame reuse, and returned container lifetimes; reference checks include zero/one/large counts and boundary seeds

The ownership set is indexed by the same dense value ID bound already used by
`values`, cleanup counts, and load-slot registration. Marks are not restricted
to scalar or node types, preserving the previous set's semantics. Every erase
site is retained, including the local-init sentinel path and transferred-owned
operand path. The scope stack contains indices only, not owned references;
pooling it changes allocation lifetime, not handler execution order.

Review corrections: replaced the inaccurate allocation-free frame comment,
fixed the new fixture's ambiguous hash type, validated benchmark library paths,
and explicitly set/check `EM_IR` on both probe programs. Child programs otherwise
can reset an inherited mode when their parse options change, silently measuring
tiered/native execution instead. No measurement from that unpinned probe is
used as IR evidence.

### Verification and limitations

|!Verification|!Result
|Release and debug cmake builds, targets qore qcc, -j3|Pass; existing compiler/linker warnings
|Python harness unit tests|Pass; 10 tests including missing/duplicate/wrong records and invalid program-probe arguments
|IRCallFrameReuse.qtest|Pass; release AST/IR/JIT/tiered and debug IR, each 2 cases and 1200 assertions; also passed on saved baseline IR
|IRBlockScopedObjectLifetime.qtest|Pass; release/debug, each 8 cases and 9 assertions; wrapper covers AST/IR/JIT/tiered/promoted and AOT fixtures
|IRFusedIntLocalOwnership.qtest|Pass for value/lifetime checks; 15 reported cases, 9 assertions, including 7 Linux-only RSS cases skipped on macOS
|IRReturnBorrowedSlot.qtest|Pass; release/debug, each 2 cases and 10002 assertions
|IRWeakReferenceLocalStore.qtest|Pass; release/debug, each 1 case and 12 assertions
|IRBorrowedListRead.qtest|Pass; standalone all-mode/AOT checks, exit 0
|IRExceptionTrace.qtest|Pass; release/debug, each 4 cases and 41 assertions
|JITSmoke.qtest|Pass; release, 167 cases and 1171 assertions
|IRExecModeSmoke.qtest|Pass; release, 159 cases and 496 assertions
|RecursiveClosureLocals.qtest|Existing environment failure reproduced before/after: its AOT wrapper rejects a macOS object-target linker warning; other two cases pass
|RecursiveClosureLocals.qr, -O3 --strip-source --parse-option=PO_ENABLE_DEBUG|Pass; manual compile/run bypassing only the wrapper's warning assertion, prints recursive-closure-locals: ok
|Java/Qore baseline plus two interleaved comparisons|Pass; 660 independent-reference checks each, including the three new call/selection workloads
|Pinned-IR program-boundary comparison|Pass; 60 reference checks, 5 forks per library/scenario
|Native/JVM call retention|Pass; AOT loop disassembly retains call helpers; JMH compiler hints prohibit callee inlining, and diagnostic PrintInlining confirms boundaryStep was not inlined
|Committed measurement evidence|Pass; 296 unique jobs, 1480 positive samples, expected fork/sample counts and independently recomputed medians; local paths/environment excluded
|Strict Qore documentation tables|Pass; qpp --table-strict for all changed documentation
|Working/staged git diff --check|Pass

No full-suite, sanitizer, or allocation-counting run was performed. The storage
change is justified by sampled allocation stacks and measured workloads, not
an exact malloc count or production latency claim. The retained runtime diff
does not implement a new explicit-context calling convention or AOT fast entry.

## Function-reference native-entry audit (2026-10-06)

This `audit-changes` review covers changes after `54e3d929b` on
`feature/performance`: guarded function-reference dispatch, an additive native
adapter ABI, compiler lowering, semantic fixtures, and documentation/evidence.
The complete sandboxing, cancellation, and documentation-table guides were
applied. No qlib, parser, QPP, public ABI, or serialized AOT descriptor changed.
No agents were delegated work. The earlier audits above describe earlier commits.

### Registration, structure, and tests

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module; runtime change has a release note
|qore_user_module or qore_external_user_module registration|N/A|No installed module
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm changed
|Module %modern and redundant directives|N/A|No .qm changed
|No parse directives in separated .qc files|N/A|No .qc changed
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|Both fixtures and measurement evidence carry 2026 notices
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm added
|QPP namespace matches namespace constructor|N/A|No QPP class
|Test %modern directive|Pass|Both new test files use %modern
|Test executable permission|Pass|Both .qr and .qtest have executable permission
|Module path precedes %requires|Pass|Wrapper prepends repository qlib before QUnit and FsUtil
|External modules use %try-module|N/A|Only Qore-delivered QUnit and FsUtil are required

### Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|None added
|No unchecked C++ network operations|Pass|None added; bind_function_call_ref_native binds code metadata, not a socket
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such C++ operation added
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|Wrapper uses FsUtil-owned temporary directory and quoted trusted subprocess arguments under explicit no-child-restrictions
|C++ loops over 100 iterations check cancellation|Pass|New loops are bounded by 16 arguments or 64 scalar-expression nodes; signature count is checked before existing static eligibility scans
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|Existing compiler/interpreter/native loop polling is unchanged; no deprecated API added
|Cancellation frequency of 100 tight or 10 expensive iterations|N/A|No new unbounded loop; native adapter calls an existing compiled body with its existing polls
|No uncancellable blocking operation|Pass|No runtime blocking operation added

The fast path checks both reference and variant ownership against the active
Program; it never bypasses a Program boundary. Domain-bearing variants fall
back. Program call context, diagnostic frame, parse options/location, stack
checks, and native exception handling remain. Attached debuggers fall back.

### DataProvider checklist

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
|JNI dependency JARs committed|N/A|No JNI change
|JAR install rules in CMake|N/A|No Java installation change

### Code review

|!Check|!Status|!Evidence
|No workarounds, TODOs, FIXMEs, stubs|Pass|Complete conservative adapter with generic fallback, not a partial replacement of the general calling convention
|Exception safety|Pass|Arguments borrowed until return; ValueHolder protects owned result during argument cleanup; RAII restores diagnostic state on native unwind; QoreJITException caught at C++ boundary
|Thread safety|Pass|Adapter and code-identity fields written only before reference publication, then immutable; no shared last-target cache; existing Program metadata lifetime/publication applies
|Type safety|Pass|Typed native function pointer and typed scalar test drivers; intentionally untyped test drivers exercise coercion/overload/error fallback rather than asserting a false static contract
|Performance|Pass|Separate saved baseline compiler/library; interleaved process-level measurements and larger-batch repeat; controls and program-boundary probe recorded separately
|Error handling|Pass|Check reference kind, owning Program, unique variant, argument count, exact binding, native code identity, debugger and execution mode; preserve generic fallback
|Public method Doxygen params, returns, throws, examples, caveats|N/A|Only internal runtime/compiler helpers added
|Documentation|Pass|ABI ownership, eligibility, lifetime, compatibility limits, diagnostic switches and release notes documented
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP changed
|Security|Pass|No new I/O capability, unchecked user indices, user-controlled format strings or credentials; compiled adapter is bound by code identity, not function name alone
|Correctness and edge cases|Pass|AST-referenced all-mode tests cover zero/multiple args, integer/float boxing, bools, boxed ownership/COW, NOTHING, defaults, overloads, coercion, references, no-narrow, varargs, recursion, methods, exception stacks and foreign Program calls in both directions

Audit corrections: added debugger and execution-mode fallback, bounded the
signature scan before static eligibility, and changed the adapter to pass the
callee's AOT context explicitly. A context-independent body can still need its
context for stripped-debug source locations and return-type caches. The initial
null-context timing run was stopped and excluded from final evidence. The
fixture compares throwing source locations with AST and includes stripped-debug
AOT compilation. That stronger test also exposed missing lazy PC maps for
adapter frames; native eligibility was restricted to compiler-proven
non-throwing scalar expressions. Potentially throwing bodies use the normal
registered entry through the runtime fast path. No serialized ABI extension
or native-symbol lookup is needed.

The final exception-safety pass also checks the sink after installing the
runtime location (which can report stack exhaustion). Restoration uses the
non-fallible location update, matching QoreProgramLocationHelper, instead of
performing another stack check during destruction. Both builds, regression
checks, and targeted before/after measurements were rerun after this correction.

Performance review also moved the program-identity guard ahead of RTTI:
the initial same-program optimization added 2.3-3.4% to the cross-program
control in two runs. Foreign references now reject before the dynamic cast.
Both guard orderings are distinguished in the measurement evidence; final
performance claims use the program-first ordering.

This remains a focused review, not a full-suite or sanitizer run. The adapter
does not yet cover potentially throwing or context-dependent bodies, boxed
returns, method references, or imported fast entries, and is not a universal
TLS-free calling convention.

### Verification

|!Verification|!Result
|Release/debug cmake builds, qore and qcc, -j3|Pass; existing warnings only
|IRFunctionRefCalls.qtest|Pass release/debug, each 1 case and 33 wrapper assertions, plus 100 repetitions of the semantic matrix per child; AST/IR/JIT/tiered, runtime-disabled, AOT adapter-disabled, stripped-source and stripped-debug builds
|AOTKnownCallRefFastEntry, AOTGenericFunctionFastEntry, AOTGenericCrossObjectFastEntry, AOTOptionalScalarFastEntry, AOTNativeClosure|Pass; all five standalone scripts exit 0
|AOTQoLinkNativeSymbols|Pass; 4 cases, 38 assertions
|IRCallFrameReuse|Pass release/debug; each 2 cases, 1200 assertions
|IRExecModeSmoke|Pass; 159 cases, 496 assertions
|IRExceptionTrace|Pass release/debug; each 4 cases, 41 assertions
|IRReturnBorrowedSlot|Pass release/debug; each 2 cases, 10002 assertions
|JitDebugStep|Pass; 1 case, 7 assertions
|policy-barrier|Pass; 4 cases, 6 assertions
|sandbox-manager|Pass; 67 cases, 408 assertions
|thread-cancel|Pass; 47 cases, 81 assertions
|test-debug|Pass; 10 cases, 847 assertions
|Harness unit tests|Pass; 10 tests
|Pre-change AOT executable with new runtime|Pass; all 165 independent reference checks, no adapter required
|Native-entry emission|Pass; final benchmark executable contains the step adapter and calls the binding helper
|Strict documentation tables and git diff --check|Pass

The final release regression run used `--exec-mode=ir` for the listed test
drivers (child processes also exercise their explicitly selected modes).
It passed 301 QUnit cases / 13159 assertions, plus the five standalone AOT
scripts. The debug subset passed 9 cases / 11276 assertions. Harness checks
are separate: 10 Python unit tests. The final builds used
`cmake --build build --target qore qcc -j3` and the corresponding `build-debug`
command. No production performance or full-suite/sanitizer claim is made.

Final measurement validation: 13 explicitly phased experiments, 464 jobs and
2320 measured samples; the final runtime contributes 98 jobs / 490 samples.
The evidence validator checked unique job keys, fork/sample counts, finite
positive timings, recomputed median-of-fork-means summaries, benchmark source
hashes, and the final runtime library hash. Committed metadata is allowlisted
and excludes inherited environments, credentials, and absolute local paths.
The final same-program callback reductions are 22.7-23.0% for IR and
59.4-60.0% for AOT; the final cross-program probe is 1.9% slower, and IR
arithmetic/list controls are 2.4% / 0.8% slower. These limitations are included
in [RESULTS.md](RESULTS.md#guarded-function-reference-calling-convention-2026-10-06),
not omitted from the performance review.

## Cached metadata and typed scalar reference ABI audit (2026-10-06)

The audit-changes skill covers the working changes after 766a5ce07 on
feature/performance. C++ runtime/compiler internals, existing semantic fixtures,
documentation and measurement evidence changed. No qlib, parser, QPP, public
class ABI or serialized descriptor changed. The sandboxing, cooperative
cancellation and documentation-table guides were applied. No agents were used.

### Registration, structure, and tests

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module; runtime change has a release note
|qore_user_module or qore_external_user_module registration|N/A|No installed module
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm changed
|Module %modern and redundant directives|N/A|No .qm changed
|No parse directives in separated .qc files|N/A|No .qc changed
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|New evidence carries a 2026 notice; modified fixtures retain theirs
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm added
|QPP namespace matches namespace constructor|N/A|No QPP class
|Test %modern directive|Pass|Both test files use %modern
|Test executable permission|Pass|Both .qr and .qtest have executable permission
|Module path precedes %requires|Pass|Wrapper prepends repository qlib before QUnit and FsUtil
|External modules use %try-module|N/A|Only Qore-delivered QUnit and FsUtil are required


### Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|No new filesystem operations
|No unchecked C++ network operations|Pass|No network operations; binding helpers attach compiler metadata, not sockets
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No C++ I/O added
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|Existing FsUtil temporary directory and quoted test subprocesses only
|C++ loops over 100 iterations check cancellation|Pass|New metadata scan has at most 16 parameters; typed lowering at most one; existing body proof bounded to 64 expression nodes
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|No deprecated check introduced; existing native/IR body polls preserved
|100 tight / 10 expensive iteration frequency|N/A|No new unbounded C++ loop
|No uncancellable blocking operation|Pass|No runtime blocking operation added; test worker wait has a 10-second timeout and normal Counter cancellation behavior

### DataProvider checklist

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
|JNI dependency JARs committed|N/A|No JNI change
|JAR install rules in CMake|N/A|No Java installation change


### Code review

|!Check|!Status|!Evidence
|No workarounds, TODOs, FIXMEs, stubs|Pass|Complete bounded scalar ABI with explicit generic fallback, not an unchecked replacement for general calls
|Exception safety|Pass|RAII preserves context/frame restoration; typed helpers initialize output on handled errors; owned boxed merge result registered before sink check; consumed arguments released after borrowed invocation
|Thread safety|Pass|Metadata and entry pointers immutable before reference publication; no cached mutable execution mode or debugger state; shared-reference worker test
|Type safety|Pass|Versioned scalar signature tags validated at attachment and invocation; exact LLVM/native representation proof; int64 boolean carrier; full-width boxed fallback preserved
|Performance|Pass|Separate metadata-only and typed pilots, final interleaved before/after controls and Java comparison, larger-batch repeat and Program-boundary control; no universal speedup claim
|Error handling|Pass|Bounds, scalar tag, variant/Program/code identity and exact signature checked; unassigned values and sparse slots retain binding errors; optional AOT EH retains merged sink check
|Public Doxygen params, returns, throws, examples, caveats|N/A|Only internal helpers; ABI/ownership/fallback caveats documented in design and declarations
|Documentation|Pass|Design, diagnostic switches, release notes, measured results and reproducible evidence updated
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP method changed
|Security|Pass|No unchecked I/O, user format strings or unchecked indices; evidence excludes inherited environments and credentials
|Correctness and edge cases|Pass|AST-referenced fixture covers scalar shapes, full-width values, NOTHING, defaults/coercion/overloads/references, ownership/COW, exceptions, methods, Program crossings, execution-mode changes and multiple threads

Audit corrections: native argument proof now requires the exact declared
scalar type as well as assigned representation. The generic branch keeps its
boxed result rather than coercing a typed declaration's possible NOTHING.
Review also caught the experimental C++ EH fallback setting a global
skip-next-sink-check marker that was invalid on the merged native edge. Both
EH-only markers are now cleared, and QORE_AOT_EH=1 was added to the fixture's
compiler matrix. The interrupted pre-correction broad timing run is excluded.

The metadata descriptor adds reference-object storage and a bounded creation
scan. Reference creation cost is not measured here. Typed calls still perform
RTTI, Program/context/diagnostic work and caller-result boxing. Invoke-form,
multi-argument and non-scalar calls retain existing dispatch; potentially
throwing bodies retain the registered normal entry. No full-suite, sanitizer
or exact allocation-count claim is made.

### Verification

|!Verification|!Result
|Release/debug qore and qcc builds|Pass; cmake --build build (and build-debug) --target qore qcc -j3
|IRFunctionRefCalls release/debug|Pass; each 1 case / 43 wrapper assertions, 100 semantic repetitions per child, four shared-reference workers; AST/IR/JIT/tiered and runtime-disabled, AOT adapter-disabled, typed-disabled, stripped-debug and C++ EH variants
|Five standalone AOT fast-entry/closure scripts|Pass; AOTKnownCallRefFastEntry, AOTGenericFunctionFastEntry, AOTGenericCrossObjectFastEntry, AOTOptionalScalarFastEntry, AOTNativeClosure
|AOTQoLinkNativeSymbols|Pass; 4 cases / 38 assertions
|IRCallFrameReuse release/debug|Pass; each 2 cases / 1200 assertions
|IRExecModeSmoke|Pass; 159 cases / 496 assertions
|IRExceptionTrace release/debug|Pass; each 4 cases / 41 assertions
|IRReturnBorrowedSlot release/debug|Pass; each 2 cases / 10002 assertions
|JitDebugStep|Pass; 1 case / 7 assertions
|StackGuardCompiled|Pass; 5 cases / 22 assertions
|Sandbox policy-barrier and sandbox-manager|Pass; 4 / 6 and 67 / 408 cases/assertions
|thread-cancel|Pass; 47 cases / 81 assertions
|test-debug|Pass; 10 cases / 847 assertions
|Harness unit tests|Pass; 10 Python tests
|Previously compiled AOT binary, new runtime|Pass; 165 independent reference checks
|All twelve typed runtime helpers emitted in fixture|Pass; nm confirms each scalar return / zero-or-scalar-argument helper
|Strict documentation tables and git diff --check|Pass

The final regression commands use --exec-mode=ir for test drivers; children
select their own modes. Release totals: 306 cases / 13191 assertions, plus five
standalone scripts. Debug subset: 9 cases / 11286 assertions. The metadata-only
and typed pilot measurements are explicitly distinguished from the final
corrected binary in results-native-calls-macos-arm64.json. See RESULTS.md for
controls, limitations, and the matched Java result.

Evidence validation passed for seven phased experiments, 258 jobs and 1290
measured samples: unique job keys, fork/sample counts, finite positive samples,
recomputed medians and final library identity. Metadata is allowlisted.
