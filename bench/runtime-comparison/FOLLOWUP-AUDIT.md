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

## Exact-reference recognition and borrowed diagnostic names audit (2026-10-06)

The audit-changes skill covers this follow-up after merge 2fd14f43d. Scope:
lib/JITRuntime.cpp, the existing function-reference fixture, design/release
notes, results and sanitized measurement evidence. No qlib source, public
class layout/vtable, serialized descriptor, compiler lowering, parser or QPP
changed in this follow-up. The sandboxing, cancellation and table guides were
applied. No agents were used.

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
|No unchecked C++ filesystem operations|Pass|No filesystem operation added
|No unchecked C++ network operations|Pass|No network operation added
|QoreSandboxManagerHelper for filesystem/network operations|N/A|No new C++ I/O
|Qore File, Dir, Socket, HTTPClient usage justified|N/A|Fixture changes add no I/O
|C++ loops over 100 iterations check cancellation|Pass|No new C++ loop
|Use qore_check_cancel, not qore_check_io_interrupt|Pass|Existing call/body cancellation behavior untouched; no deprecated API
|100 tight / 10 expensive iteration frequency|N/A|No new C++ loop
|No uncancellable blocking operation|Pass|Frame publication/pop and Program guards remain unchanged; no blocking operation added

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
|No workarounds, TODOs, FIXMEs or stubs|Pass|Exact concrete-node guard and complete owned/borrowed frame specializations; other reference kinds retain virtual dispatch
|Exception safety|Pass|Stack helper remains last member: pop/readers finish before destruction; context restoration and sink checks unchanged; no new allocation
|Thread safety|Pass|Borrowed function name is stable for the active invocation; frames initialize all fields before publication and wait for readers on pop; thread.cpp copies names into owned QoreStringNodes during snapshot construction
|Type safety|Pass|typeid equality proves the static downcast to the exact internal class; future derived nodes fall back; Name template has only owning string and const-string-reference specializations
|Performance|Pass|Saved merged baseline, interleaved process-level timing, arithmetic/polymorphic controls, count-4096 repeat, Program-boundary probe and separate original-baseline comparison
|Error handling|Pass|Program-first rejection, debugger, signature, argument, execution-mode and normal-entry guards unchanged; closure/method/foreign references retain general dispatch
|Public Doxygen params, returns, throws, examples, caveats|N/A|No public helper or method added; internal comments document lifetime and exact-type scope
|Documentation|Pass|Design and release notes document borrowed-name lifetime and unchanged diagnostics; results report measured scope and limitations
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP change
|Security|Pass|No capability check removed; exact downcast guarded; no new input indices, user-controlled format strings or credentials
|Correctness and edge cases|Pass|Long-name reference shared by four threads, captured closure with updated capture, all previous semantic cases, concurrent stack snapshots, debugger, cancellation and sandbox regressions

Borrowed-name lifetime was traced through QoreProgramStackLocationHelper and
thread.cpp's stack publication/pop and getCallStackHash implementation. Names
are copied into snapshot values before the reader releases the frame. The
active invocation and its reference outlive the frame; ordinary frames still
own synthesized names. No diagnostic frame, policy boundary or cancellation
check is elided. FunctionCallReferenceNode has no current subclasses; future
subclasses deliberately retain virtual dispatch rather than inheriting an
unchecked fast convention.

### Integration and verification

|!Verification|!Result
|Fetch origin and merge origin/develop|Pass; e88224334 merged cleanly in 2fd14f43d after d3e546088 was audited and committed
|Merged release/debug qore/qcc baseline builds|Pass; saved release library hash 84f62496296c44547caf70bdae147c577488d2fb8c1c927877e41a484181b7f0
|Merged QoreAgentStrategies qmod rebuild and tests|Pass; rebuilt changed module and dependencies; 25 cases / 104 assertions
|Merged AOTTypedForeachNothing release/debug|Pass; each 3 cases / 24 assertions
|Optimized release/debug qore/qcc builds|Pass; cmake --build build (and build-debug) --target qore qcc -j3
|IRFunctionRefCalls release/debug|Pass; each 1 case / 43 wrapper assertions, all execution modes, disabled paths, stripped debug and experimental C++ EH; updated long-name and captured-closure cases
|Five standalone AOT scripts|Pass; known reference, generic function, cross-object generic, optional scalar, native closure
|IR/AOT regression suite|Pass; AOTQoLinkNativeSymbols, IRCallFrameReuse, IRExecModeSmoke, IRExceptionTrace, IRReturnBorrowedSlot, JitDebugStep, StackGuardCompiled
|Sandbox policy barrier and sandbox-manager|Pass after rebuilding required DataProvider qmod; initial missing-artifact failure was not a runtime correctness failure
|Cancellation and debugger regressions|Pass; thread-cancel and test-debug
|Concurrent stack snapshots release/debug|Pass; each 3 cases / 7 assertions
|get-call-stack|Pass; 1 case / 7 assertions
|Pre-merge typed AOT executable on final runtime|Pass; 165 independent reference checks
|Harness unit tests|Pass; 10 tests
|Strict documentation tables and git diff --check|Pass

Successful final release cases total 338 / 13333 assertions plus five standalone
scripts; debug subset 15 / 11317. The initial sandbox run stopped on a
missing DataProvider qmod after configure invalidated stale generated links;
DataProvider-qmod was rebuilt, then sandbox and remaining tests reran and
passed. CMake's two root-owned generated packaging files were preserved under
/tmp/qore-cpack-backup.WA2Lvn and regenerated as the normal user. No tracked
source was removed. The debug subset does not test the changed qlib module.

No full-suite, sanitizer or allocation-count run was performed. The timing
kernels use short callback names; long-name behavior is regression-tested but
its separate performance benefit is not quantified. Measurements attribute
the combined recognition/name change, not independent shares to each edit.

Evidence validation passed for four phased experiments, 119 jobs and 595
measured samples: unique jobs, expected fork/sample counts, finite positive
timings, recomputed medians, benchmark source hashes and final library identity.
Metadata is allowlisted and excludes inherited environment values.

## Explicit callback execution-context audit (2026-10-07)

Pre-commit `audit-changes` review on `feature/performance` against
1501146e732f52835bcf88a872b588bf09354b88. The main agent read the complete
skill, module-structure, sandboxing and cooperative-cancellation guides.
No delegation was used. All applicable checks pass in the retained scope.

Scope: additive typed-callback helper ABI v2, an invocation-scoped live
thread-state view in RuntimeConfig's private implementation, combined native
frame/context save/restore, acquire-guarded lazy AOT readiness, regression
coverage, documentation and benchmark evidence. No parser, QPP, installed
module, qlib source, public object layout, or serialized AOT format changes.
This does not complete the runtime-wide TLS migration or expand native adapter
eligibility. Existing v1 entry points remain available.

The initial prepared-executor refactor was removed after mixed IR results and
slower polymorphic controls. The retained view is appended to the private
implementation, preserving the existing fields' offsets. The narrowed change
was rebuilt and all listed release/debug tests rerun. The experiment does not
attribute the initial control shift to either edit individually. The larger
final run also had a slower IR polymorphic control; a longer, differently
shuffled repeat did not reproduce it. No IR speedup or universal no-regression
claim is made. See RESULTS.md for every retained and rejected measurement.

### Registration, structure, and tests

|!Check|!Status|!Evidence
|New module entry in 120_modules.dox.tmpl|N/A|No .qm module added
|New module release-note entry|N/A|No .qm module; existing runtime optimization note updated
|qore_user_module or qore_external_user_module registration|N/A|No installed module added
|CMake QMOD list registration|N/A|No QMOD added
|First lowercase module intro section|N/A|No .qm file
|Module %modern and redundant directives|N/A|No .qm file
|No parse directives in separated .qc files|N/A|No .qc file
|No deprecated %include usage|Pass|No added source uses %include
|Copyright 2026 on new files|Pass|New evidence JSON has a 2026 notice and MIT identifier; modified code/tests retain notices
|Multi-file module directory layout|N/A|No module added
|No duplicate top-level .qm entry point|N/A|No .qm file
|QPP namespace matches namespace constructor|N/A|No QPP class
|Test %modern directive|Pass|Both modified IRFunctionRefCalls files use %modern
|Test executable permission|Pass|Both modified test files retain mode 100755
|Module path precedes %requires|Pass|Wrapper prepends in-repo qlib before QUnit and FsUtil; fixture needs no modules
|External modules use %try-module|N/A|Only Qore-delivered QUnit and FsUtil are required

### Sandboxing and cancellation

|!Check|!Status|!Evidence
|No unchecked C++ filesystem operations|Pass|No filesystem access added
|No unchecked C++ network operations|Pass|No network access added
|QoreSandboxManagerHelper around filesystem/network operations|N/A|No such operation added
|Qore File, Dir, Socket, HTTPClient usage justified|Pass|No new I/O in fixture; existing trusted wrapper uses quoted subprocess arguments and FsUtil temporary directories
|C++ loops over 100 iterations check cancellation|Pass|No C++ loop added; the prepared-executor refactor is absent
|Use qore_check_cancel, not deprecated qore_check_io_interrupt|Pass|No cancellation implementation or call site changed
|Cancellation frequency of 100 tight or 10 expensive iterations|N/A|No new C++ loop; Qore fixture loops retain runtime polling
|No uncancellable blocking operation|Pass|No new runtime blocking operation; existing stack-walker handshake is preserved; fixture counter waits have 10-second bounds

Program ownership, current Program and debugger checks still run for opaque
references. The helper reads live ThreadData through the acquired view, not a
RuntimeConfig Program snapshot that a nested compatibility call can refresh.
It preserves call_program_context separately from current_pgm, applies the
normal runtime parse-option override on entry and restoration, and restores
closure environment, frame boundaries, source location and innermost-frame
marker. The same stack publication and reader-completion handshake is used.
No sandbox resolution, policy barrier, cancellation point, or stack guard is
removed. Policy-barrier, sandbox, cancellation and debugger tests pass.

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
|No workarounds, TODOs, FIXMEs or stubs|Pass|Complete bounded v2 helper path; rejected prepared executor removed, not left disabled in production
|Exception safety|Pass|Result initialized before attempted call; materialization and frame setup checked through xsink; QoreJITException caught with RAII restoration; argument/result ownership remains in existing caller lowering
|Thread safety|Pass|Native view is thread-local and reacquired per invocation; no reference/function stores it; diagnostic fields initialized before release publication; last-member helper waits for readers before destruction
|Type safety|Pass|Existing exact-node, scalar signature and normal-entry identity guards preserved; new callbacks use code<int(int)>; no C-style cast added
|Performance|Pass|AOT callback benefit repeats in interleaved measurements; controls, rejected prototype and mixed IR movements retained without an IR improvement claim
|Error handling|Pass|Generic fallback remains for foreign Programs, debugger attachment, closures/methods and incompatible values; lazy preparation still happens at invocation, not unused-reference construction
|Public Doxygen params, returns, throws, examples, caveats|N/A|No public method added; RuntimeConfig gains only an internal friend declaration
|Documentation and release notes|Pass|Internal helper ABI, live-view lifetime, acquisition point, compatibility, disabled path and remaining TLS/boxing limitations documented; existing release note updated
|QPP CONSTANT and RET_VALUE_ONLY flags|N/A|No QPP change
|Security|Pass|No user-controlled formats, unchecked new indexing or credentials; borrowed state is confined to native invocations on its acquiring thread
|Correctness and edge cases|Pass|All scalar signature combinations, wide integers, NOTHING, sparse slots, wrong types, defaults, overloads, references, ownership, captures, recursion, foreign Programs, disabled paths and concurrent stack inspection covered

The stack helper is the final member of a fully initialized diagnostic frame;
its output caller location is assigned before publication. AOT context and
PC-map readiness use the existing acquire/release protocol. The slow initial
materialization path still establishes Program-call context. The view is
acquired in the generated logical entry after the stack guard, not before it,
and reset in the lowerer's per-function state. Native entries and output
boxing retain v1 semantics; only the surrounding helper ABI changes.

Test development found that asserting nonempty names/positive eager locations
for every AOT frame was invalid on the saved baseline too. The concurrent
fixture therefore validates its known named frames and allows builtin eager
locations, while the existing exact exception-line test remains unchanged.
It does not require probabilistic sampling of the short native leaf.

### Verification

Final commands used `DYLD_LIBRARY_PATH` and `QORE_LIBDIR` pointing at the
corresponding build, with `build/qore --exec-mode=ir TEST` (or build-debug).
No qlib source changed, so no qmod rebuild was required.

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|IRFunctionRefCalls release/debug|Pass; each 1 case / 48 wrapper assertions plus the child semantic matrix; AST/IR/JIT/tiered, disabled fast paths, v1/v2 AOT helpers, stripped source/debug and experimental EH
|IR/AOT release regressions|Pass; AOTQoLinkNativeSymbols, IRCallFrameReuse, IRExecModeSmoke, IRExceptionTrace, IRReturnBorrowedSlot, JitDebugStep, StackGuardCompiled, AOTRuntimeLocationCache, AOTLazyFunctionContext
|Policy, cancellation, debugger and stack tests|Pass; policy-barrier, sandbox-manager, thread-cancel, test-debug, concurrent-call-stacks, get-call-stack
|Debug subset|Pass; IRFunctionRefCalls, IRCallFrameReuse, IRReturnBorrowedSlot, StackGuardCompiled, concurrent-call-stacks
|Pre-change AOT executable on final runtime|Pass; all 165 reference records byte-identical to saved baseline output
|Harness unit tests|Pass; 10 tests
|Generated native code inspection|Pass; callback loop calls v2 helper and acquires RuntimeConfig once after its entry stack guard
|Benchmark evidence integrity|Pass; seven experiments, 289 jobs / 1445 measured samples; unique jobs, sample counts, finite values and medians recomputed; runtime/library identities recorded; metadata excludes inherited environment and absolute user paths
|Program-boundary controls|Pass; 30 reference checks per mode; slower overlapping medians reported without an IR benefit claim
|Separate sampling|Pass; five-second profile confirms v2 dispatch and remaining frame/stack-guard work; qualitative only
|Strict documentation tables and git diff --check|Pass

Release: 311 QUnit cases / 13223 assertions plus the standalone AOT location
script. Debug: 13 cases / 11279 assertions. Fixture-internal expectations and
benchmark reference checks are additional. No full-suite, sanitizer or
allocation-count run was performed. Timing, compilation, regression tests and
sampling were separate; ordinary workstation activity was not eliminated.

## Develop integration audit (2026-10-07)

Applied audit-changes before merging origin/develop at 0122225d0 into
feature/performance at 575f3b9dd. The merge is conflict-free and contains only
two upstream DPQL Unicode fixes (five files). Runtime sources are unchanged.
Read the module-structure, sandboxing, cancellation and both DataProvider guides.
No delegation or external-service tests were used.

Rebuilt with cmake --build build --target qore qcc DataProvider-qmod -j3.
Dpql.qtest passes 55 cases / 2785 assertions; it retains its existing explicit
source requirements. A separate normal-module-path Unicode smoke passed after
the qmod rebuild. IRFunctionRefCalls passes 1 case / 48 wrapper assertions.
No full-suite or DPQL performance claim is made. git diff --cached --check passes.

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|No %include usage (deprecated for modules)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|Copyright 2026 on all new files|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module or QPP class; existing DataProvider module registration and layout unchanged.
|%modern directive present|Pass|Dpql.qtest retains %modern, mode 100755 and prepended qlib path before its Qore-delivered requirements; no external dependency added.
|Executable permission set (chmod +x)|Pass|Dpql.qtest retains %modern, mode 100755 and prepended qlib path before its Qore-delivered requirements; no external dependency added.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Dpql.qtest retains %modern, mode 100755 and prepended qlib path before its Qore-delivered requirements; no external dependency added.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Dpql.qtest retains %modern, mode 100755 and prepended qlib path before its Qore-delivered requirements; no external dependency added.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Only Unicode tokenization, serialization, completion and in-memory tests changed; no new filesystem or network operations.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|Only Unicode tokenization, serialization, completion and in-memory tests changed; no new filesystem or network operations.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|Pass|Only Unicode tokenization, serialization, completion and in-memory tests changed; no new filesystem or network operations.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Only Unicode tokenization, serialization, completion and in-memory tests changed; no new filesystem or network operations.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|N/A|No C++ changes or blocking operations; Qore scan loops retain runtime cancellation polling.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|N/A|No C++ changes or blocking operations; Qore scan loops retain runtime cancellation polling.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No C++ changes or blocking operations; Qore scan loops retain runtime cancellation polling.
|No blocking operations without cancellation support|N/A|No C++ changes or blocking operations; Qore scan loops retain runtime cancellation polling.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Response/output types use private Fields|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Password/secret fields have "sensitive": True|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|No bare field/option names in prose — must use backticks|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|DPQL utilities only: no action, app, provider schema, factory, catalog, credentials or JNI dependency change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Reviewed the five incoming files; 55 DPQL cases / 2785 assertions, qmod Unicode smoke and 48 callback assertions pass.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Qore-managed values; malformed Unicode has an explicit error/replacement path. No C++ allocations.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|New tokenizer byte buffer is instance state; completion helpers use call-local immutable inputs.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Reviewed the five incoming files; 55 DPQL cases / 2785 assertions, qmod Unicode smoke and 48 callback assertions pass.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Byte-offset scans advance by whole UTF-8 characters; no global cache or new nested input scan. No DPQL performance improvement claim.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Reviewed the five incoming files; 55 DPQL cases / 2785 assertions, qmod Unicode smoke and 48 callback assertions pass.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Public isBareFieldName has param/return documentation, examples and version; module documents Unicode names and diagnostic columns.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Reviewed the five incoming files; 55 DPQL cases / 2785 assertions, qmod Unicode smoke and 48 callback assertions pass.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Reviewed the five incoming files; 55 DPQL cases / 2785 assertions, qmod Unicode smoke and 48 callback assertions pass.

## Native leaf-frame audit (2026-10-07)

Applied audit-changes to the optimization after merge 089ab8e44. Read the full
skill and its module-structure, sandboxing and cooperative-cancellation guides;
the integration audit above also covers the two DataProvider guides. No agents
were delegated work.

Scope: specialized empty local/closure-frame bookkeeping for the existing v2
typed scalar callback path, the recursive mixed-frame regression, documentation
and sanitized measurements. No new native eligibility, helper ABI, serialized
format, public layout, module, parser or qlib source change.

### Design and correctness review

The local stack retains its ordinary frame marker and frame count but omits
the general cleanup-vector record. Its paired pop asserts that the top slot
is that unassigned marker and directly restores the position/count. Closure
pop likewise removes only the proven-empty top marker. Stack-block allocation
and reuse still use the existing push routines; a pop at the first slot can
leave an empty current block for ordinary stack unwinding, as before.

This is justified by emitFunctionRefNativeEntry's existing whole-body scalar
expression and context-independence proof, not by assuming every same-Program
function is a leaf. General/throwing/context-dependent callbacks retain the
existing fallback. RuntimeConfigNativeCallHelper is used only by the v2
typed-reference helper. Admission still checks Program, debugger, exact node,
signature and native-entry identity; ordinary v1 entries remain unchanged.

Frame inspection scans the actual local/closure markers, not the private
cleanup vector. Markers and counts therefore remain observable. The diagnostic
frame is unpublished and the existing reader handshake completes before either
marker is popped. Location, parse-option override, call Program, closure
environment, stack guards and cancellation semantics remain unchanged.
No Qore values are owned by the empty markers, so skipping general variable
destruction loses no ownership operation; debug assertions validate this.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No module or QPP class added; no module registration/layout change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No module or QPP class added; no module registration/layout change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No module or QPP class added; no module registration/layout change.
|Module added to QMOD list in CMakeLists.txt|N/A|No module or QPP class added; no module registration/layout change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No module or QPP class added; no module registration/layout change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No module or QPP class added; no module registration/layout change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No module or QPP class added; no module registration/layout change.
|No %include usage (deprecated for modules)|N/A|No module or QPP class added; no module registration/layout change.
|Copyright 2026 on all new files|Pass|New sanitized evidence JSON has 2026 copyright and MIT identifier; no other file added.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No module or QPP class added; no module registration/layout change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No module or QPP class added; no module registration/layout change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No module or QPP class added; no module registration/layout change.
|%modern directive present|Pass|IRFunctionRefCalls.qr retains %modern and mode 100755; no dependencies added. Existing wrapper prepends qlib before QUnit/FsUtil.
|Executable permission set (chmod +x)|Pass|IRFunctionRefCalls.qr retains %modern and mode 100755; no dependencies added. Existing wrapper prepends qlib before QUnit/FsUtil.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|IRFunctionRefCalls.qr retains %modern and mode 100755; no dependencies added. Existing wrapper prepends qlib before QUnit/FsUtil.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|IRFunctionRefCalls.qr retains %modern and mode 100755; no dependencies added. Existing wrapper prepends qlib before QUnit/FsUtil.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem/network operations added; runtime admission, sandbox policy and current Program checks unchanged.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No filesystem/network operations added; runtime admission, sandbox policy and current Program checks unchanged.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|Pass|No filesystem/network operations added; runtime admission, sandbox policy and current Program checks unchanged.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|No filesystem/network operations added; runtime admission, sandbox policy and current Program checks unchanged.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop or blocking operation added. Existing cancellation and stack-reader handshake remain unchanged.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No C++ loop or blocking operation added. Existing cancellation and stack-reader handshake remain unchanged.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No C++ loop or blocking operation added. Existing cancellation and stack-reader handshake remain unchanged.
|No blocking operations without cancellation support|Pass|No C++ loop or blocking operation added. Existing cancellation and stack-reader handshake remain unchanged.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Response/output types use private Fields|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Password/secret fields have "sensitive": True|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider changes in this optimization; upstream DPQL merge audited separately above.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete specialized frame pair; no TODO, FIXME, stub, or expanded eligibility.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Existing RAII helper restores context on normal/exception exit; marker slots are unassigned and own no Qore values; ordinary residual-variable cleanup remains unchanged.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|TLS stacks remain owning-thread state; diagnostic publication and reader-completion handshake precede marker destruction. No new shared mutable state.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Compiler admission retains exact scalar signature/whole-body proof; test callback parameters use code<int(int)> and code<int()>.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Removes local marker-vector push/pop and general empty-frame cleanup; constant-time leaf pop. Matched before/after and controls recorded in RESULTS.md.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing current-Program/debugger/entry-identity guards and generic fallback unchanged; debug assertions verify empty top markers.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Only internal methods added; document pairing, compiler proof, empty-slot invariant and observable frame preservation; design/README/release note updated.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new untrusted format string, indexing input, I/O or credentials; stack indices derive from established push/pop invariant, asserted in debug.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Baseline and optimized semantic matrix agree, including 140-level recursion, outer captures, ownership and unwind across TLS blocks; broader tests listed below.

### Verification

|!Verification|!Result
|Release and debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Extended IRFunctionRefCalls fixture on saved baseline|Pass; 48 wrapper assertions, all execution modes and disabled/AOT configurations
|Extended IRFunctionRefCalls release/debug|Pass; 48 wrapper assertions each; 140-level recursion, outer capture preservation, object destruction, exception unwinding and native callbacks during cleanup
|IR/AOT regression matrix|Pass; AOTQoLinkNativeSymbols, IRCallFrameReuse, IRExecModeSmoke, IRExceptionTrace, IRReturnBorrowedSlot, JitDebugStep, StackGuardCompiled, AOTRuntimeLocationCache, AOTLazyFunctionContext, AOTRecursiveClosureFrame
|Policy and runtime integration|Pass; policy-barrier, sandbox-manager, thread-cancel, test-debug, concurrent-call-stacks, get-call-stack, Dpql
|Debug subset|Pass; IRFunctionRefCalls, IRCallFrameReuse, IRReturnBorrowedSlot, StackGuardCompiled, concurrent-call-stacks, AOTRecursiveClosureFrame
|Old AOT binary on final runtime|Pass; all 165 reference outputs byte-identical to saved pre-change result
|Harness unit tests|Pass; python3 bench/runtime-comparison/test_runner.py, 10 tests
|Benchmark integrity|Pass; five experiments, 178 unique jobs / 890 measured samples; counts, finite values and medians recomputed, source/library hashes recorded, environment and absolute user paths excluded
|Separate sampling|Pass; five seconds at 1 ms; general variable-frame cleanup no longer sampled in native callback stack, while scalar coercion/type resolution and context/guard work remain
|Strict documentation tables and diff whitespace|Pass; qpp --table-strict and git diff --check

