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
