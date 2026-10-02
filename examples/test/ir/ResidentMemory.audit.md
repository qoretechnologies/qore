# Audit: accurate resident-memory regression measurements

Copyright (C) 2026 Qore Technologies, s.r.o.

Date: 2026-10-02.

Scope: `NanBoxDoubleTagCollision.qtest`, `NanBoxDoubleTagCollisionScenario.qr`,
`IRFusedIntLocalOwnership.qtest`, `IRFusedIntLocalOwnershipScenario.qr`,
`design/qore-jit-aot-current-state.md`, and this report. Concurrent socket-controller
changes in the working tree are outside this audit and commit.

Applied every audit-changes check from
`/home/david/.codex/skills/audit-changes/SKILL.md`, including the module structure,
sandboxing, and cooperative cancellation guides. DataProvider checks are individually
marked N/A because this scope has no providers, actions, apps, or JNI dependencies.

Pipeline 58541's Ubuntu ARM64 job 206719 reported exactly 2048 kB of growth in the
promoted IR handler scenario. The fixture used VmRSS from `/proc/self/status`.
The Linux kernel documents asynchronous RSS accounting there and recommends
`smaps`/`smaps_rollup` when accuracy is required:
https://docs.kernel.org/filesystems/proc.html

Both ownership fixtures now use the Rss total in `/proc/self/smaps_rollup`, which
walks the page tables. Neither the leak limit nor the allocation workload was
relaxed. The negative control retains 200,000 boxed values, reports its retained
count, and must exceed the same 2048 kB limit; a direct run measured 7924 kB.
Without readable smaps_rollup, only memory checks are skipped. A read or parse
failure cannot silently become a zero measurement.

Validation uses the Release `build/` runtime with Qore debugging enabled and local
modules. `/usr/bin/qore` and both CMake build prefixes agree on `/usr`.
No C++ source is included in this change, so Valgrind is not required.

```sh
LD_LIBRARY_PATH=build QORE_MODULE_DIR=build/qlib-qmod:qlib \
  build/qore -b --enable-debug examples/test/ir/NanBoxDoubleTagCollision.qtest
LD_LIBRARY_PATH=build QORE_MODULE_DIR=build/qlib-qmod:qlib \
  build/qore -b --enable-debug examples/test/ir/IRFusedIntLocalOwnership.qtest
```

|!Suite|!Cases|!Assertions|!Result
|NanBoxDoubleTagCollision|16|93|Pass
|IRFusedIntLocalOwnership|15|80|Pass
|Total|31|173|Pass

Both runs exited successfully without warnings or errors. `git diff --check`
also passed. The updated fixtures are executed directly and AOT-compiled by the
suites, so no qlib qmod rebuild is needed.

|!Check|!Status|!Evidence
|1. Module catalog entry|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|2. Core/module release notes|N/A|Test measurement correction only; no released runtime or module API behavior changes.
|3. CMake module registration|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|4. QMOD registration|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|5. Lowercase introduction section|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|6. Modern module directives|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|7. No directives in separated classes|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|8. No deprecated includes|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|9. 2026 copyright|Pass|Both test suites, both fixtures, and this report have 2026 copyright notices.
|10. Directory-module layout|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|11. No duplicate module entry|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|12. QPP namespace alignment|N/A|No new or changed Qore module, module registration, or QPP class in this scope.
|13. Modern test directive|Pass|Both .qtest files use %modern; .qr fixtures enable modern mode automatically.
|14. Executable test permissions|Pass|Both .qtest files and fixtures retain mode 100755.
|15. Local module path and relative requirements|Pass|Both suites prepend local qlib before relative QUnit.qm and FsUtil.qm requirements.
|16. External dependency handling|Pass|Only in-repository QUnit and FsUtil are required; no external module dependencies.
|17. C++ filesystem sandboxing|N/A|No C++ or QPP source changes in this scope.
|18. C++ network sandboxing|N/A|No C++ or QPP source changes in this scope.
|19. Sandbox helper usage|N/A|No C++ or QPP source changes in this scope.
|20. Qore resource access|Pass|ReadOnlyFile reads only /proc/self/smaps_rollup for the test process; the standard file API enforces filesystem policy.
|21. Cancellation in long C++ loops|N/A|No C++ or QPP source changes in this scope.
|22. Current cancellation API|N/A|No C++ or QPP source changes in this scope.
|23. Cancellation check frequency|N/A|No C++ or QPP source changes in this scope.
|24. Blocking-operation cancellation|Pass|The added Qore loop retains runtime cancellation checks; no sleeps, polling, or native blocking operations added.
|25. Action names and descriptions|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|26. Action options|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|27. Action output types|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|28. API action request support|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|29. Find-action options and record type|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|30. Scheme action path/class rules|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|31. Single-key hash slices|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|32. Typed request/response classes|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|33. Public request Fields|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|34. Private response Fields|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|35. Field display name/type/description|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|36. Input examples|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|37. Typed allowed values|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|38. Sensitive password fields|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|39. AppGroup enums|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|40. App logo resources|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|41. Business-facing app descriptions|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|42. App display names|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|43. Plain short descriptions|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|44. Markdown descriptions|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|45. Business-language descriptions|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|46. Quoted boolean/missing literals|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|47. Quoted field/option references|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|48. Long description structure|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|49. FactoryMap registration|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|50. Record-type method signature|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|51. Committed dependency JARs|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|52. JAR install rules|N/A|No DataProvider, action/app registration, data type, factory, or JNI dependency changes.
|53. No workarounds or stubs|Pass|Replaces an asynchronously accounted RSS source with a page-table measurement. The 2048 kB limit and iteration counts remain unchanged.
|54. Exception safety|Pass|Managed Qore values clean up automatically; read failures propagate and missing Rss fields raise RSS-MEASUREMENT-ERROR.
|55. Thread safety|Pass|All added state is local to a synchronous fixture process; no shared mutable state is added.
|56. Type safety|Pass|Typed integer measurements, optional regex capture lists, and list<auto> for mixed-type boxing regression coverage.
|57. Performance|Pass|Only a few smaps_rollup reads per scenario; no per-iteration I/O. Negative control uses linear list append.
|58. Error handling|Pass|Unreadable smaps_rollup skips only RSS cases; failed reads or malformed output fail explicitly instead of producing a zero measurement.
|59. Documentation and examples|Pass|Fixture comments and the implemented JIT/AOT design document explain the measurement, platform requirement, and retained-value control.
|60. QPP flags|N/A|No C++ or QPP source changes in this scope.
|61. Security|Pass|No credentials or new network access; fixed proc path and existing shell quoting are preserved.
|62. Correctness|Pass|Both full suites pass across AST, IR, JIT, promoted tiers, and AOT; retained values exceed the unchanged leak threshold.

Result: **16 Pass / 46 N/A / 0 Fail**.