Release: 367 QUnit cases / 16011 assertions plus the standalone AOT location
script. Debug: 14 cases / 11282 assertions. All test commands use the matching
build via DYLD_LIBRARY_PATH and QORE_LIBDIR, and --exec-mode=ir at the wrapper.
No qlib source changed after integration. Builds, tests and final sampling were
separate from benchmark timing. No full-suite, sanitizer or allocation-count
claim is made.

### Separate pre-existing JIT finding

An initial fixture created a fresh captured local in each recursive frame.
It fails in JIT mode on the saved, unmodified merged runtime and on the final
runtime: depth 1 reads captured value 0 after returning from recursion. The
same reproducer passes AST and IR. This does not exercise the new native-leaf
path in JIT mode and is not fixed by this optimization. The final regression
uses a capture owned by the outer caller, while still exercising recursive
ordinary frames, native callbacks and exception cleanup across stack blocks.

The complete diagnostic reproducer and its hash are retained in
results-native-leaf-macos-arm64.json under baseline_finding.reproducer_source.
Run it with --exec-mode=jit and QORE_JIT_SYNC_COMPILE=1. This finding should be
isolated and fixed separately before expanding captured/native call eligibility;
it is not counted as a passing regression or hidden by the test totals above.

## Native scalar assignment audit (2026-10-07)

Applied audit-changes to the increment based on 1e974dc50. Read the full
skill, module-structure, sandboxing and cooperative-cancellation guides.
No DataProvider files changed, so its conditional guides are not applicable.
No agents were delegated work.

Scope: lib/QoreIRToLLVM.cpp, new IRNativeScalarAssignment fixture/wrapper,
design and benchmark documentation, and sanitized measurement evidence.
No runtime helper, ABI, serialized metadata, parser, module or qlib source
change.

The guard proves only the actual immediate value representation and exact
destination type. It does not strengthen a callable's return contract.
Wide ints, heap floats, NOTHING, incompatible values and non-strict
destinations still take the original fallible assignment check. The slow
result is converted only after its exception check; the native PHI is not
entered on error. Cleanup slots remain initialized and registered, and their
previous values are cleared before branching. LLVM owns generated instructions.
Release/debug LLVM verification and the O0/O3 tests validate the new CFG.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, module registration or layout change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, module registration or layout change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, module registration or layout change.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, module registration or layout change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, module registration or layout change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, module registration or layout change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, module registration or layout change.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, module registration or layout change.
|Copyright 2026 on all new files|Pass|New fixture, wrapper and JSON evidence carry 2026 copyright and MIT identification.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, module registration or layout change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, module registration or layout change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, module registration or layout change.
|%modern directive present|Pass|New fixture/wrapper are executable and use %modern; wrapper prepends repository qlib before QUnit/FsUtil. No external module dependency.
|Executable permission set (chmod +x)|Pass|New fixture/wrapper are executable and use %modern; wrapper prepends repository qlib before QUnit/FsUtil. No external module dependency.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|New fixture/wrapper are executable and use %modern; wrapper prepends repository qlib before QUnit/FsUtil. No external module dependency.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|New fixture/wrapper are executable and use %modern; wrapper prepends repository qlib before QUnit/FsUtil. No external module dependency.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No C++ filesystem/network operation added. Test subprocess/temp-directory use is harness-only; sandbox-manager regressions pass.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ filesystem/network operation added. Test subprocess/temp-directory use is harness-only; sandbox-manager regressions pass.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|Pass|No C++ filesystem/network operation added. Test subprocess/temp-directory use is harness-only; sandbox-manager regressions pass.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|No C++ filesystem/network operation added. Test subprocess/temp-directory use is harness-only; sandbox-manager regressions pass.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop or blocking operation added; generated loop cancellation and call stack checks unchanged. Thread-cancel regressions pass.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No C++ loop or blocking operation added; generated loop cancellation and call stack checks unchanged. Thread-cancel regressions pass.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No C++ loop or blocking operation added; generated loop cancellation and call stack checks unchanged. Thread-cancel regressions pass.
|No blocking operations without cancellation support|Pass|No C++ loop or blocking operation added; generated loop cancellation and call stack checks unchanged. Thread-cancel regressions pass.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI module changes.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI module changes.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI module changes.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI module changes.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI module changes.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI module changes.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI module changes.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI module changes.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI module changes.
|Response/output types use private Fields|N/A|No DataProvider or JNI module changes.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI module changes.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI module changes.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI module changes.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI module changes.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI module changes.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI module changes.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI module changes.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI module changes.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI module changes.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI module changes.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI module changes.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI module changes.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI module changes.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI module changes.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI module changes.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI module changes.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI module changes.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI module changes.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete guarded scalar lowering; no TODO, FIXME, stub, ABI or callable-admission expansion.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Immediate edges allocate nothing. Slow coercion retains initialized, registered cleanup slots and exception checks before conversion; prior slot ownership is released before either edge. No conversion is cached under the original SSA id.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only function-local LLVM lowering state changes; no shared runtime metadata or synchronization changes.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only pointer-identical strict int/float/bool destination types admit immediate guards. Test callback uses code<auto(auto)>; weak, soft, timeout and heap representations keep coercion.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-size generated guard replaces type-path lookup/coercion on matching values. Matched AOT callback reductions are 44.4% and 45.6%; all samples retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|NOTHING and wrong actual types cannot take the guard. Full slow assignment validation and error propagation remain; tests cover errors and later reuse.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal lambda comments explain the native result and non-cached conversion; design and benchmark README document scope, fallback and disable switch. No public API added.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new I/O, dynamic format string or unchecked indexing. Evidence excludes inherited environment and absolute user paths; owned opaque references are not treated as immediate scalars.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|AST/baseline semantic parity, release/debug mode matrices, wide integers, float tag collisions/NaNs, soft and timeout conversion, owned-object destruction and mixed successful/throwing stores pass.

### Verification

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Final new fixture on saved baseline|Pass; AST/IR/JIT/tiered and source-stripped AOT O0/O3
|IRNativeScalarAssignment release/debug|Pass; 46 assertions each, AST/IR/JIT/tiered, disabled guard, AOT O0/O3 with and without EH; emitted guards and retained slow coercion checked
|IRFunctionRefCalls release/debug|Pass; 48 assertions each, including foreign Programs, fallback binding, native frame inspection, recursive mixed-frame cleanup and exceptions
|Scalar/ownership regressions release/debug|Pass; IRNativeLocalSSA and AOTReturnTypeCoercion standalone scripts, NanBoxDoubleTagCollision, IRCallFrameReuse, IRReturnBorrowedSlot
|Additional release regressions|Pass; IRExceptionTrace, IRExecModeSmoke, StackGuardCompiled, AOTQoLinkNativeSymbols, JitDebugStep, sandbox-manager, thread-cancel, concurrent-call-stacks, IRWeakReferenceLocalStore, AOTOpaqueReferenceOwnership
|Old AOT binary with new runtime|Pass; 165 reference records byte-identical to baseline output
|Harness tests|Pass; 10 Python unit tests
|Measurement integrity|Pass; three experiments, 138 unique jobs / 690 samples, finite positive values and medians independently recomputed; source/library hashes retained
|Separate code inspection/sampling|Pass; emitted benchmark guard verified; five-second sample identifies remaining frame, recognition, cleanup and cancellation costs
|Strict documentation tables/whitespace|Pass; qpp --table-strict and git diff --check

Final selected suite totals: 321 release cases / 12475 assertions; 22 debug
cases / 11315 assertions, plus two standalone scripts per build. Repeated
development runs are not double-counted. Linux-only resident-memory checks in
NanBoxDoubleTagCollision are explicitly skipped on macOS; its value tests pass.
No full-suite, sanitizer or allocation-count claim. The previously documented
recursive JIT capture finding remains out of scope and is not a passing test.
Builds, regressions and profiling were separate from timing.

Audit led to explicit code-generation checks in the new regression and
conservative exclusion of non-strict destination types and owned heap values.

## Native callback cleanup audit (2026-10-07)

Scope: uncommitted C++ lowering/PC-map recovery, four test files, benchmark
evidence and documentation relative to `ebe191d30`. The audit-changes skill
and the module-structure, sandboxing and cooperative-cancellation guides
were applied by the main agent. No DataProvider files changed.

The final optimization is AOT-only and restricted to generated functions
containing typed native callback helpers. Guards precede the same cleanup
calls; no callee admission, signature, frame, debugger, cancellation-frequency
or runtime ABI contract changes. The first broad implementation exposed a
pre-existing Mach-O split-codegen location failure, reproduced on the saved
baseline and fixed by recovering maps before the partial link.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, module registration or layout change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, module registration or layout change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, module registration or layout change.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, module registration or layout change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, module registration or layout change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, module registration or layout change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, module registration or layout change.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, module registration or layout change.
|Copyright 2026 on all new files|Pass|New JSON evidence carries 2026 copyright and MIT identification; modified fixtures retain their headers.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, module registration or layout change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, module registration or layout change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, module registration or layout change.
|%modern directive present|Pass|All modified wrappers/fixture use %modern and are executable; wrappers prepend repository qlib before QUnit/FsUtil. No external dependency added.
|Executable permission set (chmod +x)|Pass|All modified wrappers/fixture use %modern and are executable; wrappers prepend repository qlib before QUnit/FsUtil. No external dependency added.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|All modified wrappers/fixture use %modern and are executable; wrappers prepend repository qlib before QUnit/FsUtil. No external dependency added.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|All modified wrappers/fixture use %modern and are executable; wrappers prepend repository qlib before QUnit/FsUtil. No external dependency added.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Compiler-internal DWARF recovery reuses the existing reader on generated partition paths; no new public filesystem operation or policy bypass. Existing output/link handling unchanged.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No network operation added.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|Pass|Compiler-internal DWARF recovery reuses the existing reader on generated partition paths; no new public filesystem operation or policy bypass. Existing output/link handling unchanged.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Temporary directories, source writes, subprocesses and unlink are confined to compiler-test fixtures; library-directory selection is explicitly quoted.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|Tight LLVM/DWARF scans poll every 100 items; CFG rewrites, simplification and partition/CU work poll every 10. Cancellation propagates; generated runtime polling frequency unchanged. Binary search is bounded by size_t width. No new blocking runtime operation.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Tight LLVM/DWARF scans poll every 100 items; CFG rewrites, simplification and partition/CU work poll every 10. Cancellation propagates; generated runtime polling frequency unchanged. Binary search is bounded by size_t width. No new blocking runtime operation.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Tight LLVM/DWARF scans poll every 100 items; CFG rewrites, simplification and partition/CU work poll every 10. Cancellation propagates; generated runtime polling frequency unchanged. Binary search is bounded by size_t width. No new blocking runtime operation.
|No blocking operations without cancellation support|Pass|Tight LLVM/DWARF scans poll every 100 items; CFG rewrites, simplification and partition/CU work poll every 10. Cancellation propagates; generated runtime polling frequency unchanged. Binary search is bounded by size_t width. No new blocking runtime operation.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI module changes.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI module changes.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI module changes.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI module changes.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI module changes.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI module changes.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI module changes.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI module changes.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI module changes.
|Response/output types use private Fields|N/A|No DataProvider or JNI module changes.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI module changes.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI module changes.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI module changes.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI module changes.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI module changes.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI module changes.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI module changes.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI module changes.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI module changes.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI module changes.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI module changes.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI module changes.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI module changes.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI module changes.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI module changes.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI module changes.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI module changes.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI module changes.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|No TODO/FIXME/stub or weakened callable eligibility. Broad early/late variants were rejected after measuring code growth and control regressions.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|LLVM owns instructions; temporary vectors and DWARF objects use RAII. Only proven non-owning scalar representations bypass release; heap/opaque values preserve original calls, sinks and ordering. Cancellation returns through existing cleanup.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only compiler-local LLVM state changes. Partition PC maps accumulate on the main compiler thread after worker joins; no shared runtime state, frame publication or synchronization change.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Exact scalar bit patterns, not a generic non-pointer test. Musttail and invoke-form releases are excluded. Typed callback fixture uses code<int(int)>; no reinterpretation of runtime ABI or new result contract.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Linear LLVM scan and reverse-order block splitting avoid repeated long suffix moves. Only typed callback callers are transformed, after the full pipeline; final callback reductions are 21.7%/21.3%, with 2.5% native-text growth and restored direct-call controls. Source-map indexing is linear per partition plus sorting; partition count is explicit compile parallelism.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing slow ownership and coercion paths retained, including NOTHING, mismatched types, heap numbers and opaque values. LLVM module verification runs after transformations; cancellation is an error, not successful partial compilation.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal API parameter/return comments, pipeline-placement rationale, disable switch and compiler statistics documented. Design/README/results explain AOT-only scope, rejected variants and tradeoffs. No new public language API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled format strings or new unchecked runtime indexes. Evidence excludes environments and absolute user paths. Object, closure/reference and plugin tags cannot pass the scalar guard.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|AST/baseline parity, release/debug mode matrices, O0/O3, disabled paths, EH, ownership/destructor, source-line, stack, sandbox and cancellation regressions pass. Known pre-existing nested-location and recursive-capture findings are recorded, not treated as passes.

### Verification

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3, LLVM 22.1.8 on macOS arm64
|Strengthened fixture on saved baseline|Pass; AST/IR/JIT/tiered and stripped-source AOT O0/O3
|IRNativeScalarAssignment release/debug|Pass; 78 assertions each, including nonzero generated guard counts, disabled guard, O0/O3 and EH
|IRFunctionRefCalls release/debug|Pass; 58 assertions each, including forced split with and without cleanup guards
|Forced-split baseline counterexample|Baseline fails assertion 51 with line 30 instead of 33; final passes all 58 assertions
|AOTParallelCodegen release/debug|Pass; 4 cases / 20 assertions each, O0/O3 single/split stripped locations, module loading and symbol checks
|Ownership and exception regressions release/debug|Pass; NanBoxDoubleTagCollision, IRCallFrameReuse, IRReturnBorrowedSlot, IRPerformanceSemantics, IRExceptionTrace, IRNativeLocalSSA, AOTReturnTypeCoercion, AOTRuntimeLocationCache
|Additional release regressions|Pass; IRExecModeSmoke, StackGuardCompiled, AOTQoLinkNativeSymbols, AOTGenericCrossObjectFastEntry, AOTLazyFunctionContext, JitDebugStep, IRWeakReferenceLocalStore, AOTOpaqueReferenceOwnership, sandbox-manager, thread-cancel, concurrent-call-stacks
|Old AOT artifact / new runtime|Pass; all 165 reference records byte-identical
|Harness tests|Pass; 10 Python unit tests
|Measurement integrity|Pass; five experiments, 357 unique jobs / 1785 finite positive samples, medians independently recomputed; rejected variants retained
|Size/control checks|Pass; final text 49856 to 51112 bytes; executable size effectively unchanged; direct-call and dispatch-control ranges overlap in both final runs
|Separate sampling|Five seconds at 1 ms; cleanup no longer prominent, native frame/validation/stack/cancellation work remains
|Strict documentation/whitespace|Pass; qpp --table-strict on all four changed prose documents and git diff --check

Selected final totals: **330 release cases / 12616 assertions**, plus four
standalone scripts; **34 debug cases / 11484 assertions**, plus three
standalone scripts. Repeated development runs are not double-counted.
NanBoxDoubleTagCollision's Linux-only resident-memory checks are skipped on
macOS; value checks pass. No full-suite, sanitizer, Linux-run or Qore allocation
count claim. Builds, tests and profiling were separate from timing.

The recursive JIT capture finding recorded earlier remains out of scope.
A second baseline finding is preserved with a complete reproducer in
[the evidence](results-callback-cleanup-macos-arm64.json): nested direct-call
exceptions report lines 4/7/10 under baseline and final AOT O0/O3 instead of
AST's 4/4/4. This is separate from the fixed parallel-map failure and is not
claimed fixed.

Audit-driven changes include cancellation propagation through expanded
compiler scans, nonempty generated-guard assertions and explicit baseline
counterexamples. Measurement-driven changes moved the pass after the full
pipeline and restricted it to typed callback callers; neither rejected broad
variant is enabled in the committed implementation.

## Loop runtime context audit (2026-10-07)

Scope: six C++ runtime/lowering files, two new Qore fixtures, the extended native
interrupt fixture, evidence and documentation relative to `51b9d37c5`.
The main agent applied audit-changes and the module-structure, sandboxing and
cooperative-cancellation design guides. No DataProvider files changed.

The optimization changes how a generated poll obtains thread state, not what
it checks. The nullable borrowed context is acquired once after the stack guard;
the no-context path still obtains current TLS state. Thread cancellation,
scope checks, deferral, global seq_cst pending count and manager precedence are
unchanged. Cold exception construction and manager lookup are shared/outlined.
Versioned C symbols are additive; old compiled artifacts remain supported.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new Qore module, QPP class, registration or layout change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new Qore module, QPP class, registration or layout change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new Qore module, QPP class, registration or layout change.
|Module added to QMOD list in CMakeLists.txt|N/A|No new Qore module, QPP class, registration or layout change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new Qore module, QPP class, registration or layout change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new Qore module, QPP class, registration or layout change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new Qore module, QPP class, registration or layout change.
|No %include usage (deprecated for modules)|N/A|No new Qore module, QPP class, registration or layout change.
|Copyright 2026 on all new files|Pass|New fixtures and JSON evidence carry 2026 copyright and MIT identification.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new Qore module, QPP class, registration or layout change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new Qore module, QPP class, registration or layout change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new Qore module, QPP class, registration or layout change.
|%modern directive present|Pass|Both new Qore fixtures use %modern; no redundant directives.
|Executable permission set (chmod +x)|Pass|Both new fixtures are executable (100755).
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|New wrapper prepends repository qlib before QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only core Qore-delivered modules are required; no external dependency.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new C++ filesystem operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new runtime filesystem/network operation requiring a sandbox helper.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Only compiler-test temporary directories and quoted subprocess commands; no new runtime Qore I/O.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new unbounded C++ loop; three-path test loop is bounded. Existing scope walk is unchanged. Generated ordinary/fused loops retain their existing cancellation sites and frequencies.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Legacy and explicit-context checks share qore_check_cancel_intern; no new use of deprecated qore_check_io_interrupt.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No polling-frequency reduction; language loops still poll each iteration. No new expensive compiler scan.
|No blocking operations without cancellation support|Pass|No new blocking runtime operation; test workers are joined and Qore waits have 30-second bounds.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI module change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI module change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI module change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI module change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI module change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI module change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI module change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI module change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI module change.
|Response/output types use private Fields|N/A|No DataProvider or JNI module change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI module change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI module change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI module change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI module change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI module change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI module change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI module change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI module change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI module change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI module change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI module change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI module change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI module change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI module change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI module change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI module change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI module change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI module change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|No TODO/FIXME/stub, skipped policy check or cached pending-state approximation. Null context is a supported teardown path, not a workaround.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Exception construction retains ReferenceHolder for cancellation reason, ownership transfer and sink behavior. Compiler invokes its existing exception branch after every poll; generated RC acquisition is non-owning.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Borrowed context is per-thread/per-invocation; live ThreadData is not a copied Program snapshot. No shared mutable cache. All cancellation atomics and scope/deferral ordering are unchanged.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Nullable RuntimeConfig pointer has explicit lifetime contract; generated helper signature matches exported C ABI. No public layout change or cast-based access to thread internals.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time acquisition once per generated invocation; no-pending polls avoid repeated TLS lookup and large cold-path frames. Direct-loop gain 35.9% and callback gain 6.8%, repeated at wider counts. Allocation-heavy variance and initial non-reproducing regression retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Null acquisition avoids destroyed TLS context and invokes the fresh legacy check. Pending requests still resolve governing manager/scope; sinkless and deferred behavior, thread priority and clear/re-request tested.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal helpers document nullable borrowed lifetime, parameters and return contract. README/design/results explain additive ABI, disable switch, unchanged semantics and remaining TLS. No new public language API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new user-controlled format string or unchecked array access; existing tid validation retained. Shell arguments quoted. Evidence allowlists metadata and excludes inherited environments/absolute user paths.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|New mode/disabled/EH fixture and native context tests pass release/debug; baseline fixture and old-AOT checks pass. Broader selected regressions pass subject to explicitly recorded pre-existing warning-wrapper and older child-library-selection limitations.

### Verification

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3, LLVM 22.1.8, macOS arm64
|IRLoopRuntimeConfig release/debug|Pass; 46 assertions each: AST/IR/JIT/tiered, AOT O0/O3, disabled context passing, native EH and emitted-helper assertions
|program-interrupt-fast-path release/debug|Pass; 3 QUnit assertions plus native assertions comparing live, legacy and null contexts, nested Programs, deferral, priority, racing requests and foreign-thread teardown
|Saved baseline fixture|Pass; AST/IR/JIT/tiered and stripped-source AOT O0/O3, explicitly selected baseline library
|Cancellation and sandbox regressions|Pass; release sandbox-manager, release/debug thread-cancel, thread-cancel-cleanup, thread-cancel-lifecycle and concurrent-call-stacks
|Call/ownership/exception/compiler regressions|Pass; IRNativeScalarAssignment, IRFunctionRefCalls, NanBoxDoubleTagCollision, IRCallFrameReuse, IRReturnBorrowedSlot, IRPerformanceSemantics, IRExceptionTrace, StackGuardCompiled, IRWeakReferenceLocalStore, AOTOpaqueReferenceOwnership, AOTParallelCodegen, AOTRuntimeLocationCache, IRNativeLocalSSA, AOTReturnTypeCoercion, AOTFusedMapFold
|Additional release regressions|Pass; IRExecModeSmoke, AOTQoLinkNativeSymbols, AOTGenericCrossObjectFastEntry, AOTLazyFunctionContext, JitDebugStep
|OptionalMapTypeFolding|Two wrapper cases pass (20 assertions) release/debug. Full wrapper rejects an existing macOS deployment-target warning; saved baseline compiler reproduces it. Explicit baseline/release/debug stripped AOT fixture runs pass. The full wrapper is not counted as passing.
|Old AOT artifact/new runtime|Pass; all 165 reference records byte-identical
|Harness tests|Pass; 10 Python unit tests
|Measurement integrity|Pass; four experiments, 246 unique jobs / 1230 finite positive samples; independently recomputed medians, no samples discarded
|Performance controls|Direct loops improve 35.9%; retained callbacks 6.8%, 7.0% and 7.6% in successive final runs. Initial wide hash slowdown did not reproduce in five-fork repeat; both results retained.
|Size/compile checks|Native text 51112 to 51648 bytes; executable +192 bytes; single compile observations 235.53 to 234.81 ms, not a timing distribution
|Separate sampling|Five seconds at 1 ms; native frame setup/restoration, closure-marker push, callable validation and stack checks remain
|Strict docs/whitespace|qpp --table-strict on the four changed prose documents; git diff --check

Selected totals, counting completed passing cases only: **356 release cases /
12792 assertions**, five standalone scripts; **124 debug cases / 11830 assertions**,
four standalone scripts. Two OptionalMapTypeFolding cases are counted once;
its full warning-rejecting wrapper is not counted as a pass. Explicit manual
AOT executions and baseline comparisons are additional checks.

Some older wrappers set LD_LIBRARY_PATH only; a debug wrapper by itself does
not prove every child loaded the debug library on macOS. New loop/native
interrupt wrappers and manual AOT executions select matching libraries explicitly.
Linux-only resident-memory checks are skipped on macOS. No full-suite,
sanitizer, Linux or Qore allocation-count claim is made. The previously recorded
recursive JIT capture and nested AOT source-line findings remain separate.

Audit review retained the nullable teardown path, unchanged atomic ordering and
per-iteration polling, and strengthened direct coverage of live context changes.
The repeated allocation control prevents treating the initial hash slowdown or
its reversal as an established optimization benefit.

## Native marker audit (2026-10-07)

Scope: three C++ files, strengthened IRFunctionRefCalls QR fixture, evidence and
four prose documents relative to `002ba39cb`. The main agent applied
audit-changes and the module-structure, sandboxing and cooperative-cancellation
guides. No DataProvider files changed.

