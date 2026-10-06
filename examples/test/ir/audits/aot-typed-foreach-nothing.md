# Typed foreach over NOTHING (#5476)

Copyright (C) 2026 Qore Technologies, s.r.o.

## Root cause and correction

The LLVM lowering of `ListSize` fetched a typed input list's data pointer
unconditionally when preparing direct element reads. `qore_rt_list_size()`
accepts `NOTHING` and returns zero, but `qore_rt_list_get_data_unchecked()`
requires an actual list. The hoisted call ran before the iteration bounds
check could skip the empty input.

A local conditionally assigned inside an outer loop reproduced the crash:

```qore
foreach int entity in ((0, 1, 0, 2, 0)) {
    list<string> targets;
    if (entity == 1) {
        targets = ("one",);
    } else if (entity == 2) {
        targets = ("two", "three");
    }
    foreach string target in (targets) {
        print(target);
    }
}
```

The interpreter printed `onetwothree`; the source-stripped AOT executable
exited with SIGSEGV. The new regression also failed against the original
library for both AOT executables and a compiled module. Disabling only
`QORE_DISABLE_AOT_TYPED_LIST_DATA_HOIST` made it pass.

The lowering now branches on the size already computed for the loop. The
nonzero path fetches the data pointer once, while the zero path contributes
a null pointer to a pointer-typed PHI. Existing bounds checks prevent that
null pointer from being read. List ownership, cancellation checkpoints,
and exception handling are unchanged. Existing AOT artifacts must be rebuilt.

## Audit-changes checklist

|!Check|!Status|!Evidence
|Module docs (`120_modules.dox.tmpl`)|N/A|No shipped module added.
|Release notes|Pass|3.0.0 notes describe the crash fix and AOT recompilation requirement.
|CMakeLists/QMOD/Makefile registration|N/A|No shipped module or build target added.
|Module structure and directives|N/A|No shipped `.qm` or `.qc` modified; generated module exists only as a test fixture.
|New QPP class namespace|N/A|No QPP class added.
|Copyright|Pass|Changed C++ source and new regression/audit files identify 2026.
|Test modern mode and executable bit|Pass|New `.qtest` uses `%modern` and mode 0755; generated fixtures use `%modern`.
|Test module paths|Pass|Local qlib path precedes relative requirements for QUnit and FsUtil; generated qmod is required by relative path.
|External test dependencies|N/A|No external module required.
|Sandboxing|Pass|No native filesystem/network operation added; Qore file/process operations create and execute isolated regression fixtures managed by TmpDir.
|Cooperative cancellation|Pass|No C++ loop/blocking operation added; existing loop checkpoints remain after pointer initialization. No deprecated interrupt API added.
|DP action registration, options, output types, request/find methods|N/A|No DataProvider changes.
|DP typed fields, visibility, examples, allowed values, sensitive fields|N/A|No DataProvider changes.
|DP app groups, logos, descriptions, scheme actions|N/A|No DataProvider changes.
|DP short/long description formatting and length|N/A|No DataProvider changes.
|FactoryMap registration and record-type signature|N/A|No DataProvider changes.
|Dependency JAR tracking and installation|N/A|No JNI changes.
|Workarounds/stubs|Pass|Corrects the unsafe hoist itself; production specialization remains enabled; no TODO/FIXME/stub added.
|Exception safety|Pass|LLVM owns new blocks/instructions; no new Qore allocation or fallible runtime operation. Sparse slots and caught exceptions are tested.
|Thread safety|Pass|New state is local to function lowering; the existing retained-list snapshot and copy-on-write semantics remain intact.
|Type safety|Pass|Native i64 size comparison and pointer-typed PHI; regression covers typed int, float, bool, and string elements.
|Performance|Pass|Constant additional work before the loop; data-pointer hoisting and unchecked in-loop reads remain enabled.
|Error handling|Pass|Zero-size inputs bypass unchecked access; sparse lists still raise `RUNTIME-TYPE-ERROR` and loop-body exceptions preserve their identity.
|Documentation|Pass|Current-state design describes the invariant; this regression provides the triggering example. No public API added requiring parameter/return/throw docs.
|QPP flags|N/A|No QPP methods changed.
|Security|Pass|Unchecked pointer fetch requires nonzero size; no credentials or user-controlled format strings introduced.
|Correctness|Pass|Regression checks values, iteration counts, and exception identities across execution tiers and source-stripped AOT artifacts; focused suites and Valgrind validation listed below.

## Validation

`AOTTypedForeachNothing.qtest` exercises each of int, float, bool, and string
lists with unassigned, single-element, populated, empty, removed, deleted,
and sparse inputs. It returns to unassigned iterations after populated ones
and after caught sparse-slot and loop-body exceptions. Expected values,
counts, exception identities, and child exit statuses are checked across AST, IR, tiered, JIT,
source-stripped AOT executables at `-O0` and `-O3`, an executable with data
hoisting disabled, and a source-stripped module whose source is deleted.

The focused suites are:

- `AOTTypedForeachNothing.qtest`
- `IRTypedForeach.qtest`
- `IRTypedForeachExceptionState.qtest`
- `IRBoundedTypedListRead.qtest`
- `AOTCollectionNothing.qtest`
- `AOTFusedMapFold.qtest`
- `AOTCollectionPredicateFusion.qtest`
- `AOTStringFoldJoin.qtest`
- `AOTStringFoldrJoin.qtest`
- `AOTSharedMapArgs.qtest`

Run with `LD_LIBRARY_PATH` and `QORE_LIBDIR` pointing to the chosen build,
`QORE_BIN` pointing to its `qore`, and `QCC` pointing to its `qcc`; pass
`-b --enable-debug` to qore. The regression also passes these runtime options
to its child interpreters and AOT executables.

Valgrind checks use the Debug build, `--error-exitcode=99 --leak-check=full
--show-leak-kinds=definite,indirect,possible
--errors-for-leak-kinds=definite,indirect,possible`, and `-b --enable-debug`
for execution. These checks use `--read-inline-info=no` because
Valgrind's inline-frame decoder warns about GCC's emitted DWARF metadata.
Memory checking is unchanged and no error/leak suppressions are used.

Pre-commit results: all ten focused suites passed with `build-debug`; the
final regression passed all three cases and 24 assertions. Valgrind passed
compilation, JIT execution, and source-stripped AOT execution of the same
generated four-type fixture with zero errors and zero definite, indirect,
or possible lost bytes. LLVM module verification and inspection confirmed
that the unchecked fetch is confined to the nonempty branch. `git diff
--check` passed.
