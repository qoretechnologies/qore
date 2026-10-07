# Audit: REST response deadline rounding

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` on 2026-10-07.
Copyright 2026 Qore Technologies, s.r.o.

[CI job 209414](https://git.qoretechnologies.com/mirror/qore/-/jobs/209414) failed the embedder retry timeout
test: it expected two attempts but received three. `RestRetryPolicy` truncates the total remaining budget
to milliseconds; REST and signed AWS requests then truncated again after deducting request preparation.
The two truncations can end the response wait more than one millisecond before the policy deadline,
allowing another retry when the backoff is zero.

The deterministic regression models a 200 ms budget, a first transient error after 1,100 microseconds,
and 600 microseconds of preparation for each request. The policy gives the second request 198 ms. The
old response wait truncates 197,400 microseconds to 197 ms, returning at 198,700 microseconds and leaving
1 ms for a third attempt. The corrected wait uses 198 ms, returning at 199,700 microseconds; the outer
policy correctly sees no whole millisecond left. Early transport timeouts still retry.

Scope: `RestClientIo.qm`, `AwsRestClient.qm`, the deterministic deadline suite, module/global release notes,
and this audit. Both response waits use overflow-safe ceiling division only after checking that the
microsecond remainder is positive. The inherited monotonic-clock hook permits deterministic tests of
both production request paths without network I/O, sleeps, or polling. The rounding can extend the
requested wait by less than one millisecond, matching the Future API's granularity.

|!Check|!Status|!Evidence
|Module docs (`120_modules.dox.tmpl`)|N/A|No new module.
|Release notes|Pass|Updated both module release notes and global release notes.
|CMake / QMOD / Makefile registration|N/A|Existing modules and installed sources; no registration changes.
|Introduction section|Pass|Existing lowercase `restclientiointro` and `awsrestclientintro` retained.
|Modern directives|Pass|Both modules and the new test use `%modern`; no redundant directives added.
|Separated sources / includes|N/A|No separated source or deprecated include added.
|Module layout / duplicate modules|Pass|Existing single-file modules retained.
|Copyright|Pass|Changed modules, test, and audit identify 2026.
|QPP namespace paths|N/A|No C++ or QPP changes.
|Test directives / permissions|Pass|Executable test prepends local qlib and uses relative requirements for all in-repo modules.
|External test dependencies|N/A|New test requires only in-repo modules.
|C++ filesystem / network sandbox checks|N/A|No C++ changes.
|Qore filesystem / network operations|Pass|No new direct File, Dir, Socket, or HTTPClient operations; existing transport APIs and cancellation remain in place.
|Cancellation loops / API / blocking operations|Pass|No new loops or blocking operations in production; error cleanup still uses cancellation deferral. No deprecated cancellation API added.
|DP action labels / descriptions / options / output types|N/A|No action registration changes.
|DP API / find behavior / scheme actions / hash slices|N/A|No DataProvider implementation changes.
|DP data-type classes / Fields visibility / descriptions / examples|N/A|No DataProvider types changed.
|DP allowed values / sensitive fields|N/A|No options or credentials changed.
|DP app groups / logos / descriptions / display names|N/A|No app registrations changed.
|DP short descriptions / markdown / business language / sections|N/A|No DataProvider descriptions changed.
|FactoryMap / getRecordTypeImpl signature|N/A|No factory or record-type implementation changed.
|Dependency JARs / installation|N/A|No dependencies added.
|Workarounds / stubs|Pass|Fixes the loss of precision at the response-wait boundary; original integration assertions and retry eligibility remain unchanged.
|Exception safety|Pass|Timeout and other request failures cancel unfinished I/O through the existing guarded cleanup. No new resource ownership or allocation in production.
|Thread safety|Pass|Production uses call-local arithmetic and the monotonic clock; test clocks are used synchronously on the test thread.
|Type safety|Pass|Integer time units, typed exception/response hashes, and typed request/cancellation callbacks.
|Performance|Pass|Constant-time conversion; no new production loops, copies, or allocations.
|Error handling|Pass|Expired and invalid budgets remain errors; timeout translation, original exception details, and cancellation cleanup are covered independently.
|Documentation|Pass|Public request documentation describes millisecond granularity and retains its request example and parameter/return/exception contracts. Both release-note sections describe the fix.
|QPP flags|N/A|No QPP changes.
|Security|Pass|No credentials or new production I/O; test AWS signing uses explicit fixture values and disables credential discovery.
|Correctness|Pass|Deterministic tests exercise REST and AWS paths, the observed third attempt, early transient failures, fractional/integral/expired budgets, invalid/overflowing input, maximum valid input, and error cleanup. Existing HTTP integration tests validate actual transport behavior.

Validation:

- Before the arithmetic fix, the controlled-clock regression reports exactly `Expected: 2, Actual: 3`;
  its fractional-millisecond case also fails. Both pass after the fix.
- Rebuilt `RestClientIo-qmod` and `AwsRestClient-qmod` with their dependencies in the existing Release
  build, using the installed `/usr` prefix. CMake reports the existing optional quictls and LibreSSL
  backends as disabled; compilation succeeds.
- Nine debug-enabled suites pass: deadline regressions (156 assertions), REST core (662), completion
  (89), cookies (24), options (34), redirects (87), retry policy (152), embedders (96), and AWS (66):
  1,366 assertions. The optional AWS live test skips its unavailable IoT Events endpoint after one
  signature assertion; all local AWS checks pass.
- The new 156-assertion deadline suite also passes in explicit AST, IR, JIT, and tiered modes.
- Strict Qdx extraction passes for both modules; strict QPP validation passes for the global release
  notes; `git diff --check` passes.
- Valgrind is N/A because there are no C++ changes.

All applicable audit checks pass.