The common native closure-marker push is now inline. A full block calls a
noinline helper that delegates to the original insertion routine. Marker
contents (null variable, zero order, non-owner), frame count, publication order
and empty pop are unchanged. The whole-body scalar-leaf eligibility proof is
not widened. There is no compiler ABI, serialized metadata or public layout change.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration or module-layout change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration or module-layout change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration or module-layout change.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration or module-layout change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration or module-layout change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration or module-layout change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration or module-layout change.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration or module-layout change.
|Copyright 2026 on all new files|Pass|New JSON evidence carries 2026 copyright/MIT; existing source headers retained.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration or module-layout change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration or module-layout change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration or module-layout change.
|%modern directive present|Pass|Modified QR fixture already uses %modern; no module/directive change.
|Executable permission set (chmod +x)|Pass|Modified QR fixture remains executable (100755).
|Uses %prepend-module-path  before %requires for in-repo modules|N/A|Standalone QR fixture has no module requirement; unchanged wrapper already prepends qlib.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No new external or module dependency.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No C++ filesystem operation added.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No network operation added.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new filesystem/network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No Qore filesystem/network operation added to the fixture.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new C++ loop. Full-block slow helper delegates to the existing constant-size insertion path; the Qore test iterates a bounded list of eleven depths.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No cancellation API or checkpoint change; existing checks remain.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No new scan and no polling-frequency reduction.
|No blocking operations without cancellation support|Pass|No blocking operation added.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete inline available-slot path plus original full-block growth/reuse path; no TODO/FIXME/stub or eligibility workaround.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new allocation or reference ownership. Original insertion owns full-block allocation exactly as before; marker fields remain null/zero/non-owner. Frame-count update order is unchanged.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only the existing thread-local stack changes. Marker construction remains before stack publication; pop still follows reader synchronization. Concurrent stack-walk tests pass.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Uses the existing strongly typed ClosureStackEntry constructor; no casts, layout changes or ABI reinterpretation. Ordinary captured-variable entries are unchanged.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Available-slot insertion is constant time and inlined; full-block allocation stays noinline. Callback reductions 2.6% and 4.5% repeat with nonoverlapping fork ranges. Control ranges overlap; AOT native text unchanged.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Current block position is checked against the capacity; debug asserts enforce valid available-slot range. Existing full-block insertion/exception behavior is preserved.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal push/pop contract, lifetime and outlined-helper rationale documented. README/design/results record conservative eligibility, measurements and remaining Java gap. No new public language API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new format string, user input or unchecked external index. Marker position derives from the existing stack invariant. Evidence metadata is allowlisted; no inherited environment or credentials included.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Strengthened baseline/final fixtures exercise block edges/reuse, captures, owned objects, stack restoration and unwinding. Release/debug selected regressions, disabled paths, foreign-Program fallback, concurrent stacks and old AOT checks pass.

### Verification

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Strengthened saved-baseline fixture|Pass; AST/IR/JIT/tiered and stripped-source AOT O0/O3, explicit saved-library selection
|IRFunctionRefCalls release/debug|Pass; 58 wrapper assertions each, including mode/disabled-path/EH, foreign-Program, concurrent-stack and forced-split cases; fixture adds repeated depths around 64-entry block boundaries
|Additional release/debug regressions|Pass; IRCallFrameReuse, IRReturnBorrowedSlot, IRNativeScalarAssignment, IRPerformanceSemantics, IRExceptionTrace, AOTParallelCodegen, AOTRuntimeLocationCache, StackGuardCompiled, JitDebugStep, IRLoopRuntimeConfig, thread-cancel, program-interrupt-fast-path, concurrent-call-stacks
|Selected totals|76 cases / 11631 assertions and one standalone script per build; repeated development runs not double-counted
|Old AOT artifact/new runtime|Pass; all 165 reference records byte-identical
|Harness tests|Pass; 10 Python unit tests
|Incremental measurement integrity|30 screening jobs plus 50 repeat jobs; callback gains 2.6% and 4.5%, nonoverlapping fork ranges; all control ranges overlap
|Combined measurement integrity|45 jobs compare all four optimizations with 1e974dc50 and Java; direct-loop reduction 36.6%, retained callback 60.8%; combined runtime diff hash recorded separately
|All measurement samples|125 unique jobs / 625 finite positive samples; medians independently recomputed; none discarded
|Code generation / size|Disassembly confirms inline available-slot marker and only conditional full-block call; AOT native text 51648 bytes before and after
|Separate final sampling|Five seconds at 1 ms after tests; general closure insertion absent from callback hot path, frame setup/restoration, validation, stack checks and cancellation remain
|Strict documentation / whitespace|qpp --table-strict on all four prose documents; git diff --check

No full-suite, sanitizer, Linux validation or Qore allocation-count claim.
Some older debug wrappers do not select DYLD_LIBRARY_PATH for child processes;
the primary IRFunctionRefCalls and IRLoopRuntimeConfig wrappers do select matching
libraries explicitly. Prior recursive-JIT-capture, nested AOT exception-line and
default-target linker-warning findings remain outside this change; none is
counted as fixed by a performance optimization.

The audit strengthened boundary/reuse coverage and checked the marker's owner
and declaration-order fields against the original constructor. The final
combined benchmark is a fresh controlled comparison, not compounded percentages.
Java parity is not reached: retained callbacks still take 11.8x Java's time
on this kernel, and direct-call loop throughput remains about 3.0x slower.

## Native continuation audit (2026-10-07)

Scope relative to `408affc4a`: two compiler C++ files, new QR/QTest fixture,
Qore/Java/Python benchmark extensions, sanitized evidence and four prose
documents. The main agent applied audit-changes and read the module-structure,
sandboxing and cooperative-cancellation guides. No DataProvider files changed.

The change adds a single-use native result continuation, reuses the existing
native-local assigned-state proof for arguments, and recognizes complete
FloatBinary summaries for non-throwing add/subtract/multiply entry admission.
Runtime frame publication, cancellation, current Program/debugger/entry guards,
helper ABIs and serialized descriptors are unchanged.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration, or module layout.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration, or module layout.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration, or module layout.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration, or module layout.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration, or module layout.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration, or module layout.
|Copyright 2026 on all new files|Pass|New QR/QTest and JSON evidence carry 2026 copyright and MIT licensing.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration, or module layout.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration, or module layout.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration, or module layout.
|%modern directive present|Pass|New QR/QTest and benchmark fixture use %modern.
|Executable permission set (chmod +x)|Pass|New QR/QTest are executable (100755).
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends repository qlib before requiring QUnit/FsUtil.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit and FsUtil are required; no external dependency.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No filesystem/network operation added to C++.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No filesystem/network operation added to C++.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No filesystem/network operation added to C++.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No File::/Dir::/Socket::/HTTPClient:: operation added. TmpDir and subprocesses are test setup only, matching existing wrappers.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|The new local/store scan polls qore_check_cancel every 100 combined iterations; argument proof reuses existing analysis instead of rescanning the CFG.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Uses qore_check_cancel; no deprecated interrupt API.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|New scans are tight map/type checks with every-100 polling; runtime language-loop checkpoints are unchanged.
|No blocking operations without cancellation support|Pass|No new blocking runtime operation. Existing test helpers use bounded waits and scoped temporary directories.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete conservative fast path and generic fallback; no TODO, FIXME, stub, or disabled correctness check.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Native payloads own no references; the fast boxed cleanup value is NOTHING. Generic owned results and coercion cleanup retain registration, clearing and sink checks. Tests alternate fast/slow edges and destroy rejected objects.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|New maps are compiler-instance/per-function state, cleared for each lowering. Runtime reference descriptors, live Program/debugger/entry guards, stack publication and reader handshake are unchanged.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only exact strict scalar locals and actual native LLVM carriers are admitted. Definite-assignment proof excludes unassigned/lvalue-invalidated loads; boxed source metadata still blocks native argument reuse. Typed code references and static_cast are used. FloatBinary admission is restricted to whole-body add/subtract/multiply; division remains generic.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Linear candidate scan with constant-time proof lookup; no per-call CFG analysis. Before/after measurements and rejected initial experiment retained; see results.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Valid operand count, single use, exact destination type and StoreLocal scope/mode checked before creating a continuation. Generic mismatches, NOTHING, missing arguments and floating division retain errors.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Design/README document proof boundaries, comparison switches, fallback behavior and remaining runtime costs. No new public method or runtime ABI.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new user-controlled formatting or indexing. Reference identity, Program and debugger checks remain dynamic. Evidence metadata is allowlisted; inherited environment is excluded.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|AST/IR/JIT/tiered and O0/O3 AOT differential fixtures, disabled paths, split codegen, EH, owned rejected values, NaNs, wide integers, foreign Programs and old artifacts verified; benchmark checksums independently computed.

### Verification

|!Verification|!Result
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|New fixture on saved baseline|Pass; AST/IR/JIT/tiered and stripped-source AOT O0/O3, matching saved-library selection
|IRNativeFunctionRefContinuation|Pass; 170 assertions per build, including O0/O3, native result/float adapter selection, throwing division exclusion, disabled paths, v1/v2, EH, debug/source stripping and forced-split compilation
|Reference/assignment regressions|Pass; IRFunctionRefCalls, IRNativeScalarAssignment, IRCallFrameReuse, IRReturnBorrowedSlot, IRPerformanceSemantics, IRExceptionTrace
|Codegen/context regressions|Pass; AOTParallelCodegen, AOTRuntimeLocationCache, StackGuardCompiled, JitDebugStep, IRLoopRuntimeConfig, AOTQoLinkNativeSymbols, AOTGenericCrossObjectFastEntry, AOTLazyFunctionContext
|Representation/ownership regressions|Pass; NanBoxDoubleTagCollision, IRNativeLocalSSA, AOTReturnTypeCoercion, AOTOpaqueReferenceOwnership, IRWeakReferenceLocalStore
|Thread/context regressions|Pass; thread-cancel, program-interrupt-fast-path, concurrent-call-stacks
|Final selected totals|107 cases / 11931 assertions across 19 wrappers plus four standalone scripts per build; repeated development runs not double-counted
|Old AOT/new runtime|Pass; all 165 reference records byte-identical
|Benchmark correctness|225 independent checksum records for each selected final implementation; counts 0/1/17/1024/4096, seeds 0/7/65535
|Harness tests|Pass; ten Python unit tests, including new independent scalar references
|Measurement integrity|285 unique jobs / 1425 finite positive samples, all medians recomputed, no discarded samples; builds/tests/profiles did not overlap timing runs
|Rejected candidate|Retained; continuation alone caused a 16.5% generic-float regression. Final generic control improves 3.0%/3.9%; differing initial/final float workload definitions explicitly recorded
|Repeated gains|Wide/native-float time reductions 82-91% with nonoverlapping ranges in both final runs; small-int benefit only 1-3%, with overlap in the repeat
|Controls|Arithmetic, direct-call, polymorphic and temporary-hash ranges overlap; no general improvement claimed
|Code generation|IR and ARM64 disassembly confirm native arguments/results on the handled edge, boxed/coercing fallback retained, float leaf adapters present and division excluded
|Code size|Native text 76416 to 75452 bytes in both final runs; compile/startup observations not claimed as speedups
|Separate final profile|Five seconds at 1 ms, after measurement and before regression jobs; native frames, validation, stack checks and cancellation remain
|Strict docs/whitespace|qpp --table-strict on all four prose documents; git diff --check

### Audit decisions and limits

Review added an explicit match to StoreLocal's outer-scope exclusion so a
placeholder boxed value can never be published through that path. It also
strengthened argument-proof tests with partially assigned and deleted inputs,
and preserved the immediate-scalar guard on fallback rather than forcing full
coercion. The first failed measurement is retained as evidence, not hidden by
the later source/eligibility changes.

New maps are per-function compiler state. Scalar payloads do not own heap
references; generic owned values retain ordinary cleanup, including when a
later loop iteration takes the native edge. The FloatBinary summary describes
the complete body, not just its return expression; capture, method/context,
generic-reentry and throwing-operation exclusions remain required. Stack
checks and reader-visible frame lifetimes are not relaxed.

No full-suite, sanitizer, Linux or Qore allocation-count claim. Linux-only RSS
checks are not exercised on macOS. Some older wrappers set only LD_LIBRARY_PATH
for child processes; debug wrapper totals do not prove every legacy AOT child
loaded a debug runtime. The primary new continuation/reference/context wrappers
select matching LD_LIBRARY_PATH and DYLD_LIBRARY_PATH explicitly.

Previously recorded recursive-JIT-capture, nested AOT exception-line and
default-target linker-warning findings remain outside this change and are not
counted as fixed. Java parity is not reached: native retained callbacks still
take about 11.5-12x Java's time. Remaining frame/validation work is described in
the next-stage plan in RESULTS.md.

## Prepared target audit (2026-10-07)

Scope relative to `75d26dde3`: three C++ files, expanded QR/QTest regression,
sanitized benchmark evidence and four prose documents. The main agent applied
audit-changes using the module-structure, sandboxing and cooperative-cancellation
guides already read for this optimization series. No DataProvider/qlib edits.

The implementation caches immutable facts only, retains an exact non-capturing
reference to prevent recycled-address hits, and invalidates borrowed fields
through the existing persistent cleanup owner token. Dynamic guards, lazy
failure timing, publication order and v1/v2 compatibility remain intact.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration, or module layout.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration, or module layout.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration, or module layout.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration, or module layout.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration, or module layout.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration, or module layout.
|Copyright 2026 on all new files|Pass|New sanitized evidence carries 2026 copyright and MIT licensing; edited fixture already has 2026 copyright.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration, or module layout.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration, or module layout.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration, or module layout.
|%modern directive present|Pass|QR/QTest fixture uses %modern.
|Executable permission set (chmod +x)|Pass|Existing QR/QTest remain executable (100755).
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Wrapper prepends repository qlib before QUnit/FsUtil.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|No blocking operations without cancellation support|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete versioned cache/helpers, lowering, disable control and regression tests; no TODO/FIXME/stub.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Owner token initialized in native entry and registered with persistent exit/EH cleanup. Replacement retains before releasing; exact node has no user destructor. Sink checked before execution and after frame installation; RAII restores the frame on exceptions.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Each cache is stack-local to one invocation and signature. Borrowed facts are immutable under an owned reference. Program/debugger/entry/readiness remain live; existing acquire publication and stack-reader handshake unchanged.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Typed code<int(int)> tests; exact RTTI established before downcast; target-pointer LLVM layout mirrors versioned C++ descriptor. Type/signature admission and v1/v2 ABIs unchanged.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time positive cache, no scan/allocation on hits; measured retained-call improvements with generic and direct controls. Raw samples retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|NOTHING invalidates all borrowed fields; replacement, recycled-address pressure, foreign Program, debugger attach/detach and catch reuse are covered; existing generic binding retained.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal ABI ownership, invalidation, immutable/live facts, lifetime and versioning documented in header, implementation, current-state design and benchmark README/results. No new user-visible class/method API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP changes.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No I/O or user-controlled formatting added. Descriptor initialized before use and fixed-layout slots allocated using target pointer types. Evidence excludes inherited environment values.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|AST/reference checksums and release/debug selected regression matrix pass; all 12 helpers registered; old artifact/new runtime checks retained.

### Verification

|!Check|!Outcome
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Saved-baseline expanded fixture|Pass; AST/IR/JIT/tiered and stripped-source AOT O3 with explicit saved-library selection
|Native continuation wrapper|Pass; 208 assertions per build, O0/O3, native/prepared/typed/context-disabled controls, EH, stripped debug information, forced split, runtime fast-call disabled
|Selected totals|107 cases / 11969 assertions in 19 wrappers and four standalone scripts per build
|Semantics|Target replacement, catch reuse, foreign Program, debugger attach/detach after warm-up, scalar edge cases, ownership, stack guards, thread cancellation, Program interruption and concurrent stack walkers pass
|Old artifact/new runtime|Pass; 225 records byte-identical
|Harness|Pass; ten Python unit tests
|Measurement integrity|195 unique jobs / 975 finite positive samples; medians independently recomputed; no discarded benchmark samples
|Native callback benefit|17-20% reductions with nonoverlapping fork ranges in both runs
|Generic controls|Branch-float median 1.2-1.5% slower; ranges overlap; noisy temporary-hash median 4.3% slower; all retained and no zero-regression claim
|Code generation / size|Owner initialized to NOTHING, persistent cleanup on exits, v3 helper and native continuation present; native text 75452 to 77592 bytes
|Final profile|Five seconds at 1 ms, separate from builds/tests/timings; RTTI/type decoding absent from native hot path, frame helpers and outlined cache hit remain
|Documentation / whitespace|Strict qpp tables on all four prose docs; git diff --check

No full-suite, sanitizer or Linux claim. Some legacy debug wrappers do not
select DYLD_LIBRARY_PATH for every child; focused callback/reference/context
wrappers explicitly select matching libraries. Pre-existing recursive-JIT-capture,
nested AOT exception-line and default-target linker-warning findings remain
outside this change. Native callbacks still take 9.4-9.9x Java's time.

The audit motivated explicit owner-token invalidation documentation and tests
for debugger attachment after cache warm-up; no guard is hoisted merely because
a target was prepared.

## Native frame inlining audit (2026-10-07)

Scope relative to `1faf357d1`: two runtime C++ files, existing design/results
documentation and sanitized evidence. The main agent applied audit-changes with
the module-structure, sandboxing and cooperative-cancellation guides already
read for this series. No test/qlib/DataProvider source or serialized ABI changed.

The positive cache hit remains inline, misses are outlined, and native frame
constructors/destructor are inlined when their implementations are visible.
Frame publication/destruction order, owner cleanup, dynamic guards, cancellation
and stack protection are unchanged. An unmeasured development disassembly
exposed an enclosing-frame call boundary; that was fixed before final tests
and all recorded timing runs.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration, or module layout.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration, or module layout.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration, or module layout.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration, or module layout.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration, or module layout.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration, or module layout.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration, or module layout.
|Copyright 2026 on all new files|Pass|New sanitized evidence carries 2026 copyright and MIT licensing; edited fixture already has 2026 copyright.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration, or module layout.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration, or module layout.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration, or module layout.
|%modern directive present|N/A|No test source changed; the existing focused regression matrix is rerun in release and debug.
|Executable permission set (chmod +x)|N/A|No test source changed; the existing focused regression matrix is rerun in release and debug.
|Uses %prepend-module-path  before %requires for in-repo modules|N/A|No test source changed; the existing focused regression matrix is rerun in release and debug.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No test source changed; the existing focused regression matrix is rerun in release and debug.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No new filesystem/network operation; existing scoped subprocess/TmpDir code is test setup only.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|No blocking operations without cancellation support|Pass|No C++ loop or blocking operation added. Existing language-loop checkpoints and cancellation helpers are unchanged.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete inlining boundary change and outlined miss path; no TODO/FIXME/stub or semantic shortcut.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Same RAII frame, initialization/destruction order, owner token and sink checks. Explicit defaulted destructor preserves member cleanup; all existing exception exits remain.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|No new shared state. Same publication order, acquire/release operations, rare reader wait, live Program/debugger/entry guards and per-invocation cache lifetime.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|No helper ABI, descriptor layout or callable signature changes. Miss function retains exact RTTI validation and original metadata extraction.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Inlining removes measured positive-hit/frame call layers without removing work required by semantics; before/after evidence records runtime text tradeoff.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing guard/fallback and replacement/NOTHING/exception paths unchanged; regression matrix covers cache misses, debugger attachment, foreign Programs and stack exhaustion.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Implementation comments and current-state/results docs explain inlining scope, split-build fallback symbols and unchanged runtime guarantees; no new public API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP changes.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new input, I/O, formatting or storage. Sanitized measurement evidence excludes inherited environment.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Identical benchmark checksums, old v3 artifact compatibility, release/debug selected regression matrix and disassembly verify behavior and intended inlining.

### Verification

|!Check|!Outcome
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3; no new attribute diagnostics
|Final regression matrix|107 cases / 11969 assertions in 19 wrappers plus four standalone scripts per build; development runs not double-counted
|Semantics|Stack guards, debugger attach/detach after cache warm-up, cancellation, Program interruption, concurrent walkers, foreign Program fallback, owned results and catch reuse pass
|Old v3 AOT artifact/new runtime|Pass; all 225 records identical
|Harness checks|Ten Python tests and 225 independent benchmark records per implementation pass
|Timing integrity|195 unique jobs / 975 finite positive samples; independently recomputed median fork means; no builds/tests/profiles alongside timing
|Native callback result|14-18% incremental reduction; nonoverlapping fork ranges in both runs
|Generic controls|Branch-float medians 0.8-1.5% slower; generic-control ranges overlap; no zero-regression claim
|Code generation / size|Frame and positive-hit helper calls absent in hot-path disassembly; ordinary helper symbols retained; AOT text unchanged, runtime text +21788 bytes
|Profiles|Separate callback/arithmetic profiles, five seconds at 1 ms each, no concurrent build/test/timing/profile work
|Documentation / whitespace|Strict qpp tables on changed prose; git diff --check

The audit checked complete nesting of the inlining boundary rather than assuming
a hint removed a call, and verified publication/cleanup semantics against the
unchanged implementations. No full-suite, sanitizer, Linux or split-build/GCC
performance claim. Legacy debug-wrapper child-library-selection and previously
recorded recursive-JIT-capture/nested-AOT-location/default-target-linker findings
remain. Native callbacks are still 8.0-8.2x Java, so the optimization goal is not
marked complete. The new profiles prioritize preserving loop cancellation
semantics while removing its no-pending helper call overhead.

## Inline cancellation poll audit (2026-10-07)

Scope relative to `77f11fc72`: six runtime/compiler C++ files, two existing
Qore loop fixtures, one native cancellation fixture, design/benchmark prose
and sanitized evidence. The main agent applied audit-changes, with module
structure and sandboxing guides read for this series and the complete
cooperative-cancellation guide reread for this iteration. No qlib,
DataProvider, QPP or serialized metadata change.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration or module layout.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration or module layout.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration or module layout.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration or module layout.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration or module layout.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration or module layout.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration or module layout.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration or module layout.
|Copyright 2026 on all new files|Pass|Modified test fixtures already carry 2026/MIT; new sanitized evidence carries 2026/MIT.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration or module layout.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration or module layout.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration or module layout.
|%modern directive present|Pass|Both Qore loop fixtures use %modern.
|Executable permission set (chmod +x)|Pass|Both Qore loop fixtures remain tracked mode 100755.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends repository qlib before QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only modules delivered with Qore are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Runtime changes add no filesystem/network operations.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|Runtime changes add no filesystem/network operations.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|Runtime changes add no filesystem/network operations.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Existing TmpDir/subprocess usage is scoped test compilation and execution; no runtime I/O added.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new long C++ scan or blocking operation. Every existing language-loop checkpoint remains; fused internal loops retain their 100-element poll. Pending/unavailable cases use the full nondeprecated cancellation helper.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No new long C++ scan or blocking operation. Every existing language-loop checkpoint remains; fused internal loops retain their 100-element poll. Pending/unavailable cases use the full nondeprecated cancellation helper.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No new long C++ scan or blocking operation. Every existing language-loop checkpoint remains; fused internal loops retain their 100-element poll. Pending/unavailable cases use the full nondeprecated cancellation helper.
|No blocking operations without cancellation support|Pass|No new long C++ scan or blocking operation. Every existing language-loop checkpoint remains; fused internal loops retain their 100-element poll. Pending/unavailable cases use the full nondeprecated cancellation helper.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete versioned descriptor, generated guard, null/layout fallback and disable control; no TODO/FIXME/stub.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Descriptor is stack-local and contains no owned resources. All fields initialized before use; merged exception-sink checks and the full pending/error path are retained.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only stable addresses are borrowed within the active same-thread invocation. Deferral is owner-thread state; both request loads retain seq_cst. No request value, manager identity, scope or Program is cached.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Target-sized pointer fields match LLVM layout. Runtime admits only matching unsigned/atomic sizes, alignment and lock-free representations; unsupported layouts select full helper. Existing RuntimeConfig layout unchanged.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time negative-path guards remove measured per-checkpoint helper calls. No allocations or scans added; before/after evidence includes direct, callback and generic controls.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Invalid TIDs/null contexts initialize all-null descriptors; generated code checks availability before dereferencing. Pending flags always re-enter scope/manager/priority resolution; teardown and re-registration tested.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal ABI and borrowed lifetime, atomic order, layout fallback, disable switches and unchanged polling frequency documented in headers, current-state, cancellation guide and benchmark docs.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled formatting, I/O or unbounded indexing. TID bounds checked; descriptor initialized. Evidence uses an allowlist excluding inherited environment.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|AST/reference benchmark checksums, enabled/disabled JIT/AOT, O0/O3, EH, split/stripped artifacts, cancellation/deferral, Program interruption races, old AOT and release/debug regression tests pass.

### Verification

|!Check|!Outcome
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Expanded fixture on saved baseline|Pass; AST/IR/JIT/tiered and stripped-source AOT O3
|Final regression matrix|108 cases / 12085 assertions in 20 wrappers plus seven standalone checks per build
|Generated cancellation|While/for/do/foreach, enabled/disabled controls, O0/O3, JIT/tiered, EH, split and stripped-debug artifacts; seq_cst i8/i64 loads asserted
|Native cancellation|Five paths including explicit/legacy/null context and real/null descriptors; deferral, manager changes, request-clear races, sinkless checks and 20 foreign-thread re-registrations
|Fused loops / shutdown|Map-fold and collection fusion, context elision and linked-executable background-thread shutdown checks pass
|Old artifact / benchmark correctness|225 old-artifact records identical; 225 independent checks per benchmark implementation
|Harness|Ten Python unit tests pass
|Timing integrity|195 unique jobs / 975 finite positive samples, independently recomputed medians; no samples discarded or concurrent builds/tests/profiles
|Measured benefit|Arithmetic/direct 26-37% and native callbacks 5-8% reductions; nonoverlapping fork ranges in both runs
|Controls / size|Generic ranges overlap; temporary hash 1.3% slower median retained; AOT text 77592 to 81864, runtime text +2116 bytes
|Inspection|Native atomic LDARB/LDAR loads remain; full helper only on pending/unavailable path; merged sink check retained
|Profiles|Sequential callback/arithmetic sampling, five seconds at 1 ms; no concurrent build/test/benchmark/profile
|Documentation / whitespace|Strict qpp tables on all five changed prose docs; git diff --check

The audit drove explicit documentation and checks for address-only borrowing,
thread teardown/re-registration, all-null descriptor fallback and unchanged
atomic order/frequency. A development-only undeclared LLVM type name was fixed
before final release/debug builds and every recorded measurement.

No full-suite, sanitizer or Linux claim. Focused wrappers explicitly select
matching libraries; some legacy debug wrappers retain the previously noted
child-library-selection caveat. Other pre-existing recursive-JIT-capture,
nested-AOT-location and default-target-linker findings remain out of scope.
Callbacks still take 7.5-7.8x Java's time; the active goal remains incomplete.

## Typed callee RuntimeConfig audit (2026-10-07)

Scope relative to `23434f6c8`: four runtime/compiler C++ files, one expanded
Qore wrapper, a new native stack-guard fixture and wrapper, existing
design/results docs and sanitized evidence. The main agent applied
audit-changes with all required module-structure, sandboxing and cancellation
guides read for this series. No qlib, DataProvider or serialized ABI change.

The optional entry appends RuntimeConfig while retaining v1. Its binding must
match the validated v1 body/signature and is immutable after publication.
Review fixed stale optional-entry invalidation on v1 rebinding and cleared the
creating caller's debug location before emitting the adapter. Final builds,
regressions and measurements all use those fixes.

### Full checklist

|!Check|!Status|!Evidence
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class, registration or module layout.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QPP class, registration or module layout.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class, registration or module layout.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class, registration or module layout.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class, registration or module layout.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class, registration or module layout.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class, registration or module layout.
|No %include usage (deprecated for modules)|N/A|No new module, QPP class, registration or module layout.
|Copyright 2026 on all new files|Pass|New native fixture, Qore wrapper and sanitized evidence carry 2026/MIT.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class, registration or module layout.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class, registration or module layout.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class, registration or module layout.
|%modern directive present|Pass|Changed and new Qore wrappers use %modern.
|Executable permission set (chmod +x)|Pass|Existing wrapper remains 100755; new wrapper has executable permission.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib prepended before QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only modules delivered with Qore are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new runtime filesystem/network operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new runtime filesystem/network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new runtime filesystem/network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|TmpDir/subprocess use is bounded, scoped native-test compilation and execution.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|Admitted compiler summaries contain at most 64 nodes; no new long runtime loop or blocking operation. Native test recursion checks cancellation every 100 levels. Existing language-loop and fused-loop checkpoints unchanged.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Admitted compiler summaries contain at most 64 nodes; no new long runtime loop or blocking operation. Native test recursion checks cancellation every 100 levels. Existing language-loop and fused-loop checkpoints unchanged.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Admitted compiler summaries contain at most 64 nodes; no new long runtime loop or blocking operation. Native test recursion checks cancellation every 100 levels. Existing language-loop and fused-loop checkpoints unchanged.
|No blocking operations without cancellation support|Pass|Admitted compiler summaries contain at most 64 nodes; no new long runtime loop or blocking operation. Native test recursion checks cancellation every 100 levels. Existing language-loop and fused-loop checkpoints unchanged.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete optional typed entry, binder, guard and disable control; unavailable summaries retain v1 rather than emitting a partial body.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new runtime allocation/ownership. Existing RAII frame and sink/exception exits remain. LLVM module owns adapters; failed emission erases the function and InsertPointGuard restores caller state.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Optional entry is initialized before publication and immutable thereafter. Rebinding v1 clears it. RuntimeConfig stays invocation-local; live Program/debugger/entry guards and observer handshake unchanged.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Versioned signature appends RuntimeConfig after native args, context and sink; scalar kind/body identity must match validated v1. Booleans normalized; exact return carrier checked. Prepared v3 layout unchanged.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Constant-time entry selection and explicit stack guard avoid callee TLS lookup. Reuses bounded summary emitters; before/after timing and code-size evidence retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Invalid/mismatched bindings declined, missing/disabled summaries retain old entry. Guard failure returns zero with the sink populated; helper frame unwinds normally. Native test checks exhaustion and recovery in main/foreign threads.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|New internal ABI params/lifetime documented in header; README and current-state distinguish helper versions from callee-entry versions and explain controls and unchanged guarantees.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled formatting, I/O or unbounded buffer access. Test padding indexes are fixed; summary index validation reused. Measurement metadata is allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug matrix, old artifact, 225-record independent benchmark checks and generated-code inspection pass; both guards reject exhaustion and recover. Callback tests cover zero/unary, wide/scalar edges, NaNs, fallback, replacement, foreign Programs and debugger attachment.

### Verification

|!Check|!Outcome
|Release/debug builds|Pass; cmake --build build and build-debug --target qore qcc -j3
|Final selected regressions|109 cases / 12280 assertions in 21 wrappers plus seven standalone checks per build
|Expanded callback wrapper|400 assertions per build; entry/summary disable controls, O0/O3, split, EH, stripped source/debug info, runtime fallback
|Native stack fixture|Legacy and explicit guards reject exhaustion and recover; main thread and three foreign-thread registrations
|Runtime semantics|Scalar/NaN/zero/unary results, ownership, catch reuse, target replacement, foreign Programs, debugger attachment, cancellation and concurrent stack walkers pass
|Old AOT / independent checks|225 old-artifact records identical; 225 reference checks per benchmark implementation
|Harness|Ten Python tests pass
|Primary timing integrity|195 unique jobs / 975 finite positive samples; medians independently recomputed; no builds/tests/profiles alongside either primary run
|Diagnostic repeat retained|90 jobs / 450 samples overlapped brief qpp documentation checks near job 55; explicitly excluded from primary claims and fully rerun in isolation
|Native callback result|3.3-5.0% lower time; nonoverlapping fork ranges in both primary runs
|Controls|Arithmetic/direct medians 0.4-0.6% slower, ranges overlap; generic ranges also overlap; no zero-regression claim
|Code size|AOT text +480 bytes (0.6%); runtime text +3600 bytes
|Inspection / profile|Callee guard uses RuntimeConfig without TLS; separate five-second profile retains frame-helper dominance; stack checks and reader handshake unchanged
|Documentation / whitespace|Strict qpp tables on all four changed prose docs; git diff --check

No full-suite, sanitizer or Linux claim. Focused wrappers select matching
libraries; legacy child-library-selection and previously recorded
recursive-JIT-capture/nested-AOT-line/default-target-linker findings remain.
The audit influenced entry invalidation, debug-scope handling, native guard
exhaustion coverage and explicit retention of a timing run affected by a
documentation check. Callbacks still take 7.1-7.5x Java's time, so the goal
remains active.

## Float comparison summary audit (2026-10-07)

Scope: uncommitted changes against b6923e800 on feature/performance.
Production: QoreJIT.h, QoreAOT.cpp, QoreIRToLLVM.cpp. Tests: three new
comparison fixtures and AOTSymbolIndex child-library selection. Supporting
changes: benchmark comment, README, RESULTS, current-state and sanitized
measurement evidence. No qlib, DataProvider, public API or parser change.

Applied audit-changes and the module-structure, sandboxing and cooperative-
cancellation guidance. Every checklist item follows; repository Qore table
syntax takes precedence over the skill's example Markdown table.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module or C++ QPP class.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module or C++ QPP class.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module or C++ QPP class.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module or C++ QPP class.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module or C++ QPP class.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module or C++ QPP class.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module or C++ QPP class.
|No %include usage (deprecated for modules)|N/A|No new module or C++ QPP class.
|Copyright 2026 on all new files|Pass|New fixtures and measurement evidence carry 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module or C++ QPP class.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module or C++ QPP class.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module or C++ QPP class.
|%modern directive present|Pass|New fixtures and existing symbol-index wrapper use %modern.
|Executable permission set (chmod +x)|Pass|All three new fixtures are executable; existing wrapper remains 100755.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit/FsUtil requirements follow the repository qlib prepend.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil modules are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new runtime filesystem/network operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new runtime filesystem/network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new runtime filesystem/network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|File/TmpDir/subprocess use is limited to scoped test compilation, imports and execution.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|New diamond scans share the existing 80-instruction whole-body cap; summaries cap at 64 nodes, functions at eight blocks. Outer collection/dependency loops retain cancellation every 100 visits.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Existing qore_check_cancel() remains; no deprecated check introduced.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Existing tight-loop 100-visit checks remain; bounded new scans cannot exceed 100 instructions.
|No blocking operations without cancellation support|Pass|No new runtime blocking operation. Tests exercise existing subprocess APIs.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete conservative admission and validation, with ordinary fallback for unsupported bodies; no stubs/TODOs or partial summaries.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new runtime Qore allocations or ownership paths. Compiler containers are RAII-managed; rejected summaries are not published. Existing sink/stack/error paths remain.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Compiler-local maps/vectors only. Published summaries are immutable. Runtime live guards, frames, RuntimeConfig state and reader handshake are unchanged.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Typed float comparisons produce bool nodes; arithmetic/Neg require float operands; Select requires bool/float/float and final result float. Import checks match collection. Tests use code<float(float)>.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Bounded four-block diamond recognition and 64-node summaries. Comparison/select emission removes generic callback dispatch; before/after results and code size retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Validate ownership of blocks, unique visitation, terminal branches/returns, common merge, two PHI predecessors, bounds and operand types. Unknown optional tags are declined by old readers; unsupported/throwing work falls back.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal enum semantics, README, current-state and measured limitations documented; no new public method or QPP API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No production I/O, user format strings or unchecked new index access. Operand validation also rejects boolean negation on import. Evidence metadata is allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug truth tables cover six comparisons in both operand positions, infinities, both NaNs, signed zero, nested branches and lazy division/side effects. O0/O3, disabled paths, split/EH, cross-object and old/new reader checks pass.

### Verification

|!Check|!Outcome
|Builds|Release/debug qore and qcc targets pass; stale debug reflection module rebuilt before final symbol-index checks
|Selected regression matrix|155 cases / 13331 assertions in 23 wrappers plus 11 standalone checks per build
|New comparison wrapper|632 assertions per build; verifies all 15 eligible entries and conservative throwing/effect fallbacks
|Semantic edge coverage|All six comparisons, both operand positions, infinities/NaNs, signed zero, nested branches, lazy division and side effects
|Compatibility|Old/new producer-consumer objects pass both directions; final fixture passes baseline compiler; old artifact's 225 records match
|Independent checks / harness|225 records per implementation; ten Python tests pass
|Timing integrity|195 unique jobs / 975 finite positive samples; medians independently recomputed; no builds/tests/profiles/docs validators during timings
|Measured benefit|Branch callback time falls 90.40% and 90.86%; fork ranges do not overlap in either run; no general control-kernel speedup claim
|Code size|AOT text +628 bytes; runtime text +2828 bytes
|Generated code / separate profile|Typed branch entries, ordered comparison/select and callee guard present; helper/frame work still dominates qualitative profile
|Documentation / whitespace|Strict qpp tables on changed prose; git diff --check

No full-suite, sanitizer or Linux claim. Initial fixture parse/oracle mistakes
were fixed before validation and timing; the nested-index oracle failure also
reproduces on the baseline and is recorded separately in RESULTS. The stale
debug reflection artifact was rebuilt, not bypassed. Audit review additionally
tightened float/bool node checks, retained the PHI condition by value across
map insertions, kept speculative division/side effects ineligible, and made
child library selection explicit. Java parity remains incomplete.

## Compact prepared frame audit (2026-10-07)

Scope: uncommitted changes against 0f3791786 on feature/performance.
Production change is limited to JITRuntime.cpp. Two executable fixtures,
README/current-state, measured results and sanitized raw evidence accompany it.
No qlib, DataProvider, parser, public API or serialized/native helper ABI change.

Applied audit-changes and module-structure, sandboxing and cooperative-
cancellation guidance. All 62 checklist items are reported below.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module or C++ QPP class.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module or C++ QPP class.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module or C++ QPP class.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module or C++ QPP class.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module or C++ QPP class.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module or C++ QPP class.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module or C++ QPP class.
|No %include usage (deprecated for modules)|N/A|No new module or C++ QPP class.
|Copyright 2026 on all new files|Pass|New fixtures and measurement evidence carry 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module or C++ QPP class.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module or C++ QPP class.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module or C++ QPP class.
|%modern directive present|Pass|New fixtures and existing symbol-index wrapper use %modern.
|Executable permission set (chmod +x)|Pass|All three new fixtures are executable; existing wrapper remains 100755.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|QUnit/FsUtil requirements follow the repository qlib prepend.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil modules are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new runtime filesystem/network operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new runtime filesystem/network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new runtime filesystem/network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|File/TmpDir/subprocess use is limited to scoped test compilation, imports and execution.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new C++ loop. Existing compiler/runtime cancellation checkpoints and blocking behavior are unchanged; Qore fixture loops use normal language cancellation.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No new/deprecated cancellation call; existing qore_check_cancel paths remain.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No polling interval changes or new long C++ scan.
|No blocking operations without cancellation support|Pass|Existing reader handshake is retained; no new runtime blocking operation.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete descriptor-backed v3 frame; v2 keeps its original independent frame. No stub, TODO or partial fallback.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No runtime allocation or ownership change. Cache owner already retains metadata; the publishing helper stays last and waits for readers before frame destruction or subsequent cache replacement/cleanup.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|The descriptor belongs to one invocation/thread; a scalar leaf cannot reenter or mutate it while published. Release publication and the existing pop/walker handshake order all observer reads before later cache writes.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Const descriptor reference replaces three copied pointers; virtual methods keep the same types and results. Tests use code<int(int)> and a typed hashdecl result.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Fewer per-call metadata copies and a statically smaller frame. Native int helper stack allocation shrinks 272 to 256 bytes; isolated before/after timings retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|All admission, live Program/debugger/entry/readiness guards and sink checks remain identical. Descriptor access occurs only after successful preparation; disabled/generic cases use existing paths.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal class documents publication/lifetime constraints; README/current-state and measurements explain scope. No public method or helper ABI change.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No I/O, user-controlled formats or new indexed access in production. Borrowed metadata remains pinned by the unchanged owner and reader handshake; evidence metadata is allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug regressions, native reader stress during hit/replacement/cleanup, O0/O3/EH/split and disabled paths pass. Existing target replacement, foreign Program, debugger, ownership, stack guard and cancellation coverage retained.

### Verification

|!Check|!Outcome
|Builds|Release/debug qore and qcc targets pass
|Selected regressions|156 cases / 13403 assertions in 24 wrappers plus 11 standalone checks per build
|New observer fixture|72 assertions per build; four callers, two readers, 400 snapshots per reader; hits, target replacement and repeated invocation cleanup
|Execution coverage|AST/IR/JIT/tiered and O0/O3, v2, runtime generic fallback, EH and split AOT; pre-change fixture also passes
|Borrowed lifetime review|Descriptor cannot change within the admitted non-reentrant leaf; pop waits for observers before any cache replacement/cleanup; publishing helper remains last
|Old artifact / independent checks|225 matching old-artifact records; 225 independently checked records per implementation
|Harness|Ten Python tests pass
|Timing integrity|195 jobs / 975 finite positive samples; independently recomputed medians; no overlapping builds/tests/profiles/docs validators
|Benefit|Ordinary/tagged float time falls 2.15-2.79%, nonoverlapping ranges in both runs; integer/branch results remain less conclusive
|Controls / outliers|Small overlapping-range median regressions reported; wide-callback outlier and all generic-control samples retained
|Code/stack size|Integer helper stack allocation 272 to 256 bytes; AOT text unchanged; runtime text +1092 bytes
|Separate profile|Helper/frame remains dominant; no additive attribution or Java-parity claim
|Docs / whitespace|Strict qpp tables for changed prose and git diff --check

The audit drove dedicated concurrent-reader coverage and explicit lifetime
documentation rather than weakening publication or restoration semantics.
Fixture authoring errors were corrected before final validation/timing.
No full-suite, sanitizer or Linux claim; existing unrelated findings remain
documented. The next inline-guard experiment is still pending.

## Inline stack guard audit (2026-10-07)

Scope: all uncommitted changes after `9438835b4`. C++ runtime/compiler,
four existing Qore wrappers, one native test harness, design/benchmark prose
and sanitized JSON evidence. No qlib module, DataProvider, QPP or JNI change.
Applied audit-changes and the module-structure, sandboxing and cooperative-
cancellation design guides. Every checklist item is reported below.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module or C++ QPP class.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module or C++ QPP class.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module or C++ QPP class.
|Module added to QMOD list in CMakeLists.txt|N/A|No new module or C++ QPP class.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module or C++ QPP class.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module or C++ QPP class.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module or C++ QPP class.
|No %include usage (deprecated for modules)|N/A|No new module or C++ QPP class.
|Copyright 2026 on all new files|Pass|New evidence has 2026/MIT metadata; existing source/test notices retained.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module or C++ QPP class.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module or C++ QPP class.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module or C++ QPP class.
|%modern directive present|Pass|All four changed Qore wrappers retain %modern.
|Executable permission set (chmod +x)|Pass|All four wrappers remain tracked mode 100755.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib prepend precedes QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only modules delivered with Qore are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new runtime filesystem or network operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new runtime filesystem or network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new runtime filesystem or network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Existing scoped TmpDir/File/subprocess operations compile, inspect and execute test artifacts only.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|New compiler entry loop has two iterations; argument copy at most four. Native recursive test retains cancellation every 100 levels; no new long runtime scan.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Existing qore_check_cancel retained; no deprecated API added.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No production polling interval changes; recursive harness polls every 100 levels.
|No blocking operations without cancellation support|Pass|No new runtime blocking operation; observer handshake remains unchanged.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete optional ABI and full-check fallback, no TODO/stub. Rejected merge-back design replaced after measurements.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new runtime allocation. Live limit is borrowed from the current thread only; full v2 fallback checks its result and preserves sink/overflow handling. Frame unwinding and cache ownership are unchanged.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Bindings are immutable before publication, with v1/v2 replacement clearing v3. ThreadData limit address stays on the acquiring thread within its invocation; no copied mutable limit or cross-thread cache. Publication/pop-reader handshake retained.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Validated scalar signature selects the existing typed C++ ABI plus RuntimeConfig and const size_t pointer. Fixed i64 view is supplied only when size_t and pointer are 64-bit; null bypasses it elsewhere. No serialized/layout reinterpretation of RuntimeConfig.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Bounded code generation, no new allocation/scan on calls. Cold tail return removes argument saves and halves measured integer callee frame. Tagged float ranges separate twice; mixed controls and both rejected runs retained.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|v3 binder requires validated v2, signature and normal-body identity. Null or failed inline guard reaches full v2. Unsupported layouts receive null; unavailable summaries retain prior entries. Live Program/debugger/body/readiness checks unchanged.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Helper declarations document parameters, borrowed lifetime, fixed width and mandatory fallback. README/current-state/results explain compatibility and limitations; no public QPP API.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No format strings, new I/O, secrets or unchecked user-derived indexes. Parameter indexes follow admitted zero/one-argument shape. Non-null limit view is runtime-owned and invocation-local; evidence fields allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug selected matrix, native exhaustion/recovery including foreign registrations, generated guard/tail shape, float edge cases and concurrent readers pass. Old artifacts and independent benchmark oracles match; object inspection limitations explicit.

### Verification

|!Check|!Outcome
|Builds|Release/debug qore and qcc pass
|Selected regressions|156 cases / 13929 assertions, 24 wrappers plus 11 standalones per build
|Guard proof|Null/live address, fresh SP, supported fixed-width layout, full v2 failure return; no unchecked unsupported target
|Native tests|Exhaustion and recovery for four guard modes, main and repeatedly registered foreign thread
|Generated tests|O0/O3, v3 disabled, older helper controls, EH/split, comparison/select float cases; 554 continuation, 978 float and 98 reader assertions per build
|Observer semantics|Existing publication/helper-last ordering, empty markers, policies and stack-reader synchronization unchanged
|Old artifact / oracles|225 matching old-artifact records and 225 independent checks per timed implementation
|Harness|Ten Python tests pass
|Timing integrity|390 jobs / 1950 positive finite samples, unique jobs and independently recomputed medians; no overlapping builds/tests/profiles/docs validation
|Rejected candidate|Two complete runs retained; merge-back helper edge did not improve performance
|Retained benefit|Tagged-float 0.75%/2.15% reductions with separated ranges; other callback evidence less conclusive
|Tradeoffs|AOT text +576, runtime text +2536 bytes; integer callee frame 32 to 16 bytes; helper remains 256 bytes
|Separate profile|Helper/frame remains dominant; no additive or Java-parity claim
|Cross-target scope|ELF64/ELF32 objects inspected; baseline-reproduced manifest filename issue makes CLI exit 1; no Linux runtime claim
|Docs / whitespace|Strict qpp table validation and staged git diff whitespace check

The skill drove explicit live-address ownership and unsupported-layout fallback
review. Native harness fallback is equivalent C++ coverage, not a forced call
of generated LLVM's null edge; that edge has structural coverage. No full-suite
or sanitizer claim. Existing unrelated findings remain documented.

## Deferred scalar context audit (2026-10-07)

Scope: all uncommitted changes after `9e74e1788`: internal C++ runtime helper,
two new integration-test files, design/benchmark prose and sanitized evidence.
Applied audit-changes with module-structure, sandboxing and cooperative-
cancellation guides. No qlib, DataProvider, QPP or JNI changes. Every skill
check is accounted for below.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|Module added to QMOD list in CMakeLists.txt|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|No %include usage (deprecated for modules)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|Copyright 2026 on all new files|Pass|Both new test files and evidence have 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new shipped module or QPP class; generated fixtures are scripts.
|%modern directive present|Pass|New wrapper and both generated scripts use %modern; not applicable to C++ harness.
|Executable permission set (chmod +x)|Pass|New Qore wrapper is executable; C++ source is compiled by that wrapper.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib prepend precedes QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil modules are required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No new production filesystem/network operation or direct I/O in the native harness.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No new production filesystem/network operation or direct I/O in the native harness.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new production filesystem/network operation or direct I/O in the native harness.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|File/TmpDir/glob/subprocess operations are scoped to compiling, linking and running temporary test artifacts.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new production loop. Native 4096-step sweep polls every 10 iterations; stack inspection every 100; remaining loops have at most 20 iterations.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|New test polls use qore_check_cancel; no deprecated API.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Production poll frequency unchanged. Expensive test sweep polls every 10 iterations and tight stack scan every 100.
|No blocking operations without cancellation support|Pass|No new production blocking operation. Existing reader wait retained; native harness joins its bounded, cancellable worker.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider or JNI change.
|Response/output types use private Fields|N/A|No DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete deferred helper with idempotent failure materialization and eager compatibility path. No stub/TODO; the constructor lambda is functional but remains a measured follow-up optimization.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new production allocation. Context is materialized before overflow exception construction, then restored in original order. Stack publication/helper-last lifetime and cache owner are retained; unmaterialized fields are never restored/read. Native test consumes exceptions in holders and releases its prepared owner.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|New pointer and deferred fields belong only to the current thread/invocation. Cache is pinned and cannot be replaced by a pure non-reentrant leaf. Readers use separately initialized immutable diagnostic fields and the unchanged pop/walker handshake.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Only validated, re-emitted scalar callee-v3 bindings enter deferral; zero/one native argument ABI unchanged. Compile-time helper selection uses conditional_t with explicit type_traits include. Native test verifies the typed signature, selected entry and prepared cache.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|No new scan, allocation or metadata copying in successful calls. Callback ranges separate twice with 7.30-10.63% time reduction. Larger 304-byte helper frame and small control regressions are reported; no universal speedup claim.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Full native and IA-64 register-stack overflow branches materialize before raising the original exception. All Program/debugger/body/readiness guards and sink checks remain. Older entries stay eager; actual boundary sweep proves error and recovery paths.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal-only helper documents purity, borrowed ownership and mandatory materialization. No public QPP/API addition needing business examples; README/current-state/results describe safety boundaries, compatibility and measured costs.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, user format string, secret or unbounded index. Limit mutation exists only in an isolated same-thread test sweep and is restored before inspecting errors. Production limit is unchanged. Evidence metadata allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|157 cases / 13957 assertions plus 11 standalones per release/debug build, new boundary sweep and concurrent reader tests pass. Two old artifacts and all independent benchmark checks match; unsupported-runtime and full-suite limitations stated.

### Verification

|!Check|!Outcome
|Final builds|Release/debug qore and qcc pass, including explicit type_traits dependency
|Selected matrix|157 cases / 13957 assertions in 25 wrappers plus 11 standalones per build
|New native boundary test|28 assertions: O0/O3, v3/eager binding, 4096 limit offsets, main plus three foreign registrations, failure diagnostics and recovery; saved baseline passes too
|Deferred-state proof|Only re-emitted pure scalar v3 body; no context reads/writes, allocation or generic reentry before its full guard
|Failure semantics|Native and register-stack guards materialize context before exception construction; existing policy override and restoration order preserved
|Observers|Independent diagnostic frame and empty markers always active; same publication/pop handshake; helper stays last and cache remains pinned
|Old artifacts|225 matching records each for callee-v2 and callee-v3 artifacts
|Independent benchmark checks|225 records per implementation; ten Python harness tests pass
|Timing integrity|195 unique jobs / 975 finite positive samples; medians independently recomputed; no builds/tests/profiles/docs validation during timings
|Benefit|All five callback ranges separate in both runs; 7.30-10.63% less time
|Controls / size|Small control regressions retained; helper frame 256 to 304 bytes, AOT text unchanged, runtime text -6060 bytes
|Separate profile|Frame/helper dominates; out-of-line constructor-selection lambda confirmed, queued as next measured experiment
|Documentation / whitespace|Strict qpp table checks and staged git diff whitespace check

The skill drove forced native-boundary coverage and preservation of failure
materialization/restoration order. Initial test setup did not reach the typed
path; the final native-register aggregate asserts actual admission rather than
accepting generic fallback. No full-suite, sanitizer or Linux runtime claim.
Java parity is not achieved.

## Frame-helper selector audit (2026-10-07)

Scope: one inlining attribute/comment in `lib/JITRuntime.cpp`, current-state
documentation, results and evidence. Applied audit-changes and the previously
read module-structure, sandboxing and cooperative-cancellation guides. All 62
checks are recorded below; no new tests or production behavior are introduced.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Module added to QMOD list in CMakeLists.txt|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No %include usage (deprecated for modules)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Copyright 2026 on all new files|Pass|New measurement evidence carries 2026/MIT metadata.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|%modern directive present|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Executable permission set (chmod +x)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Uses %prepend-module-path  before %requires for in-repo modules|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|No blocking operations without cancellation support|N/A|Inlining attribute and comment only; no new I/O, loop, wait or cancellation point.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Response/output types use private Fields|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Password/secret fields have "sensitive": True|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No bare field/option names in prose — must use backticks|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|One complete inlining change; no TODO, workaround or stub.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No allocation or fallible operation added. Both helper constructors/destructors, failure materialization and restoration order are unchanged.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|No new state. Same invocation-local descriptor pin, publication ordering and pop/reader handshake; concurrent-reader tests pass.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Compile-time conditional helper type is unchanged; ABI, signatures, scalar eligibility and typed call sites unchanged.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Removes an observed wrapper call; both repeats show separated callback ranges and 6.67-8.58% less time. Frame shrinks 32 bytes; runtime text grows 10356 bytes, reported.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|All live admission, stack, sink, debugger, Program and readiness checks and fallback paths remain. Native boundary sweep passes on both builds.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal comment explains the hint; current-state and results document unchanged semantics and measured costs. No public API added.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No module, QPP, DataProvider, JNI or Qore test source change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new indexing, format string, I/O or credentials. Evidence uses allowlisted metadata.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Each build: 157 cases / 13957 assertions and 11 standalones; ten Python tests; two old artifacts and all timed implementations match 225 reference records. All 975 windows validated.

Release/debug qore and qcc builds pass. Selected coverage is 25 wrappers with
157 cases / 13957 assertions plus 11 standalones per build; ten Python tests
pass. Both old artifacts and timed implementations pass 225 record checks.
All 195 unique jobs / 975 positive finite samples and summary medians are
independently validated. Timings are isolated; profiling follows both runs.

The skill preserved all state/failure/observer semantics and required recording
the code-size tradeoff. Strict documentation-table, JSON and staged whitespace
checks pass. No full-suite, sanitizer or Linux runtime claim; Java parity open.

## Scalar-integer callback audit (2026-10-07)

Scope: compiler admission/re-emission in `lib/QoreIRToLLVM.cpp`, two new
executable Qore test files, equivalent Java/Qore/Python benchmark additions,
and documentation/evidence. Applied audit-changes and the module-structure,
sandboxing and cooperative-cancellation guides. No qlib source edit.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Module added to QMOD list in CMakeLists.txt|N/A|No shipped module, QPP class, DataProvider or JNI change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No %include usage (deprecated for modules)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Copyright 2026 on all new files|Pass|Both new executable fixtures and measurement evidence carry 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No shipped module, QPP class, DataProvider or JNI change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No shipped module, QPP class, DataProvider or JNI change.
|%modern directive present|Pass|New wrapper, fixture and existing benchmark use %modern.
|Executable permission set (chmod +x)|Pass|Both new Qore test files are executable.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib prepend precedes QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil modules required.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Compiler admission/emission change introduces no production filesystem or network operation.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|Compiler admission/emission change introduces no production filesystem or network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|Compiler admission/emission change introduces no production filesystem or network operation.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|TmpDir and subprocess calls compile/run isolated test artifacts; benchmark uses existing controlled local build/run workflow.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No added C++ loop, blocking operation or cancellation point; existing live checks retained. New Qore fixture loops are bounded and use normal language cancellation; Java/Python loops belong to benchmark/reference.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No added C++ loop, blocking operation or cancellation point; existing live checks retained. New Qore fixture loops are bounded and use normal language cancellation; Java/Python loops belong to benchmark/reference.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No added C++ loop, blocking operation or cancellation point; existing live checks retained. New Qore fixture loops are bounded and use normal language cancellation; Java/Python loops belong to benchmark/reference.
|No blocking operations without cancellation support|Pass|No added C++ loop, blocking operation or cancellation point; existing live checks retained. New Qore fixture loops are bounded and use normal language cancellation; Java/Python loops belong to benchmark/reference.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No shipped module, QPP class, DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No shipped module, QPP class, DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Response/output types use private Fields|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No shipped module, QPP class, DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No shipped module, QPP class, DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No shipped module, QPP class, DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No shipped module, QPP class, DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No shipped module, QPP class, DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No shipped module, QPP class, DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No shipped module, QPP class, DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No shipped module, QPP class, DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No shipped module, QPP class, DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete scalar-leaf admission and re-emission; no TODO, stub or workaround. CFG-disabled path uses the existing general-expression proof, explicitly tested.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No new Qore allocation or ownership path. Existing LLVM function rollback removes disabled/unavailable re-emitted entries. Native stack failure and context materialization remain unchanged.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only compile-time selection changes; no mutable shared runtime state. Existing cache ownership, Program guards and frame reader handshake remain.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|NativeInt-only scalar kinds with explicit opcode allowlist; checked operand/type emitter; strict signature binders and zero/one-argument ABI unchanged. Two-argument boxed adapter tested separately.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Reuse whole-body summaries that previously suppressed general integer-expression admission. Three newly admitted kernels measured against saved compiler/runtime using identical new sources; controls and code-size costs reported.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Division/modulo/effects stay excluded; unknown scalar kinds fail closed. v1/v2/v3 disable controls, sink handling, Program/debugger/readiness and stack checks unchanged and tested.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal comment, current-state and benchmark README document proof reuse, wrapping and controls. No new public runtime method or format; results distinguish eligibility gains from retained-call overhead.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No shipped module, QPP class, DataProvider or JNI change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new production I/O, format string or unchecked runtime indexing. Existing emitter validates parameter indexes/types; evidence metadata allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Each release/debug build: 158 cases / 15329 assertions in 26 wrappers plus 11 standalones; ten Python tests. New fixture verifies signed wrapping, wide values, comparisons, effects, division errors, generated entries, O0/O3 and fallbacks. All benchmark records independently checked.

### Verification

|!Check|!Outcome
|Release/debug build|qore and qcc pass
|Selected matrix|158 cases / 15329 assertions in 26 wrappers plus 11 standalones per build
|New wrapper|1372 assertions, O0/O3 and eleven AOT configurations, all execution modes and runtime fallback
|Native admission|Generated v1/v2/v3 entries asserted; throwing/effectful functions excluded; two-argument boxed entry tested
|Signed semantics|Full signed width, wide boxed values, arithmetic and composed affine wrapping, six comparisons and branch directions
|Fallback semantics|Division/modulo errors and side effects preserved; disabled CFG summary may use existing general-expression proof
|Compatibility|New/saved runtimes cross-run new/saved compiler artifacts with 270 matching records each; older v2/v3 artifacts each match 225
|Independent reference|270 checks per measured implementation; ten Python tests
|Timing integrity|195 unique jobs / 975 finite positive samples, recomputed medians; no builds/tests/profiles/docs validators during timings
|Benefit|Target ranges separate twice; 91.44-95.13% less time, already-native control ranges overlap
|Code size|Same expanded sources: AOT +916 bytes, runtime +136 bytes; helper frame remains 272 bytes
|Separate sample|Helper remains dominant; next experiment targets cold-path outlining, not removed checks
|Final checks|Strict qpp tables, valid JSON and staged whitespace check

The skill required explicit native admission evidence, opcode exclusion and
signed-edge validation. No ABI/metadata change, sanitizer, full-suite or Linux
runtime claim. Java parity remains open.

## Rejected cold-path audit (2026-10-07)

Final scope: new executable `AOTNativeMarkerGrowth.qtest`, rejected-experiment
results and JSON evidence. The C++ outlining experiment failed the performance
gate and was completely reverted. Applied audit-changes with module-structure,
sandboxing and cooperative-cancellation guides; no qlib edit.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No shipped module, QPP, DataProvider or JNI change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No shipped module, QPP, DataProvider or JNI change.
|Module added to QMOD list in CMakeLists.txt|N/A|No shipped module, QPP, DataProvider or JNI change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No shipped module, QPP, DataProvider or JNI change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No shipped module, QPP, DataProvider or JNI change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No shipped module, QPP, DataProvider or JNI change.
|No %include usage (deprecated for modules)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Copyright 2026 on all new files|Pass|New executable wrapper and JSON evidence carry 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No shipped module, QPP, DataProvider or JNI change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No shipped module, QPP, DataProvider or JNI change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No shipped module, QPP, DataProvider or JNI change.
|%modern directive present|Pass|Wrapper and generated fixture use %modern.
|Executable permission set (chmod +x)|Pass|New qtest is executable.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Repository qlib prepend precedes QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil dependencies.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|Production experiment reverted; no committed C++ I/O change.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|Production experiment reverted; no committed C++ I/O change.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|Production experiment reverted; no committed C++ I/O change.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|File/TmpDir/subprocess use is scoped to generating, compiling and executing temporary test artifacts.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|N/A|No committed C++ change. Test loops use Qore cancellation; largest individual loop has 71 iterations. Existing interruptible subprocess execution.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|N/A|No committed C++ change. Test loops use Qore cancellation; largest individual loop has 71 iterations. Existing interruptible subprocess execution.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No committed C++ change. Test loops use Qore cancellation; largest individual loop has 71 iterations. Existing interruptible subprocess execution.
|No blocking operations without cancellation support|N/A|No committed C++ change. Test loops use Qore cancellation; largest individual loop has 71 iterations. Existing interruptible subprocess execution.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No shipped module, QPP, DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No shipped module, QPP, DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No shipped module, QPP, DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No shipped module, QPP, DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No shipped module, QPP, DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Response/output types use private Fields|N/A|No shipped module, QPP, DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No shipped module, QPP, DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No shipped module, QPP, DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No shipped module, QPP, DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No shipped module, QPP, DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No shipped module, QPP, DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No shipped module, QPP, DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No shipped module, QPP, DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No shipped module, QPP, DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No shipped module, QPP, DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No shipped module, QPP, DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No shipped module, QPP, DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No shipped module, QPP, DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No shipped module, QPP, DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No shipped module, QPP, DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No shipped module, QPP, DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No shipped module, QPP, DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|No production workaround retained. Rejected optimization fully reverted; complete new regression and evidence only.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Production ownership/error paths unchanged after revert. Test uses scoped TmpDir and checks every subprocess status.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|No committed shared runtime state change. Prior frame publication, reader synchronization and variable-marker lifetime retained.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Typed code<int(int)> and typed two-argument outer callable force intended generic-to-native boundary. Strongly typed local strings stay live.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Experiment fails repeated speed-benefit gate and regresses callee-v2 control 4.70-5.61%; reverted rather than accepted on code-size evidence.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Nonzero child status and incorrect output fail the wrapper. Runtime failures, fallback behavior and stack checks are unchanged after revert.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Results/evidence record rejected diff, complete samples, control regression, code size and next proof obligation. No public API added.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No shipped module, QPP, DataProvider or JNI change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Arguments are shell-quoted, generated sources are fixed test data, artifacts scoped to TmpDir. No credentials; measurement metadata allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Candidate release/debug matrix and saved-baseline final boundary test pass. Debugger proves growth under padded53; corrected initial fixture did not exercise that path. Restored builds revalidated before commit.

### Verification

|!Check|!Outcome
|Candidate matrix|159 cases / 15369 assertions in 27 wrappers plus 11 standalones per release/debug build; ten Python tests
|Final boundary fixture|40 assertions each on candidate release/debug and saved baseline; AST/IR/JIT/tiered, O0/O3, v3/eager and runtime fallback
|Actual growth path|Debugger stopped in outlined growth under native call from padded53; then fixture completed successfully
|Coverage correction|Initial direct callers used native locals and did not hit growth; generic outer invocation forces TLS locals in final fixture
|Old artifacts / reference|225/270 old v2/v3 records match; 270 independent checks per measured implementation
|Timing integrity|240 unique jobs / 1200 finite positive samples; independently recomputed medians; no tests/builds/profile/docs checks during timings
|Performance gate|Failed: repeat callback ranges overlap; callee-v2 wide/float regress 4.70%/5.61% with separated ranges
|Resolution|Rejected C++ diff reverted; evidence and regression retained
|Final source/build checks|No production diff; release/debug restored builds and final regression checked
|Documentation / JSON|Strict qpp tables, valid evidence JSON and staged whitespace check

The skill caused rejection of the unproven optimization, retaining the
measurements and strengthening actual-path regression coverage. No full-suite,
sanitizer or Linux runtime claim; Java parity remains open.


## Rejected marker-deferral audit (2026-10-07)

Final scope: native host regression extension, rejected-experiment results and
JSON evidence. Both production variants and their architecture/README edits
are reverted. Applied audit-changes with the module-structure, sandboxing and
cooperative-cancellation guides; no qlib edit.

|!Check|!Status|!Findings
|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No shipped module, QPP, DataProvider or JNI change.
|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No shipped module, QPP, DataProvider or JNI change.
|Module added to QMOD list in CMakeLists.txt|N/A|No shipped module, QPP, DataProvider or JNI change.
|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No shipped module, QPP, DataProvider or JNI change.
|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No shipped module, QPP, DataProvider or JNI change.
|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No shipped module, QPP, DataProvider or JNI change.
|No %include usage (deprecated for modules)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Copyright 2026 on all new files|Pass|Existing native fixture and new JSON evidence carry 2026/MIT notices.
|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No shipped module, QPP, DataProvider or JNI change.
|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No shipped module, QPP, DataProvider or JNI change.
|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No shipped module, QPP, DataProvider or JNI change.
|%modern directive present|Pass|Generated caller source uses %modern; existing wrapper also uses %modern.
|Executable permission set (chmod +x)|Pass|Existing NativeLeafContext.qtest wrapper is executable; C++ host source is compiled, not executed as a script.
|Uses %prepend-module-path  before %requires for in-repo modules|Pass|Existing wrapper prepends repository qlib before QUnit/FsUtil requirements.
|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered dependencies; no external module added.
|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No new C++ filesystem operation; runtime experiment is fully reverted.
|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No new C++ network operation.
|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new filesystem/network operation requiring a sandbox helper.
|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No Qore module change; existing wrapper uses scoped temporary compilation artifacts.
|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|256-step native limit sweep polls qore_check_cancel every ten iterations; arbitrary stack inspection polls every 100 entries. Source-generation loops are bounded to 71.
|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|New polling uses qore_check_cancel, not the deprecated I/O helper.
|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Expensive sweep/caller checks every ten; stack-entry inspection every 100.
|No blocking operations without cancellation support|Pass|No new blocking operation; existing internal parse lock is infrastructure, not a cancellation point.
|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No shipped module, QPP, DataProvider or JNI change.
|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No shipped module, QPP, DataProvider or JNI change.
|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No shipped module, QPP, DataProvider or JNI change.
|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No shipped module, QPP, DataProvider or JNI change.
|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No shipped module, QPP, DataProvider or JNI change.
|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Response/output types use private Fields|N/A|No shipped module, QPP, DataProvider or JNI change.
|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No shipped module, QPP, DataProvider or JNI change.
|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No shipped module, QPP, DataProvider or JNI change.
|Password/secret fields have "sensitive": True|N/A|No shipped module, QPP, DataProvider or JNI change.
|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No shipped module, QPP, DataProvider or JNI change.
|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No shipped module, QPP, DataProvider or JNI change.
|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No shipped module, QPP, DataProvider or JNI change.
|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No shipped module, QPP, DataProvider or JNI change.
|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No shipped module, QPP, DataProvider or JNI change.
|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No shipped module, QPP, DataProvider or JNI change.
|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No shipped module, QPP, DataProvider or JNI change.
|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No shipped module, QPP, DataProvider or JNI change.
|No bare field/option names in prose — must use backticks|N/A|No shipped module, QPP, DataProvider or JNI change.
|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No shipped module, QPP, DataProvider or JNI change.
|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No shipped module, QPP, DataProvider or JNI change.
|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No shipped module, QPP, DataProvider or JNI change.
|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No shipped module, QPP, DataProvider or JNI change.
|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No shipped module, QPP, DataProvider or JNI change.
|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Both failed runtime variants completely reverted; retained native host regression and negative evidence are complete.
|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Test uses QoreProgramHelper, ValueHolder and ReferenceHolder ownership; checks parse/call/list errors. Runtime partial-constructor/marker rollback changes rejected with the experiment.
|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Each main/foreign invocation owns its Program, descriptor, limit sweep and external-function state. Limit restored before inspection. No committed runtime shared-state change.
|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Strongly typed code<int()> captures and bigInt builtin signature; typed C++ descriptor and static_cast; QEM_AST enum selects actual TLS callers.
|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Performance rejection enforced: initial float regression 41.07-59.92%, repeat 50.92%; cleanup variant still 19.83%. Bounded quadratic test-source size intentionally covers every occupancy.
|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Actual binding/cache admission asserted; overflow error/trace, caller context, every owned local/capture and recovery result validated. Fixture parse errors reported explicitly.
|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Results record both rejected diffs, full samples, proof limitations and next ABI hypothesis. No public API added.
|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method change.
|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Generated names and source are bounded fixed test data; stack entries and sinks checked; no user-controlled format strings or credentials. Evidence metadata allowlisted.
|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Saved-baseline and restored release/debug final fixture checks; original candidate targeted matrix; actual local/closure growth debugger proofs. No allocation-failure injection, full-suite, sanitizer or Linux claim.

### Verification

|!Check|!Outcome
|Original candidate matrix|159 cases / 15369 assertions in 27 wrappers plus 11 standalones per release/debug build; ten Python tests; before final host-fixture strengthening
|Final native fixture|Existing wrapper's 28 assertions plus native checks: O0/O3, v3/eager, main and three foreign-thread registrations, saved baseline and restored release/debug
|Actual growth|Local and closure stacks separately grow from full block position 64 to new position 1; both frame counts rise before exception construction; debugger runs finish successfully
|Fixture corrections|Typed closure syntax, explicit parse lock and AST caller execution corrected; initial native callers did not exercise TLS growth
|Compatibility / reference|Old v2/v3 artifacts match 225/270 records; each measured implementation passes 270 independent checks
|Timing integrity|160 unique jobs / 800 finite positive samples; independently recomputed medians; no tests/builds/profiles/docs validators during timings
|Performance gate|Failed twice: significant float regressions despite integer improvements; both runtime variants reverted
|Final source/build checks|No production diff; release/debug restored builds and final native regression checked
|Documentation / JSON|Strict qpp tables, valid evidence JSON and staged whitespace check

The skill requires retaining the negative measurements and rejecting the
mixed-result optimization. It also requires explicit actual-path coverage,
not merely successful output. No full-suite, sanitizer, allocation-failure
injection or Linux runtime claim. The Java-speed goal remains active.

## Rejected register-return audit (2026-10-07)

Applied audit-changes to the uncommitted scope against `61345f823` on
`feature/performance`. The main agent read the complete skill and required
module-structure, sandboxing and cooperative-cancellation guides. All 62
checks below are explicitly assessed. This is not a full-suite/sanitizer audit.

Retained scope: `AOTFunctionRefScalarABI.qr/.qtest`, benchmark results and
sanitized JSON evidence. All helper-v4 declarations, exports, registry entries,
target-specific LLVM rewriting and prototype-only native-host changes are
reverted. No include/lib diff remains; restored release/debug builds pass.

The skill's actual-path review found that unused-input and boolean targets
did not receive native adapters. The final fixture checks four admitted
integer/float zero/identity shapes and eight generic shapes, while exercising
all twelve scalar helper signatures. Earlier claims must not interpret twelve
helper signatures as twelve admitted native callee shapes. Test-only trim/
newline and overbroad native-admission expectations were corrected and rerun.

|!Check|!Status|!Evidence
|1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QMOD, .qc or QPP class.
|2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QMOD, .qc or QPP class.
|3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|4. Module added to QMOD list in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QMOD, .qc or QPP class.
|6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QMOD, .qc or QPP class.
|7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QMOD, .qc or QPP class.
|8. No %include usage (deprecated for modules)|Pass|No added source uses %include.
|9. Copyright 2026 on all new files|Pass|Both new executable fixtures and JSON evidence carry 2026 copyright and MIT notices.
|10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QMOD, .qc or QPP class.
|11. No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QMOD, .qc or QPP class.
|12. ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QMOD, .qc or QPP class.
|13. %modern directive present|Pass|Both new Qore files use %modern.
|14. Executable permission set (chmod +x)|Pass|Both .qr/.qtest files have executable permission.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends in-repo qlib before requiring QUnit/FsUtil.
|16. External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|Only QUnit and FsUtil, shipped with Qore, are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|20. No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|FsUtil TmpDir owns test artifacts; trusted compiler/executable children use quoted paths and fixed control prefixes under %no-child-restrictions.
|21. All for/while loops that could iterate >100 times have qore_check_cancel() checks|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|All C++ changes reverted; no retained C++ I/O or loop.
|24. No blocking operations without cancellation support|Pass|No production blocking operation added; finite test subprocesses and existing harness timeouts only.
|25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider/app/action/type/JNI change.
|26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider/app/action/type/JNI change.
|27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider/app/action/type/JNI change.
|28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider/app/action/type/JNI change.
|29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider/app/action/type/JNI change.
|30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider/app/action/type/JNI change.
|31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider/app/action/type/JNI change.
|32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider/app/action/type/JNI change.
|33. Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider/app/action/type/JNI change.
|34. Response/output types use private Fields|N/A|No DataProvider/app/action/type/JNI change.
|35. Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider/app/action/type/JNI change.
|36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/app/action/type/JNI change.
|37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/app/action/type/JNI change.
|38. Password/secret fields have "sensitive": True|N/A|No DataProvider/app/action/type/JNI change.
|39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider/app/action/type/JNI change.
|40. App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider/app/action/type/JNI change.
|41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/app/action/type/JNI change.
|42. display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/app/action/type/JNI change.
|43. short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/app/action/type/JNI change.
|44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider/app/action/type/JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/app/action/type/JNI change.
|46. No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider/app/action/type/JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/app/action/type/JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/app/action/type/JNI change.
|49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/app/action/type/JNI change.
|50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider/app/action/type/JNI change.
|51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/app/action/type/JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/app/action/type/JNI change.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Both unsuccessful runtime/compiler prototypes completely reverted; no partial ABI or feature flag remains.
|54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|No retained C++ allocation; TmpDir owns fixtures and every compiler/child status is asserted.
|55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|N/A|No retained runtime mutation or shared mutable state; wrapper processes are sequential.
|56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|All callable parameters use code<return(args)>; auto-return providers intentionally test checked generic fallback.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|No claimed speedup: first float regression separates, all HFA ranges overlap. Both variants reverted; 80 jobs / 400 samples preserved.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Compiler/run failures, exact scalar results and emitted helper/callee selection are asserted. Fixture admission assumptions were corrected against baseline IR.
|59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|No public API retained. Fixture comments, results and JSON distinguish helper ABI coverage from four admitted native shapes; no cross-platform runtime or Java parity claim.
|60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method added or changed.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled format strings or raw memory retained. Shell paths quoted; metadata allowlist omits environment and user paths.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Final fixture passes saved baseline and restored release/debug: 530 assertions each. NativeLeafContext restored release/debug: 28 each. Ten Python tests; 270 reference checks per timed implementation.

### Validation record

|!Gate|!Result
|Restored release/debug builds|qore/qcc pass; production source diff empty
|Final scalar signature fixture|530 assertions per saved baseline/restored release/restored debug, all pass
|Restored NativeLeafContext|28 wrapper assertions per release/debug plus native main/foreign-thread overflow checks
|Prototype focused validation|Integer carrier: release continuation 554, release ABI 472, debug native v3/v4 host 40; FP carrier: ABI 472 per build, release continuation 592
|Reference checks|270 per timed implementation; ten Python harness unit tests pass
|Timing integrity|80 unique jobs / 400 finite positive samples; five fork means and medians independently recomputed; no builds/tests/profiles/docs validators overlap timings
|ABI evidence|Native arm64 Darwin bridge passes for integer pair and homogeneous double pair; foreign targets have IR/object checks only
|Performance gate|Rejected both variants: integer carrier float +6.37%; HFA has no separated before/after ranges or useful overall gain
|Final source/metadata|Both rejected production diffs retained with SHA-256; runtime/executable hashes and code sizes recorded; environment not committed
|Documentation|Strict qpp table validation, JSON parsing and staged whitespace check pass

No full selected matrix, full-suite, sanitizer, allocation-failure injection
or Linux runtime claim. No fresh Java measurement or Java-level speed claim.
The goal remains active. Next: isolate redundant middle stack-guard work in
proven pure callee-v3 execution, retaining both enclosing guard boundaries and
all diagnostic publication/reader synchronization.
## Middle stack-guard audit (2026-10-07)

Applied audit-changes against `089aaf452` on `feature/performance`.
The complete skill and required module-structure, sandboxing and cooperative-
cancellation guides were read. Scope: two runtime C++ files, one C++ host test,
one Qore test wrapper, design/README/release notes, results and raw JSON evidence.
All 62 checks follow; no DataProvider, QPP, module registration or qlib change.

The only omitted guard was in deferred pure-leaf frame publication, between
the prepared helper's entry guard and callee-v3's live guard. Publication,
local/closure markers, reader synchronization, mutable admission/readiness
checks and eager legacy guards remain. The float test extends the existing
integer boundary sweep and asserts actual native-entry selection, both overflow
boundaries, trace visibility and recovery on main and registered foreign threads.

|!Check|!Status|!Evidence
|1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QMOD, .qc or QPP class.
|2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QMOD, .qc or QPP class.
|3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|4. Module added to QMOD list in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QMOD, .qc or QPP class.
|6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QMOD, .qc or QPP class.
|7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QMOD, .qc or QPP class.
|8. No %include usage (deprecated for modules)|Pass|No added %include.
|9. Copyright 2026 on all new files|Pass|New JSON evidence carries 2026 copyright and MIT notice; existing test headers retained.
|10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QMOD, .qc or QPP class.
|11. No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QMOD, .qc or QPP class.
|12. ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QMOD, .qc or QPP class.
|13. %modern directive present|Pass|NativeLeafContext wrapper and emitted provider use %modern.
|14. Executable permission set (chmod +x)|Pass|Modified .qtest remains executable (100755); C++ host is compiled, not a script.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|In-repo qlib prepended before QUnit/FsUtil requirements.
|16. External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|Only bundled QUnit/FsUtil dependencies.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No new C++ filesystem operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No new network operation.
|19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No C++ external-resource operation added.
|20. No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|Existing FsUtil TmpDir owns trusted compiler/test artifacts; emitted source extended only. No production Qore I/O change.
|21. All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|New 4096-step guard sweep polls qore_check_cancel; trace walk also polls.
|22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|Only qore_check_cancel used.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|Expensive overflow sweep polls every 10, trace walk every 100.
|24. No blocking operations without cancellation support|Pass|No blocking operation added; unchanged reader-lock handshake protects popped frames.
|25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider/app/action/type/JNI change.
|26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider/app/action/type/JNI change.
|27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider/app/action/type/JNI change.
|28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider/app/action/type/JNI change.
|29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider/app/action/type/JNI change.
|30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider/app/action/type/JNI change.
|31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider/app/action/type/JNI change.
|32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider/app/action/type/JNI change.
|33. Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider/app/action/type/JNI change.
|34. Response/output types use private Fields|N/A|No DataProvider/app/action/type/JNI change.
|35. Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider/app/action/type/JNI change.
|36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/app/action/type/JNI change.
|37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/app/action/type/JNI change.
|38. Password/secret fields have "sensitive": True|N/A|No DataProvider/app/action/type/JNI change.
|39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider/app/action/type/JNI change.
|40. App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider/app/action/type/JNI change.
|41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/app/action/type/JNI change.
|42. display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/app/action/type/JNI change.
|43. short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/app/action/type/JNI change.
|44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider/app/action/type/JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/app/action/type/JNI change.
|46. No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider/app/action/type/JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/app/action/type/JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/app/action/type/JNI change.
|49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/app/action/type/JNI change.
|50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider/app/action/type/JNI change.
|51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/app/action/type/JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/app/action/type/JNI change.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete scoped change, no TODO/stub, no benchmark-specific runtime condition or new flag.
|54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|ValueHolder and ReferenceHolder own reference/exception data; sinks checked; cache owner released. Existing frame RAII/materialization and exception propagation retained.
|55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only owning-thread context and live limits touched. Release publication, acquire reader check and lock-backed pop synchronization unchanged.
|56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Strong code<float(float)> signature and exact typed entry 10; static_cast and memcpy bit transport match existing host fixture.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Repeated integer callback gain, runtime text -1020 bytes and no added asymptotic cost. Float benefit not uniform; hash medians +3.31%/+1.39% with overlapping ranges retained as unresolved control risk, not dismissed.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Helper and callee guards remain; float test restores real stack limit before exception inspection, distinguishes early/leaf overflow and verifies recovery.
|59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal constructor contract, current-state design and README describe both retained boundaries. Release note included; no new public API.
|60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method changed.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user format strings, buffer indexing or credentials introduced. Evidence metadata allowlist omits environment and local paths.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|39 selected files pass per release/debug (160 cases/15899 assertions plus standalones); expanded float host passes baseline/release/debug. Old v2/v3 artifacts match 225/270 records; ten Python tests and 270 reference checks per timed implementation.

### Validation record

|!Gate|!Result
|Release/debug builds|Both qore/qcc targets pass; measured library hashes retained in JSON
|Selected matrix|39 files per build: 28 QUnit wrappers, 160 cases / 15899 assertions, 11 standalone files; all pass
|Final extended float fixture|After the matrix, expanded NativeLeafContext passes saved baseline/release/debug: 28 wrapper assertions each plus native assertions
|Compatibility|Old callee-v2/v3 binaries match original 225/270 reference records
|Benchmark harness|Ten Python unit tests pass; 270 independent checks per timed implementation
|Timing integrity|358 unique jobs / 1790 finite positive samples; medians independently recomputed; no builds/tests/profiles/validators overlap timings
|Code shape|Helper frames stay 272 bytes; AOT text 91320 bytes unchanged; runtime text shrinks 1020 bytes
|Performance|Retain repeated integer-callback improvement; no uniform float speedup, no Java parity, hash-control uncertainty explicitly recorded
|Documentation/evidence|Strict qpp tables, JSON parsing, diff hash and staged whitespace validation

No full-suite, sanitizer, allocation-failure injection or Linux runtime claim.
The audit skill led to extending actual float overflow-boundary coverage and
retaining the unfavorable controls in the decision record. The goal remains
active; next isolate repeated RuntimeConfig-to-ThreadData pointer loads without
snapshotting the current Program, debugger/body readiness or stack limits.
## Rejected thread-view audit (2026-10-07)

Applied audit-changes against `3af96b157` on `feature/performance`.
The main agent read the complete skill and its required module-structure,
sandboxing and cooperative-cancellation guides. Final scope is benchmark results,
sanitized evidence and this audit. Both runtime prototypes are fully reverted;
no include/lib/test/qlib diff remains. All 62 checks are assessed below.

|!Check|!Status|!Evidence
|1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QMOD, .qc or QPP class.
|2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module, QMOD, .qc or QPP class.
|3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|4. Module added to QMOD list in CMakeLists.txt|N/A|No new module, QMOD, .qc or QPP class.
|5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QMOD, .qc or QPP class.
|6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QMOD, .qc or QPP class.
|7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QMOD, .qc or QPP class.
|8. No %include usage (deprecated for modules)|Pass|No retained source/include change.
|9. Copyright 2026 on all new files|Pass|New sanitized JSON has 2026 copyright and MIT notice.
|10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QMOD, .qc or QPP class.
|11. No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QMOD, .qc or QPP class.
|12. ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QMOD, .qc or QPP class.
|13. %modern directive present|N/A|No retained test source change.
|14. Executable permission set (chmod +x)|N/A|No retained test source change.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|No retained test source change.
|16. External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No retained test source change.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|N/A|No retained C++ or Qore source/I/O/loop change.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|N/A|No retained C++ or Qore source/I/O/loop change.
|19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No retained C++ or Qore source/I/O/loop change.
|20. No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No retained C++ or Qore source/I/O/loop change.
|21. All for/while loops that could iterate >100 times have qore_check_cancel() checks|N/A|No retained C++ or Qore source/I/O/loop change.
|22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|N/A|No retained C++ or Qore source/I/O/loop change.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No retained C++ or Qore source/I/O/loop change.
|24. No blocking operations without cancellation support|Pass|No retained runtime blocking operation; benchmark runs are bounded and isolated.
|25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider/app/action/type/JNI change.
|26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider/app/action/type/JNI change.
|27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider/app/action/type/JNI change.
|28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider/app/action/type/JNI change.
|29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider/app/action/type/JNI change.
|30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider/app/action/type/JNI change.
|31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider/app/action/type/JNI change.
|32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider/app/action/type/JNI change.
|33. Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider/app/action/type/JNI change.
|34. Response/output types use private Fields|N/A|No DataProvider/app/action/type/JNI change.
|35. Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider/app/action/type/JNI change.
|36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider/app/action/type/JNI change.
|37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider/app/action/type/JNI change.
|38. Password/secret fields have "sensitive": True|N/A|No DataProvider/app/action/type/JNI change.
|39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider/app/action/type/JNI change.
|40. App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider/app/action/type/JNI change.
|41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider/app/action/type/JNI change.
|42. display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider/app/action/type/JNI change.
|43. short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider/app/action/type/JNI change.
|44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider/app/action/type/JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider/app/action/type/JNI change.
|46. No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider/app/action/type/JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider/app/action/type/JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider/app/action/type/JNI change.
|49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider/app/action/type/JNI change.
|50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider/app/action/type/JNI change.
|51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider/app/action/type/JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider/app/action/type/JNI change.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Both prototypes completely reverted; no unused overload, accessor or feature flag remains.
|54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|N/A|No retained C++ allocation or ownership change.
|55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|N/A|No retained runtime state or synchronization change.
|56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|N/A|No retained language/API typing change; benchmark data uses existing schema.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Rejected mixed broad pilot and negligible narrow repeat. All unfavorable medians/outliers retained; no claim of universal regression or gain.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|120 unique jobs, 600 finite positive samples and all medians independently validated. Corrected initial syntax-command NDEBUG mismatch and reran successfully.
|59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Results distinguish medians from isolated call latency, overlapping ranges from equivalence, and syntax checks from a full split-runtime build. No public API retained.
|60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method changed.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Evidence metadata allowlist excludes environment/private paths; no credentials or user-controlled format strings.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|270 reference checks per timed implementation. Both variants pass 1213 focused assertions per release/debug; restored build/test results below. No compiler/semantic source retained.

### Validation record

|!Gate|!Result
|Broad candidate|Release/debug builds; five focused suites, 1213 assertions per build; both implementation units pass standalone release syntax checks
|Narrow candidate|Same builds/focused suites and standalone syntax checks pass
|Initial syntax command|Broad ad-hoc compile omitted DNDEBUG against release headers; corrected flags resolve all eight assertion-related errors
|Native boundary coverage|Integer/float early and callee overflow, actual adapter selection, recovery, main/foreign-thread registration and concurrent stack readers
|Timing integrity|120 unique jobs / 600 positive finite samples; independently recomputed medians; no build/test/profile/validator overlaps timings
|Correctness oracle|270 independent records per measured implementation
|Performance gate|Both rejected: broad mixed float medians; narrow repeat mixed sub-1% medians, all ranges overlap; no outlier discarded
|Production rollback|All three runtime files restored to HEAD; final build/test verification recorded below
|Evidence|Both exact production diffs, hashes and sanitized metadata retained; no fresh Java timing/parity claim

The skill's performance gate kept both unconvincing runtime variants out of
the branch. The earlier committed middle-guard optimization remains intact.
No full selected matrix, full suite, sanitizer, allocation-failure injection or
Linux runtime claim for these rejected prototypes. The goal remains active.

Final verification: restored release/debug qore/qcc builds pass, and all five
focused suites pass again (1213 assertions per build). Runtime text returns
to 13875264 bytes; fresh restored binary hashes are recorded separately from
the saved baseline. Ten Python harness tests, strict qpp tables, JSON integrity/
exact diff-hash validation and staged whitespace checks pass. Production,
compiler and test source diff is empty.

## Prepared markers and signatures audit (2026-10-07)

Scope: uncommitted changes against `cb1abb16e`: five internal C++ runtime
files, native C++ test fixture, Python benchmark runner/tests, README, design,
release notes, results, this audit and sanitized evidence. No Qore module,
QPP class, DataProvider or JNI source changes. Reviewed using the complete
audit-changes skill and its module-structure, sandboxing and cooperative-
cancellation guides. All 62 checks are reported below, including N/A items.

|!Check|!Status|!Findings
|1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QMOD, .qc or QPP class.
|2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module; native runtime release note updated separately.
|3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No build target/module registration change.
|4. Module added to QMOD list in CMakeLists.txt|N/A|No QMOD change.
|5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No Qore module source change.
|6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No Qore module source change.
|7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No separated .qc source change.
|8. No %include usage (deprecated for modules)|Pass|No deprecated Qore %include added.
|9. Copyright 2026 on all new files|Pass|New sanitized evidence and embedded diagnostic script carry 2026 copyright and MIT notices.
|10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new Qore module.
|11. No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new Qore module.
|12. ns=Qore::XX matches the QoreNamespace constructor path|N/A|No QPP class.
|13. %modern directive present|N/A|Changed native test is a C++ fixture; its unchanged Qore wrapper has %modern and in-repo module setup.
|14. Executable permission set (chmod +x)|Pass|Python test_runner.py retains executable mode 100755; no new executable test file.
|15. Uses %prepend-module-path  before %requires for in-repo modules|N/A|No changed Qore test/module dependencies.
|16. External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No changed Qore test/module dependencies.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Five runtime files and the native test add no filesystem operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No runtime network operation added.
|19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operation requiring a sandbox helper.
|20. No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No Qore source change; Python harness uses bounded local subprocess/file operations.
|21. All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|New native test loop has exactly two invalid values. No unbounded runtime loop added; existing cancellation polls remain.
|22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated interrupt API introduced; existing cancellation checks remain.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No cancellation checkpoint/frequency changes.
|24. No blocking operations without cancellation support|Pass|Existing diagnostic-reader handshake remains intact; no new runtime blocking operation. Harness subprocess timeout retained.
|25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|33. Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|34. Response/output types use private Fields|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|35. Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|38. Password/secret fields have "sensitive": True|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|40. App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|42. display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|43. short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|46. No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, action, typed data-provider class or JNI change.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete policy-specific RAII and prepared-only accessors; no unused constructor, TODO or stub. Rejected variants survive only as evidence; no benchmark-specific runtime dispatch.
|54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Closure frame count increments only after successful insertion; a throwing closure push rolls back the local marker. Enum transition commits materialization after nonthrowing saved-state copies. Both paths unpublish/wait before owner or frame destruction; sinks/guards unchanged.
|55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Helper/state/variable markers are invocation/thread-local; borrowed descriptors remain owned and immutable while published. Per-call publication and pop/walker synchronization unchanged. Same-thread variable inspection cannot run inside the proven pure leaf.
|56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Three enum states and compile-time native result policy. Signature assertions are restricted to validated, owned, fixed-signature slots; generic checks and boxed fallback remain. No exported native helper/cache layout change.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Final integer callbacks improve 14.6–16.8% across primary/repeat, with separated ranges. Rejected float variants and earlier 3% eager slowdown are retained; combined eager integer medians overlap baseline. General-control medians remain mixed/slower; no equivalence or Java-parity claim.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Reuse flag requires a compatible baseline and AOT mode; default paired compilation preserved. Both library selections run complete reference checks before timing; errors abort. Raw sample counts and medians independently validated.
|59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal wrapper/accessors document parameters, lifetime and allocation behavior. Design, README and release notes distinguish eager/deferred markers and immutable/live checks. No new public language/QPP method.
|60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method change.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new format string, buffer, credentials or I/O attack surface. Sanitized metadata excludes environment/private absolute paths; bounded CLI inputs and temporary output directories retained.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Final release/debug matrices each pass 39 files, 160 QUnit cases/15899 assertions plus 11 standalones. Cold/populated signature/Nothing misses and reuse pass original/marker baselines and candidate. Old v2/v3 records225/270, 11Python tests and standalone syntax checks pass.

### Validation record

|!Gate|!Result
|Final production identity|Five-file core diff ca54440635ca977c2e79ef1d919d5a05520b46a899332d1ce584c64a26763cab; test diff 646ad91548fc5b952a89e0032906e613be26154df00d5658177cf5ffb922d421
|Final builds|Release/debug qore/qcc targets pass; final library hashes recorded in evidence
|Selected runtime matrix|39 files per build, including 28 QUnit wrappers: 160 cases / 15899 assertions plus 11 standalones
|Focused semantics|Native overflow/recovery, main/foreign threads, integer/float eager and inline providers, marker growth, concurrent readers, debugger mutation, callback replacement, wrong signatures, NOTHING and valid cache reuse
|Baseline native fixture|Extended admission/overflow fixture passes saved original and marker-only candidate libraries
|Artifact compatibility|Old callee-v2/v3 artifacts produce byte-identical 225/270 reference records
|Compilation coverage|Standalone release syntax checks for JITRuntime.cpp and thread.cpp pass; not a full split-runtime build
|Benchmark harness|11 Python tests pass, including single/paired/reused AOT modes and invalid reuse options; live same-artifact integration passed
|Measurement integrity|1136 standard jobs / 5680 samples plus 20 crossed-artifact jobs / 100 samples; all positive/finite, unique, medians recomputed
|Isolation|No build, test, profiler or documentation validator overlaps timing; sampling profiles run separately
|Performance|Repeated 14.6–16.8% integer callback benefit; mixed float/control medians; final eager integer baseline ranges overlap, not proof of equality
|Size|Runtime text 13875264 → 13872648 bytes; both inspected scalar helper frames remain 272 bytes
|Documentation|README/design/release/results/audit strict Qore tables; no Markdown separator tables; diff whitespace clean
|Limitations|No full-suite, sanitizer, allocation-failure injection, Linux runtime or Java-parity claim

The skill's performance and documentation checks led to rejection of the
all-result and shared-cleanup variants, explicit investigation of the earlier
eager regression, and correction of stale always-eager-marker/fallback wording.
All rejected/provisional variants and outliers remain in the evidence. Marker
allocation rollback and enum-state/lifetime reasoning are reviewed and exercised
through normal marker growth/overflow; injected allocation failure was not run.

The accepted scope is the combined implementation, not marker deferral alone.
General polymorphic/hash control medians remain slightly slower in some runs;
overlapping ranges do not rule out indirect code-layout effects or regression.
The goal remains active: repeated integer wins are not Java parity.

## AOT method context audit (2026-10-08)

Scope against `870bdce9d`: one C++ runtime implementation, one existing Qore
test, README/design/release documentation, results, this report and sanitized
evidence. Applied the complete audit-changes skill with the module-structure,
sandboxing and cooperative-cancellation guides; DataProvider/JNI sections are
reported individually as N/A. No Qore library module source was edited.

|!Check|!Status|!Findings
|1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new repository module.
|2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|N/A|No new module; runtime release note updated separately.
|3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new build target or module registration.
|4. Module added to QMOD list in CMakeLists.txt|N/A|No QMOD change.
|5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new repository .qm.
|6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No module source change; generated test module uses %modern.
|7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No separated .qc source change.
|8. No %include usage (deprecated for modules)|Pass|No deprecated %include added.
|9. Copyright 2026 on all new files|Pass|New sanitized evidence carries 2026 copyright and MIT notice.
|10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new repository module.
|11. No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new repository module.
|12. ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new QPP class.
|13. %modern directive present|Pass|AOTDotEvalOverload.qtest and its generated module/scripts retain %modern.
|14. Executable permission set (chmod +x)|Pass|Existing Qore test retains executable mode.
|15. Uses %prepend-module-path  before %requires for in-repo modules|Pass|Changed test prepends the repository qlib path before requiring QUnit/FsUtil.
|16. External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil are required.
|17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Changed C++ method path adds no filesystem operation.
|18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No runtime network operation added.
|19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operation requiring sandbox helpers.
|20. No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|File/mkdir/backquote operations belong to the existing isolated TmpDir test harness; fixed test sources and temporary compiled artifacts only.
|21. All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop added. New test loops are bounded to eight outer and two inner iterations; runtime cancellation paths unchanged.
|22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated interrupt API introduced.
|23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|Pass|No cancellation checkpoint/frequency changes.
|24. No blocking operations without cancellation support|Pass|No new runtime I/O or blocking operation; process flag uses thread-safe C++ static initialization. Existing frame publication/reader synchronization retained.
|25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|33. Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|34. Response/output types use private Fields|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|35. Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|38. Password/secret fields have "sensitive": True|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|40. App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|42. display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|43. short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|46. No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|47. No bare field/option names in prose — must use backticks|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|48. Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|52. JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, action/app, typed provider class or JNI changes.
|53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete guarded context elision; no TODO/stub or benchmark-specific dispatch. Self-only mutable-flag prototype is not retained.
|54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Unused argv is skipped only without excess arguments. Existing excess-list allocation/ref ownership/release scope and success/error cleanup ordering remain. Optional ArgvContextHelper restores instantiated context; new lifetime/exception oracles pass baseline and candidate.
|55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Final path reads acquire-published immutable AOT context flags, not variant flags rewritten by IR materialization. Ready context required; no eager materialization or shared-state mutation. Invocation-local decisions, existing frame/Program/object lifetime and reader synchronization retained.
|56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|No new ABI/casts or serialized metadata. Receiver validity, resolved variant checks, parameter binding and return filtering remain; null context is short-circuited before flag access.
|57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Enabled polymorphic medians improve 13.2–14.3% with separated ranges in pilot/primary/longer repeat. Disabled control removes gain (+0.80%, overlapping). Scalar/hash controls remain mixed; all raw samples retained and no equivalence or Java-parity claim.
|58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Wrong/missing parameters retain existing validation and error cleanup. Test fixture initially lacked required variadic declarations; original baseline rejected it, declarations corrected and baseline rerun passed. Benchmark checks fail closed.
|59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal comments and README/design/release docs specify immutable proof, readiness, same execution Program, debugger and excess-argument restrictions; no new public method. Results separate loop throughput from isolated call latency.
|60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method changes.
|61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new runtime I/O, user-controlled format string, buffer or credentials. Evidence metadata uses an allowlist; no private absolute paths/environment dump.
|62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Final release/debug selected matrices each pass 48 files: 166 cases/15935 assertions plus17standalones. New mixed-receiver, explicit-self, private member, closure, recursive/nested, argv, excess-object destruction and exception cases match baseline/source/qmod/disabled controls. Old225/270 records, 11Python tests and standalone syntax pass.

### Validation record

|!Gate|!Result
|Production identity|Runtime diff 181969fe8434b2a68dfea0552fd838a9abdbf39d25639191fd2f71a82f06ed13; test diff 073d3d1d5f9ea7ebd69ee7f94b615e22c9586f298035b599a0499e39f88f7949
|Builds|Release/debug qore/qcc targets pass; library hashes retained in evidence
|Selected matrices|48 files per build: 31 QUnit wrappers, 166 cases / 15935 assertions plus 17 standalones
|Focused checks|Seven files per build: nine cases / 62 assertions plus three standalones; final fixture also passes saved baseline
|Ownership/context fixture|Source, qmod, disabled-cache and disabled-context outputs agree; explicit self, private members, closure capture, recursion, nested receivers, argv restoration and timely excess-object destruction on success/error
|Compatibility|Old v2/v3 artifacts produce byte-identical 225/270 records
|Other checks|11 Python harness tests; standalone release JITRuntime.cpp syntax; strict Qore tables, evidence/diff-hash integrity and whitespace checks
|Timing|310 unique jobs / 1550 positive finite samples; medians independently recomputed; no build/test/profile/validator overlaps
|Performance gate|Three enabled polymorphic runs improve 13.2–14.3%, separated ranges; disabled control +0.80%, overlapping; scalar controls remain mixed/slower, not declared equivalent
|Size|Runtime text 13872648 → 13873068 bytes (+420)
|Limitations|No full-suite, sanitizer, allocation-failure injection, Linux runtime, full split-runtime build or Java-parity claim

The skill's thread-safety check changed the implementation from mutable variant
flags to acquire-published immutable AOT context flags. Its performance check
did not accept the self-only pilot as a standalone win and kept the unfavorable
wide-callback medians visible. The test harness initially omitted explicit
variadic declarations on unused-argument methods; the original baseline caught
that fixture error, which was corrected and retested before final validation.

The final candidate leaves object/class/receiver context and frame publication
in place. It skips neither excess-argument ownership nor parameter/return
filtering. The process disable flag is sampled once on first fast-method use.
The performance goal remains active; this is a measured method-runtime step,
not a new method-entry ABI or Java parity.

## Assigned method fast-entry audit (2026-10-08)

Pre-commit audit against `132d53ad38aaf48bbcec4637b2389e7cb6b95564` on
`feature/performance`. Scope: `QoreIRToLLVM.cpp`, new assigned-method fixture /
wrapper, measurement contract/results/evidence, current-state design and release
notes. No qlib source, module, QPP, public API, serialized or runtime ABI change.
The complete audit skill and applicable module-structure, sandboxing and
cooperative-cancellation guides were read and applied. All 62 items follow.

The audit materially changed the implementation. The admission-only prototype
passed its then-selected matrix but changed a legal subclass NOTHING return to
integer zero. AST and saved AOT returned `36:null`; the prototype returned
`36:0`. The corrected code preserves boxed subclass returns, boxes native fast
results with normal ownership, and does not apply the speculative base's
non-NOTHING/local-effect summary across the fallback. Expanded regressions cover
normal/invoke paths, optional/string returns, captured-local mutation, wide
integers, tagged floats, booleans and throwing/invalid receiver cases.

The rejected prototype's 25% pilot result is not an accepted optimization claim.
The corrected implementation improves the polymorphic workload 16.6-17.3% across
three measurements, with separated fork ranges. Other control ranges overlap;
unfavorable medians and outliers are retained. Disabling the guarded path removes
the benefit. This is partial devirtualization with one base-class arithmetic
case inlined, not Java parity or three retained native method calls.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or relevant registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe assigned receiver admission and preserved subclass fallback contracts.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or relevant registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or relevant registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or relevant registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or relevant registration change.
|9|Copyright 2026 on all new files|Pass|Both new regression files carry 2026 copyright and SPDX headers; evidence documentation inherits the existing suite header.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or relevant registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or relevant registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or relevant registration change.
|13|%modern directive present|Pass|The new .qr fixture and .qtest wrapper both use %modern.
|14|Executable permission set (chmod +x)|Pass|Both new regression files are tracked as executable mode 100755.
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends the in-repository qlib path before requiring QUnit and FsUtil.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit and FsUtil are required; there are no external module dependencies.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Compiler changes add no filesystem operations; artifact generation uses the unchanged compiler path.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operations are added.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operations require a sandbox helper.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|The test uses FsUtil::TmpDir and quoted child commands solely for isolated compiler fixtures, with automatic directory cleanup; no production Qore I/O is added.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new C++ loop is added. Existing analysis, compiler, native stack and cancellation checks remain; thread/program interruption suites cover the retained paths.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or replacement cancellation logic is introduced.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|There are no new C++ loops requiring a new polling frequency.
|24|No blocking operations without cancellation support|Pass|No blocking C++ operation is introduced.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR changes.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR changes.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR changes.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR changes.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR changes.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR changes.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR changes.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR changes.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR changes.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR changes.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR changes.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR changes.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR changes.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR changes.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR changes.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR changes.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR changes.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR changes.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR changes.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Temporary diagnostic prints were removed. No TODO, stub or partial ABI is retained; rejected code and measurements are explicitly identified.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Speculative misses keep the owned boxed result; native fast hits box before the existing merge/cleanup tracking. Required argument filtering, sink routing and integer/float ownership regressions pass.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|The compiler reads existing per-function analysis maps; it adds no mutable runtime shared state, caches or publication protocol.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing typed class identities and BatchCalleeReturnKind enum are reused; tests use typed callbacks and ExceptionInfo catch hashes.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Corrected polymorphic workload improves 16.6-17.3% in pilot, primary and longer repeat, with separated ranges; the disabled-path benefit disappears. Mixed/unfavorable controls and rejected-prototype results remain reported. No new C++ loops or asymptotic costs.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing assignment/non-NOTHING, target/body/signature and runtime validity/exact-class guards remain. Null current_ir_func cannot reach the new helper call because base_facts would be null.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Current-state design, measurement contract, release notes and results document partial devirtualization, conservative subclass effects and boxed fallback returns; no public API is introduced.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flag changes.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new runtime ABI, format-string input, index arithmetic or credentials. Test commands quote paths. Sanitized evidence passes path/credential scans and exact source-diff/library hash checks.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Corrected release/debug builds and 51 files per build pass: 168 cases / 16041 assertions plus 18 standalones. New fixture has 98 assertions. Old v2/v3 records, 11 harness tests, native inspection and all 352 jobs / 1760 samples validate; the discovered subclass-return failure was fixed and retested.

### Final validation record

- Release/debug `cmake --build BUILD --target qore qcc -j 8` pass.
- Corrected selected matrices plus optional-scalar/receiver-lifetime tests pass
  51 files per build: 33 QUnit wrappers, 168 cases / 16041 assertions, and
  18 standalone tests. The new wrapper has 98 assertions per build.
- AST/IR/JIT/tiered, -O0/-O3, source-stripped AOT, native EH and disabled-method
  controls pass. Expanded baseline AST/AOT semantic checks pass; baseline code
  generation fails the intended newly-enabled guard assertion.
- Standalone release syntax for `QoreIRToLLVM.cpp`, eleven Python harness tests,
  old v2/v3 byte-identical 225/270 reference records, strict tables and whitespace
  checks pass. No full-suite, sanitizer, allocation-failure-injection, Linux
  runtime or full split-runtime-build claim.
- All six retained evidence records validate exact mode/kernel/fork counts,
  uniqueness, finite positive samples, startup counts and recomputed medians:
  352 jobs / 1760 samples, including rejected-prototype runs. No heavy work
  overlaps timing. Source diff and runtime/test hashes match the measured build;
  privacy scans pass.

Corrected release library:
`fce6971dbeef4025ac7481de2b721e104598b64cb47a63a5e1bf6babdcbd8864`.
Corrected debug library:
`b2013dea68e734e4becc18932ab81f015491395e7c985405cd831e6dd18e56f4`.
Compiler diff:
`737500d77b77a1ca2c3332e97de6ffa8ffa054e2b07de4cdcec1b0548f608c23`.
Test diff:
`d127d292249824908b0c371aa198abf1eb0ef9958056ee403c62c50a049c8dfb`.
Runtime text: 13873068 → 13873380 bytes (+312). Exact diffs, hashes, all samples
and limitations are in [sanitized evidence](results-assigned-method-macos-arm64.json).
Existing native artifacts need recompilation to change their code generation.
The broader performance goal remains active.

## Plain-function context audit (2026-10-08)

Scope: the retained plain-function context fix against
`c3ccb678e193d6d9404ae0ac13d8586bb096d2ef` on `feature/performance`.
This covers `lib/JITRuntime.cpp`, the two new
`AOTPlainFunctionClassContext` regression files, design/release documentation
and benchmark evidence. The uncommitted `AOTClassLocalAssignment` experiment
is excluded from this fix and commit; its compiler prototype is reverted.
No qlib, module, QPP, public API, serialized format or ABI change.

The complete audit skill and required module-structure, sandboxing and
cooperative-cancellation guides were read and applied. The audit materially
redirected the optimization: identical class annotations cannot justify blind
assignment-check elision because caller and callee access differ. The saved
AOT runtime leaked the caller's class; the new regression fails it at -O0.
The fix establishes the AST access boundary with existing RAII, leaving
parameter binding/cleanup, method/closure conventions and live guards intact.

All 62 checklist items are reported below: 21 Pass and 41 N/A.
No unresolved failure remains in this retained scope. The measured generic-call
cost is explicitly accepted for correctness, not presented as a performance gain.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or relevant registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe corrected plain-function access context and caller-context restoration.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or relevant registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or relevant registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or relevant registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or relevant registration change.
|9|Copyright 2026 on all new files|Pass|The two new regression files and measurement evidence carry 2026 copyright and MIT/SPDX notices.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or relevant registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or relevant registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or relevant registration change.
|13|%modern directive present|Pass|The new .qr fixture and .qtest wrapper both use %modern.
|14|Executable permission set (chmod +x)|Pass|Both new regression files have executable permissions and are staged as mode 100755.
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends the in-repository qlib path before requiring QUnit and FsUtil.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit and FsUtil are required; there are no external module dependencies.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|The runtime change adds no filesystem operation.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operations are added.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operations require a sandbox helper.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|The test uses FsUtil::TmpDir and quoted child commands solely for isolated compiler fixtures, with automatic directory cleanup; no production Qore I/O is added.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop is added or changed; existing stack and cancellation checks remain. Thread/program interruption suites pass.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or replacement cancellation logic is introduced.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|There are no new C++ loops requiring a new polling frequency.
|24|No blocking operations without cancellation support|Pass|No blocking C++ operation is introduced.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR changes.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR changes.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR changes.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR changes.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR changes.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR changes.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR changes.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR changes.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR changes.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR changes.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR changes.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR changes.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR changes.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR changes.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR changes.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR changes.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR changes.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR changes.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR changes.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|The fix uses the existing complete context RAII helper; no TODO, stub, diagnostic print, new ABI or partial optimization is retained.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|ObjectSubstitutionHelper restores thread and TLS RuntimeConfig state on normal and exceptional scope exit. Parameter/argv ownership and cleanup ordering are unchanged; no allocation or new fallible call is added.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only the current thread's object/class fields are scoped; no mutable shared metadata, cache or publication protocol is added. Existing frame-reader and concurrent stack tests pass.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing typed RAII context helper is reused; the fixture uses a typed callback and ExceptionInfo catch hashes. No casts or untyped callable API is added.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Two constant-time context scopes per general call preserve required semantics without new allocation or asymptotic work. Before/after costs, including the IR callback regression, are measured and reported; this is a correctness fix, not a speedup claim.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing call eligibility, parameter checks, Program switches, exception sinks and fallback routing remain. Binding and cleanup use caller context; body and return filtering use the plain function context.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Design notes and release notes explain the caller/callee access distinction and why an enforced class annotation cannot justify blind assignment-check elision. No public method or API is added.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flag changes.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|The regression closes inherited private-class access through plain-function fast calls. No new format-string input, array indexing or credentials; child arguments are quoted and evidence is sanitized.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug builds and 55 selected files per build pass: 202 QUnit cases / 16475 assertions plus 18 standalone tests. The 44-assertion new regression fails the saved baseline as expected. Old v2/v3 225/270 records, standalone runtime syntax and 11 harness tests pass.

### Final validation record

- Release/debug `cmake --build BUILD --target qore qcc -j 8` pass.
- Each build passes 55 selected files: 37 QUnit wrappers with 202 cases /
  16475 assertions, plus 18 standalone tests. The focused wrapper has 44
  assertions; access/classes/cast extras account for 33 cases / 390 assertions.
- The wrapper checks source AST/IR/JIT/tiered with synchronous low thresholds,
  stripped-source AOT -O0/-O3, native EH and disabled prelink/context/callback
  controls. Direct/invoke assignment, return filtering and caller restoration
  agree with AST. The saved baseline fails the new wrapper after 11 assertions.
- Standalone `JITRuntime.cpp` release syntax passes with 79 warnings in existing
  included headers; eleven Python harness tests pass. Old v2/v3 AOT artifacts
  reproduce all 225/270 reference records byte-for-byte.
- Two sequential interleaved measurement runs pass all reference checks and
  retain 108 unique jobs / 540 finite positive samples. Expected mode/kernel/
  fork counts, startup counts, identical before/after AOT artifact hashes and
  independently recomputed medians pass. No heavy validation overlaps timing.
- Primary IR wide callbacks worsen 3.28% with separated ranges; the longer
  repeat worsens 2.72% with narrowly overlapping ranges. Forcing AOT callbacks
  through the general helper worsens 6.32% with separated ranges. Optimized
  AOT controls have overlapping ranges; that is not proof of equivalence.
- Strict Qore tables, whitespace, source/binary hash and evidence privacy checks
  pass. No full-suite, sanitizer, allocation-failure injection, Linux runtime or
  full split-runtime build was performed. No new Java parity claim.

Release library:
`d7c01fc0f768fc6dadeef454d8af181c3e22147c51a48ade126055db01946327`.
Debug library:
`51ffef11c04f1317cf00ae5fa5d62adf57936521aad45423f6b1eff53d370de0`.
Runtime diff:
`e87f7d6840e007714899cd68adad9c6078a049dedd94d487b5808b14659018a2`.
Runtime text: 13873380 → 13873616 bytes (+236). Exact source/test diffs,
hashes, all samples, settings and limitations are in
[sanitized evidence](results-plain-function-context-macos-arm64.json).
The receiver-initializer optimization resumes from this corrected baseline;
the broader performance goal remains active.

## Checked class initializer audit (2026-10-08)

Scope: the checked AOT class-local initializer lowering against
`114eb51e33cbe0847c88ecc0c69d6f880dec878e` on `feature/performance`.
Changes comprise `lib/QoreIRToLLVM.cpp`, the two new
`AOTClassLocalAssignment` regression files, design/release notes, benchmark
instructions, results and sanitized evidence. No qlib source, module, QPP,
public API, runtime helper, serialized format or ABI change.

The complete audit skill and required module-structure, sandboxing and
cooperative-cancellation guides were read and applied. All 62 checks follow:
21 Pass and 41 N/A; no retained failure remains.
Blind identical-class check elision was rejected before timing because caller
and callee private-inheritance access can differ. The discovered pre-existing
plain-function context leak was fixed separately in the baseline commit.

The retained optimization keeps the full checked/eval-weak assignment and
reloads its actual stored value. It removes only the separate coercion temporary
and its ownership work. The cache stays NOTHING on the error edge. Restricting
the change to initial, non-result-producing stores avoids exposing unfiltered
values or altering failed reassignment behavior. The test's invoke assertion
was narrowed to the receiver slot, excluding unrelated catch-hash coercion;
the added escaped-capture closure's explicit int return annotation was corrected
before the final expanded wrapper passed.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or relevant registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe checked required-class AOT initialization, retained access/weak checks and cache coherence.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or relevant registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or relevant registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or relevant registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or relevant registration change.
|9|Copyright 2026 on all new files|Pass|Both new regression files and sanitized measurement evidence carry 2026 copyright and MIT/SPDX notices.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or relevant registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or relevant registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or relevant registration change.
|13|%modern directive present|Pass|The new .qr fixture and .qtest wrapper both use %modern.
|14|Executable permission set (chmod +x)|Pass|Both new regression files have executable permissions and are staged as mode 100755.
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends the in-repository qlib path before requiring QUnit and FsUtil.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit and FsUtil are required; there are no external module dependencies.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Compiler changes add no filesystem operation; normal artifact generation is unchanged.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operations are added.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operations require a sandbox helper.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|The test uses FsUtil::TmpDir and quoted child commands solely for isolated compiler fixtures, with automatic directory cleanup; no production Qore I/O is added.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No C++ loop is added or changed. Existing compiler/runtime stack and cancellation checks remain; thread/program interruption suites pass.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or replacement cancellation logic is introduced.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|There are no new C++ loops requiring a new polling frequency.
|24|No blocking operations without cancellation support|Pass|No blocking C++ operation is introduced.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR changes.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR changes.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR changes.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR changes.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR changes.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR changes.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR changes.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR changes.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR changes.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR changes.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR changes.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR changes.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR changes.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR changes.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR changes.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR changes.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR changes.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR changes.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR changes.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|The complete narrow initializer lowering is retained; no TODO, stub, diagnostic print or partial ABI. Blind class-proof elision was rejected before timing and the pre-existing context leak was fixed separately.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|The existing checked/eval-weak assignment owns its stored reference; success reloads the actual value into the existing owned tracker. Error edges keep the native cache NOTHING and preserve operand cleanup. Reassignments and value-producing stores are excluded; owned alias, weak, escaped-capture and native-EH tests pass.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Only per-function compiler state is changed; no runtime shared state, cache or publication protocol is added. Parallel code generation, concurrent stack readers and tiered cache tests pass.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Existing AssignmentMode enum and class type predicates are reused; optional/reference/complex types are excluded. Tests use typed callbacks and ExceptionInfo catch hashes; the new int-returning closure is explicitly typed.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|The 4096-element pilot improves the receiver-heavy polymorphic workload 6.65%; the longer 1024-element run improves it 5.88%, both with separated ranges. All control ranges overlap; unfavorable wide-callback medians remain reported. No new runtime loop or allocation. The seven-fork disabled-lowering control removes the benefit (+0.46%, overlapping ranges); native inspection confirms restoration of the original helper sequence.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Admission requires an AOT runtime-backed body local, normal initial assignment, no result, non-reference and one non-complex class type. Generic/optional and other store paths remain unchanged. Existing fresh-container proof requires one-use list/hash construction into container types, so it cannot bypass class-type exclusion.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Design, measurement instructions, release notes and results explain checked assignment/reload, exclusions, disable control, recompilation and Java comparison limits. No public method, new helper or ABI is introduced.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flag changes.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|Type/access and weak normalization checks remain; invalid input is not published into the native cache before success. No new array indexing, format-string input or credentials; subprocess arguments are quoted and evidence is sanitized.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug builds and 56 selected files per build pass: 203 cases / 16551 assertions plus 18 standalones. Expanded 76-assertion regression covers codegen/semantics/ownership; saved baseline passes expanded AST/AOT semantics and fails the intended new codegen assertion. Old v2/v3 225/270 records, 11 harness tests and standalone LLVM compiler syntax pass.

### Final validation record

- Release/debug `cmake --build BUILD --target qore qcc -j 8` pass.
- Each build passes 56 selected files: 38 QUnit wrappers, 203 cases /
  16551 assertions and 18 standalone tests. The expanded new wrapper has 76
  assertions. Release repeats it after capture-lifetime expansion; debug's
  full selected matrix uses the expanded fixture.
- AST/IR/JIT/tiered, -O0/-O3, stripped-source AOT, native EH, disabled initializer
  lowering and runtime-context controls agree. Tests cover private inheritance,
  missing/wrong/removed/weak values, capture/reference invalidation, escaped
  capture lifetime, owned alias cleanup, normal/invoke and failed reassignments.
- Expanded saved-baseline AST/AOT semantic checks pass. Its wrapper fails only
  the intended new codegen assertion at -O0 after 13 assertions.
- Standalone `QoreIRToLLVM.cpp` release syntax passes with 76 warnings in
  existing included headers; eleven Python harness tests and old v2/v3
  byte-identical 225/270 reference records pass.
- Pilot and smaller-batch longer/fresh-Java run improve the polymorphic workload
  6.65% and 5.88%, with separated fork ranges. All other Qore before/after ranges
  overlap; unfavorable wide-callback medians are retained. Disabling only this
  lowering removes the benefit (+0.46%, overlapping ranges).
- Native inspection shows coercion/unchecked-store replaced by checked-store/
  reload, and 430 → 386 instructions in the pilot's polymorphic function. Both
  disabled binaries retain the original helper sequence. The base arithmetic
  case remains inlined and two derived targets retain general dispatch.
- All 113 jobs / 565 samples validate exact mode/kernel/fork completeness,
  uniqueness, positive finite values, startup counts and recomputed medians.
  Separate before/after artifacts are compiled for this compiler change.
  No build/test/profile/disassembly/validator overlaps timing.
- Strict Qore tables, whitespace, source/test diff and binary hashes and evidence
  privacy checks pass. No full-suite, sanitizer, allocation-failure injection,
  Linux runtime or full split-runtime build claim.

Release library:
`127a66239c3b869a0e145ae67c37bfc7f8fa73c58914c015e0f75f6ca9d8d244`.
Debug library:
`6e1a5488ebd393491b05c33ff2dcbe835716cb13f8ddabc33c3ed79fb85eff96`.
Compiler diff:
`4881ef2b8e1925996405bf0b4dc0eab9bcfce9f5f521450ee3c39b601a5fef4f`.
Runtime text: 13873616 → 13873796 bytes (+180). Exact diffs, hashes and all
measurements are in [sanitized evidence](results-checked-class-store-macos-arm64.json).
The primary polymorphic workload remains about 72.1 times Java, with the
partial-devirtualization comparison caveat; wide callbacks remain about 4.89
times Java. No Java parity or universal speedup is claimed. The next structural
target is native parameter passing on guarded polymorphic cache hits.

## Native scalar method entry audit (2026-10-08)

The requested `audit-changes` skill was applied to the complete change against
`b9040b7e6d10bdde522ae3fceeb39f600e6d928a` on `feature/performance`.
All 62 checklist items were reviewed: **21 Pass, 41 N/A, no outstanding Fail**.
Required module-structure, sandboxing and cancellation guides were read; there
are no DataProvider or qlib source changes.

Scope: five internal C++ implementation/header files, one executable QUnit
wrapper and three executable fixtures, design/release/benchmark documentation
and sanitized evidence. The runtime adds a versioned optional scalar binding;
no function-table stride or serialized symbol-index format changes. Old binaries
keep the fallback. The context field is appended and the compiler emitter is private.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or relevant registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe native scalar polymorphic entries and the resolved fast-method debugger fallback correction.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or relevant registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or relevant registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or relevant registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or relevant registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or relevant registration change.
|9|Copyright 2026 on all new files|Pass|All four new executable test files and sanitized measurement JSON carry 2026 copyright and MIT/SPDX notices.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or relevant registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or relevant registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or relevant registration change.
|13|%modern directive present|Pass|The new wrapper and all three fixtures use %modern.
|14|Executable permission set (chmod +x)|Pass|All four new regression files are executable (100755).
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends the in-repository qlib path before requiring QUnit and FsUtil.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit and FsUtil are required; there are no external module dependencies.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Compiler/runtime/header changes add no filesystem operation; normal artifact generation remains unchanged.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No C++ network operations are added.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operations require a sandbox helper.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|The wrapper uses FsUtil::TmpDir and shell-quoted compiler/executable commands for isolated fixtures. Foreign Program parsing is local; no production filesystem or network access is added.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|The extracted whole-body validator scans at most 64 int/float nodes, unchanged from function-reference admission. No unbounded runtime loop is added; native stack/cancellation behavior and reader handshakes remain.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or replacement cancellation logic is introduced.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|Only the existing bounded 64-node scalar validator is refactored; there is no new C++ loop over 100 iterations.
|24|No blocking operations without cancellation support|Pass|No new blocking primitive or wait loop; the existing RuntimeConfig native-frame reader handshake is reused.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR changes.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR changes.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR changes.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR changes.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR changes.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR changes.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR changes.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR changes.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR changes.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR changes.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR changes.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR changes.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR changes.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR changes.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR changes.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR changes.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR changes.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR changes.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR changes.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR changes.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR changes.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete additive binding and guarded cache-hit path; no TODO, stub, diagnostic print or source fallback. Debugger regression is fixed, not waived; baseline-only frame-name assumptions are documented.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Context owns an atomic borrowed entry bounded by Program/image lifetime. Exact int inputs are borrowed; outputs use normal QoreValue boxing including full-width values. Existing cleanup handles consumed arguments; failure discards return value. Method context and native-frame RAII restore state and wait for readers. No fallible binding allocation.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|Binding uses idempotent acquire/release publication; relaxed already-bound load avoids redundant CAS. PIC names are populated before release publication and entries remain owned until cache destruction. Live identity/readiness/Program/debugger checks remain. Registration and reader stress tests pass; no race-detector claim.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Versioned typed int64(int64, context, sink) entry; exact bigIntTypeInfo on the actual override and NT_INT runtime input exclude references/enums/soft/optional coercions. Reader results use a hashdecl and typed ExceptionInfo catches; no C casts added.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Final 1024-element repeat: polymorphic AOT183.07909->135.40062 ns/element(-26.04%), separated ranges; earlier4096 pilot-28.22%. Final disabled-entry control182.16129->182.64991(+0.27%), overlapping ranges, removes target benefit. IR controls overlap. Wide-callback improvement also appears disabled and is not attributed; hash ranges overlap. Registration avoids repeat CAS, cache owns names once, no per-hit allocation for small ints.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Admission requires a whole-body nonthrowing scalar summary, exact unary int signature, implicit-self-compatible/context-independent same-body native entry, no specialization/import/capture, ready context/PC map and no self/argv requirement. Live Program, body, debugger and receiver guards retain generic paths for invalid/missing/extra/changed-return cases.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal helper arguments and lifetime are documented; the compiler emitter is private. Design, release notes and benchmark instructions describe additive ABI, preserved guards, debugger fallback, disable control and separate compilation. No public QPP class/API added.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flag changes.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new unchecked user-input indexing, format string or credentials. nargs must be one before reading args[0]; source-selected bounded proof, binding identity and actual resolved override are checked. Test subprocess paths are quoted; measurement evidence will be sanitized.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug qore/qcc builds and final 57-file matrices pass per build: 39 QUnit wrappers, 206 cases/16801 assertions plus18 standalone tests. New wrapper has3 cases/250 assertions, including split/native-EH/no-DWARF controls, foreign Programs, first-call races, readers and debugger attach/detach. Baseline debugger regression reports0/0 method events; corrected runtime reports16/16. Standalone runtime/compiler syntax(79/76 existing warnings), old225/270 AOT byte comparisons and11 harness tests pass.

### Corrections made during the audit

- A relaxed already-bound load avoids repeating atomic compare-and-swap when
  generic entries execute after publication; CAS remains the race-safe first-use path.
- The debugger test exposed a pre-existing resolved-cache-hit bypass of
  `evalTiered()`'s debugger fallback. The fast-method helper now rejects an attached
  execution-Program debugger. Baseline stripped AOT gets 0/0 method entry/exit
  events after warming; corrected AST/IR/JIT/tiered and stripped AOT get all 16/16.
- Initial debugger assertions required statement-step events from pure debug IR
  without statement temporary markers. LLDB and existing IR tracing established
  that distinction; the regression now checks method entry/exit and detach behavior.
  No runtime diagnostic print or instrumentation remains.
- The stack-reader test originally rejected AST's bare `next` name and unnamed
  compiled background scopes. Both occur in the saved baseline. The test accepts
  those existing representations, validates every named method frame and requires
  observed leaf frames. No runtime workaround was added.
- Native instruction count was checked against final disassembly and corrected
  to 14 before committing the evidence.

### Evidence and limits

Final runtime/compiler diff:
`5cba9ccb42cdbc3dcc3c138c0468c1b553e2ae88c6ec2ebb0cabb4a7dfd5d723`.
Test diff:
`71f3cfd087115dfb692748e5873de4f04a293532ae28431a85aa085ef1effb82`.
Release library:
`df1fb6352ad2cccd18e1594d8e051e255b262778b1b6e42be8eb7f46ad9d4f7e`.
Debug library:
`a64e7758aa182db3fd837bb743d419433aeb52fa8a435dc9a02c089d0e3b2985`.
Runtime text: 13873796 → 13878420 bytes (+4624).

Each final build passes 57 selected files: 39 QUnit wrappers, 206 cases /
16801 assertions, plus 18 standalones. New focused coverage is 3 cases /
250 assertions, including forced split codegen, native EH, no DWARF, foreign
Programs, concurrent publication/readers, full-width ownership and debugger
attach/detach. Standalone runtime/compiler syntax passes with 79/76 existing
warnings, 11 harness tests pass, and old v2/v3 artifacts reproduce all 225/270
reference records byte-for-byte.

The final target repeat improves 26.04%; its disabled-entry control is +0.27%
with overlapping ranges. The earlier 28.22% pilot is explicitly a different
candidate. All 133 jobs / 665 samples, startup samples, settings/completeness,
uniqueness and recomputed medians are retained and validated in
[the evidence](results-native-method-entry-macos-arm64.json).
Unrelated wide-control improvements persist when disabled and are not attributed;
IR/hash ranges overlap. Java polymorphic time is still about 53.74 times lower
in this workload, with the partial-devirtualization caveat.

No full repository regression, sanitizer/race-detector, Linux, allocation-failure
injection or complete split-runtime build is claimed. The performance goal remains
active; new work must preserve mutable admission guards and observability.

## Cached native-method admission audit (2026-10-08)

The requested `audit-changes` skill was applied to the complete change against
`929e08f9081a17efcc5bdcd20d09d6704a3f00f8` on `feature/performance`.
All 62 checks were reviewed: **21 Pass, 41 N/A, no outstanding Fail**.
The required module-structure, sandboxing and cancellation guides were read;
there are no DataProvider, qlib or public API changes.

Scope: internal runtime/header changes, one modified executable wrapper and
two modified executable fixtures, design/release/benchmark documentation and
sanitized measurement evidence. The PIC owns three additional immutable
borrowed pointers. Context/function-table layouts and serialized formats are
unchanged; the existing native binding is reused.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe caching immutable resolved-method facts while preserving mutable guards.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or registration change.
|9|Copyright 2026 on all new files|Pass|New measurement evidence carries 2026 copyright and MIT license; modified executable fixtures retain their notices.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or registration change.
|13|%modern directive present|Pass|Both modified fixtures and the wrapper use %modern.
|14|Executable permission set (chmod +x)|Pass|The wrapper and modified fixtures retain executable mode 100755.
|15|Uses %prepend-module-path  before %requires for in-repo modules|Pass|The wrapper prepends the repository qlib path before requiring QUnit and FsUtil.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|Pass|Only Qore-delivered QUnit/FsUtil are required; no external dependencies.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|No filesystem operation is added to the runtime/header changes.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|No network operation is added.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No new C++ filesystem/network operations need sandbox helpers.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|Pass|No new Qore filesystem/network operation. Existing shell-quoted fixture compilation uses FsUtil::TmpDir.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No new C++ loop. The existing bounded PIC key and stack/cancellation paths remain; static signature eligibility is moved from repeated hits to construction.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or replacement cancellation logic is introduced.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new C++ loop or changed polling frequency.
|24|No blocking operations without cancellation support|Pass|No new blocking primitive. Existing PIC publication mutex and native frame-reader handshake remain.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR change.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR change.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR change.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR change.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR change.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR change.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR change.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR change.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR change.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR change.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR change.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR change.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR change.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR change.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR change.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR change.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR change.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR change.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR change.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR change.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR change.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR change.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR change.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR change.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR change.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR change.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR change.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR change.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete prepared and ordinary admission paths; no TODO, stub, instrumentation or source fallback. Late entry publication remains functional.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Three borrowed pointers live with the existing retained immutable entry; no new allocation. Existing unique_ptr construction/publication, object/frame RAII, integer boxing and consumed-argument cleanup remain.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|All admission fields are initialized before release publication, read after acquire, and never changed. Entries are retained until cache destruction; epoch and Program keys remain live. Native entry/readiness acquire loads are not cached.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Actual resolved MethodVariantBase eligibility proves a user variant; exact int(int) signature excludes generic/optional/changed return contracts. Actual NT_INT is still checked at every invocation; no C casts added.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Primary polymorphic AOT133.44322->111.48285 ns/element(-16.46%), separated ranges; pilot-16.62%. Admission-disabled control132.51692->132.13502(-0.29%) overlaps. IR/wide/hash ranges overlap. No new hot-path allocation; three borrowed pointers per PIC entry.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Static/copy/generic/ineligible/unready methods retain ordinary dispatch. A prepared hit retains object validity, actual integer kind, normal-body identity/readiness, current/object/context/execution Programs and debugger checks. Late publication is handled by the ordinary path.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Design, release notes and benchmark instructions explain retained guards, entry lifetime, late-publication fallback and the admission-only disable control. Only private/internal implementation changes; no public API added.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flags changed.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No new user-controlled formats, unchecked indices or credentials. The existing PIC builder validates non-null arguments/count before a hit; nargs==1 precedes args[0]. Test subprocess paths remain quoted.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug builds each pass57 files:39 QUnit wrappers,206 cases/16835 assertions plus18 standalones. Final native wrapper rerun3 cases/284 assertions includes warmed deletion, late debugger publication and admission-disabled controls. Standalone runtime syntax79 existing warnings,11 harness tests and old225/270-record AOT byte comparisons pass. All133 jobs/665 samples validated.

### Audit decisions and final evidence

- Static admission belongs to the actual resolved override and full PIC key,
  not the parse-selected base method. The cached path explicitly retains the
  deleted-object check because it can bypass the old generic helper.
- Exact argument kind remains a live check; signature/type-key metadata alone
  does not justify accepting references or enums as native integers.
- Native entry, body identity, context readiness and debugger state are not
  snapshotted. An entry filled before native publication retains ordinary
  admission; the debugger regression tests this late-publication order.
- The added deletion fixture warms a single call site, deletes the receiver
  through another alias and requires `OBJECT-ALREADY-DELETED`. Saved-baseline
  AST and candidate source/AOT modes pass. The final wrapper was rerun after
  this fixture was added; its assertion count remains 284.
- Entry metadata is prepared before the existing release store; the existing
  ownership vector retains replaced entries. No extra allocation, publication
  primitive or lock is added. The actual defining-class pointer uses the same
  method-variant accessor as ordinary admission.
- Source inspection and disassembly confirm the prepared specialization is
  inlined in PIC dispatch. A separate profile identifies remaining context,
  frame and receiver-local costs; it does not overlap timing or replace tests.

Runtime/header diff:
`433d8936e75fa7b0e1a9d10d81c7ad42b9416fd75fa9d59af241870561602f8b`.
Test diff:
`e9bbda5e8b785e0e157fdbac141229e888a5d51246294063feea1251485450d3`.
Release library:
`653b375ba9885a149da3fa7f3c4d6082a2465b7e4ccd2f9add2936b97d3124ec`.
Debug library:
`f2c68f1697b8f0d0cc752d7e4df78e1c5b5c1b99e6b9ddf727f470274eee61f4`.
Runtime text: 13878420 → 13879800 bytes (+1380).

Release/debug validation covers 57 selected files each: 39 QUnit wrappers,
206 cases / 16835 assertions and 18 standalones. Focused native coverage is
three cases / 284 assertions in each final rerun. Standalone runtime syntax
passes with 79 existing warnings; 11 harness tests pass, and old v2/v3 AOT
artifacts reproduce all 225/270 reference records byte-for-byte.

[Evidence](results-method-admission-macos-arm64.json) retains the full diffs,
all 133 jobs / 665 samples, startup samples, hashes and validation rows.
Every measured mode passed 270 reference checks; all job combinations, sample
counts, finite positive values and recomputed medians were checked. Identical
AOT executable hashes match in every before/after runtime comparison.

The primary target improvement is 16.46%; the disabled-admission control is −0.29%
with overlapping ranges. Other primary IR/wide/hash ranges overlap. Fresh Java
is 2.54098 ns/element versus Qore 111.48285, about 43.87×, with the documented
partial-devirtualization/workload caveat. The goal is not complete.

No full repository suite, sanitizer/race detector, Linux, allocation-failure
injection or complete split-runtime build is claimed.

## Explicit native-method object context audit (2026-10-08)

The requested `audit-changes` skill was applied against
`85d37a49eb11aef5edcc20f7af9617796d135eb8` on `feature/performance`.
All 62 checks were reviewed: **16 Pass, 46 N/A, no outstanding Fail**.
Required module-structure, sandboxing and cancellation guides were read.
No Qore/qlib, DataProvider or public API files are changed.

Scope: one internal header and two runtime implementation files,
design/release/benchmark documentation and sanitized evidence. The new RAII
helper is used only in the existing pure native-method path; generic object
substitution remains untouched.

|!#|!Check|!Status|!Evidence
|1|Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)|N/A|No new module, QPP class or registration change.
|2|Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)|Pass|Release notes describe explicit RuntimeConfig object/class substitution with independent live/snapshot restoration.
|3|qore_user_module() or qore_external_user_module() call in CMakeLists.txt|N/A|No new module, QPP class or registration change.
|4|Module added to QMOD list in CMakeLists.txt|N/A|No new module, QPP class or registration change.
|5|.qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)|N/A|No new module, QPP class or registration change.
|6|%modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings|N/A|No new module, QPP class or registration change.
|7|No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)|N/A|No new module, QPP class or registration change.
|8|No %include usage (deprecated for modules)|N/A|No new module, QPP class or registration change.
|9|Copyright 2026 on all new files|Pass|The new sanitized measurement JSON carries 2026 copyright and MIT license. No new source/test file.
|10|Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)|N/A|No new module, QPP class or registration change.
|11|No second .qm for the same module at qlib/<ModuleName>.qm|N/A|No new module, QPP class or registration change.
|12|ns=Qore::XX matches the QoreNamespace constructor path|N/A|No new module, QPP class or registration change.
|13|%modern directive present|N/A|No Qore source or test files changed in this runtime-only candidate.
|14|Executable permission set (chmod +x)|N/A|No Qore source or test files changed in this runtime-only candidate.
|15|Uses %prepend-module-path  before %requires for in-repo modules|N/A|No Qore source or test files changed in this runtime-only candidate.
|16|External module dependencies use %try-module — except modules delivered with Qore itself (DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires|N/A|No Qore source or test files changed in this runtime-only candidate.
|17|No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks|Pass|Runtime/header changes add no filesystem operation.
|18|No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks|Pass|Runtime/header changes add no network operation.
|19|If filesystem/network ops exist, verify QoreSandboxManagerHelper usage|N/A|No filesystem/network operations need sandbox helpers.
|20|No File::, Dir::, Socket::, HTTPClient:: usage without justification|N/A|No Qore source or test files changed in this runtime-only candidate.
|21|All for/while loops that could iterate >100 times have qore_check_cancel() checks|Pass|No loop is added. Existing admission, native-frame and stack/cancellation behavior is unchanged.
|22|Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())|Pass|No deprecated cancellation API or changed cancellation logic.
|23|Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations|N/A|No new C++ loop or changed polling frequency.
|24|No blocking operations without cancellation support|Pass|No new blocking primitive; existing frame publication and reader waiting remain.
|25|Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)|N/A|No DataProvider, app, factory, JNI or JAR change.
|26|Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form|N/A|No DataProvider, app, factory, JNI or JAR change.
|27|Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted|N/A|No DataProvider, app, factory, JNI or JAR change.
|28|DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()|N/A|No DataProvider, app, factory, JNI or JAR change.
|29|DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>|N/A|No DataProvider, app, factory, JNI or JAR change.
|30|Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error|N/A|No DataProvider, app, factory, JNI or JAR change.
|31|Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)|N/A|No DataProvider, app, factory, JNI or JAR change.
|32|Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)|N/A|No DataProvider, app, factory, JNI or JAR change.
|33|Request/input types use public Fields (enables ClassName::Fields in action registration)|N/A|No DataProvider, app, factory, JNI or JAR change.
|34|Response/output types use private Fields|N/A|No DataProvider, app, factory, JNI or JAR change.
|35|Each field in data types has display_name, type, and desc (markdown-formatted)|N/A|No DataProvider, app, factory, JNI or JAR change.
|36|Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)|N/A|No DataProvider, app, factory, JNI or JAR change.
|37|Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text|N/A|No DataProvider, app, factory, JNI or JAR change.
|38|Password/secret fields have "sensitive": True|N/A|No DataProvider, app, factory, JNI or JAR change.
|39|groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc|N/A|No DataProvider, app, factory, JNI or JAR change.
|40|App logo stored as separate file, loaded at module level in Priv namespace|N/A|No DataProvider, app, factory, JNI or JAR change.
|41|App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value|N/A|No DataProvider, app, factory, JNI or JAR change.
|42|display_name is user-friendly ("Apache Avro" not "avro")|N/A|No DataProvider, app, factory, JNI or JAR change.
|43|short_desc is plain text, under 80 chars, single sentence — no markdown|N/A|No DataProvider, app, factory, JNI or JAR change.
|44|desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats|N/A|No DataProvider, app, factory, JNI or JAR change.
|45|Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"|N/A|No DataProvider, app, factory, JNI or JAR change.
|46|No bare True/False/NOTHING — must be backtick-wrapped in desc|N/A|No DataProvider, app, factory, JNI or JAR change.
|47|No bare field/option names in prose — must use backticks|N/A|No DataProvider, app, factory, JNI or JAR change.
|48|Long descriptions (>500 chars) use bold section headers and bullet lists|N/A|No DataProvider, app, factory, JNI or JAR change.
|49|Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)|N/A|No DataProvider, app, factory, JNI or JAR change.
|50|getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType|N/A|No DataProvider, app, factory, JNI or JAR change.
|51|Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails|N/A|No DataProvider, app, factory, JNI or JAR change.
|52|JAR install rules in CMakeLists.txt for all dependency JARs|N/A|No DataProvider, app, factory, JNI or JAR change.
|53|No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features|Pass|Complete narrow native object/class RAII substitution, with no stub, instrumentation or broad generic-helper replacement.
|54|Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation|Pass|Borrowed RuntimeConfig and ThreadData belong to the current native invocation. Both live and snapshot object/class stores are independently saved and restored, including exception unwinding. No reference ownership changes or allocation on the already-initialized native context.
|55|Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction|Pass|All new state is per-invocation stack state on the current thread. No mutable shared cache or publication protocol changes. Context restoration remains after the native frame has waited for readers.
|56|Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate|Pass|Typed RuntimeConfig reference, ThreadData/object/class pointers and deleted copy/assignment operations. No C-style casts or untyped callbacks.
|57|Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply|Pass|Primary polymorphic AOT112.26800->108.89255 ns/element(-3.01%), separated ranges; pilot-3.93%. Native-entry-disabled control181.99065->183.02620(+0.57%) overlaps. IR/wide/hash primary ranges overlap. Dispatch frame remains416 bytes; text+1548 bytes. No allocation on initialized native context.
|58|Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable|Pass|Existing actual-value, Program, receiver, body/readiness and debugger guards run before the new helper. Native caller acquisition establishes the valid same-thread RuntimeConfig view; no input validation is removed.
|59|Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats|Pass|Internal helper documents lifetime, thread, receiver and class arguments, independent restoration and no ownership change. Design/release/benchmark docs explain limited scope and correct disable control. No public API/layout change.
|60|QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects|N/A|No QPP method or flag change.
|61|Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code|Pass|No user-controlled format strings, indices, buffers or credentials. The native view's lifetime and same-thread requirement are documented.
|62|Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)|Pass|Release/debug builds each pass57 files:39 QUnit wrappers,206 cases/16835 assertions plus18 standalones. Initial8-file subset533 assertions+2 standalones; native wrapper284 assertions. Standalone runtime/thread syntax79/65 existing warnings,11 harness tests and old225/270-record AOT byte comparisons pass. All133 jobs/665 samples validated.

### Semantic and performance review

- The existing native entry guards establish an initialized, same-thread live
  RuntimeConfig before constructing the helper. Its borrowed context/thread
  pointers never outlive the invocation. Public getters/setters remain in use.
- Live ThreadData object/class fields and RuntimeConfig snapshot fields are
  saved separately and restored separately. No assumption of equal old values,
  context elision or object reference-count change is introduced.
- The native diagnostic frame is inside the substitution scope. Its destructor
  still waits for stack readers before context restoration; exception unwinding
  restores both stores. Existing stack and cancellation behavior is unchanged.
- Inlining hints appear on definitions, retaining ordinary out-of-line symbols
  for split builds. Disassembly verifies removal of generic substitution helper
  calls from native dispatch. Stack allocation remains 416 bytes.
- The attribution control disables the native entry, not merely admission
  caching: both prepared and ordinary native admission use this new helper.
  Its +0.57% change has overlapping ranges and removes the target speedup.
- The retained primary target gain is 3.01%, not the earlier 3.93% pilot. Other
  primary IR/wide/hash ranges overlap; no unrelated gains are attributed.

Runtime/header diff:
`b296d956ef9911b945b098a5a45fa2b8f05a13984a6f760c23a633e981c78aed`.
Release library:
`987393bab60dafb7f207d309f07ef01cae65ee2235d6482e9c61e17b137030e7`.
Debug library:
`b91d2c528f8b74917fe57e14a24056ae4a0fc2c3f00e03868ccaf934a456bd7f`.
Runtime text: 13879800 → 13881348 bytes (+1548).

Final release/debug validation covers 57 selected files each: 39 QUnit wrappers,
206 cases / 16835 assertions plus 18 standalones. Initial focused validation
covers eight files per build (eight cases / 533 assertions and two standalones).
Native method coverage remains three cases / 284 assertions, including deletion
after warming, late debugger publication, foreign Programs, wide ownership,
native EH, split codegen and concurrent publication/readers.

Standalone runtime/thread syntax passes with 79/65 existing warnings; 11 harness
tests pass, and old v2/v3 AOT artifacts reproduce 225/270 records byte-for-byte.
[Evidence](results-native-method-context-macos-arm64.json) retains all 133 jobs /
665 samples and startup samples, exact diffs/hashes and validation rows.
Job combinations, positive values, counts and recomputed medians were checked;
identical AOT hashes match across each runtime pair.

Fresh Java is 2.53536 ns/element versus Qore 108.89255, about 42.95×, subject to
the documented whole-workload/partial-devirtualization caveat. The active goal
is not complete; receiver-local handling remains a larger sampled cost.

No full repository suite, sanitizer/race detector, Linux, allocation-failure
injection or complete split-runtime build is claimed.
