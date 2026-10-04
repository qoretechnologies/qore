# Audit: REST completion errors

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` on 2026-10-04.
Copyright 2026 Qore Technologies, s.r.o.

Pipeline 59291 exposed `HTTPCLIENT-CANCELLED` instead of `HTTPCLIENT-TIMEOUT` in the macOS embedder deadline
test. The async controller dispatches both `abort()` and `onComplete()` for a timeout, without guaranteeing
their execution order. The REST request's abort callback prematurely rejected its Promise with cancellation;
the SSE startup callback similarly marked itself done before delivering an error to observers. Abort now
only cleans up transport work. Completion publishes the actual terminal reason, translating the controller's
socket timeout to the REST API's documented HTTP timeout. Explicit cancellation remains immediately terminal.

Scope: `RestClientIo.qm`, `RestClientIoCompletion.qtest`, module and global release notes, and the existing
async socket I/O design document. The issue 5474 and primary-key audits cover the earlier commits separately.

|!Check|!Status|!Evidence
|Module docs (`120_modules.dox.tmpl`)|N/A|No new module.
|Release notes|Pass|Updated RestClientIo 1.0 and global release notes.
|CMake / QMOD / Makefile registration|N/A|Existing module and targets; no new installed source or resource.
|Introduction section|Pass|Existing first section is lowercase `restclientiointro`.
|Modern directives|Pass|Module and new test use `%modern`; no redundant directives added.
|Separated sources / includes|N/A|No separated source or deprecated include added.
|Module layout / duplicate modules|Pass|Existing single-file module retained.
|Copyright|Pass|Changed module, new test, and audit identify 2026.
|QPP namespace paths|N/A|No C++ or QPP edits.
|Test directives / permissions|Pass|Executable test prepends local qlib and uses relative requirements as explicitly required by the user's AGENTS instructions, which take precedence over the design guide's plain-name preference.
|External dependencies|Pass|New test requires only in-repo modules; existing optional xml dependency uses `%try-module`.
|C++ filesystem / network sandbox checks|N/A|No C++ edits.
|Qore filesystem / network operations|Pass|Production uses existing stream cleanup only. Integration tests use an ephemeral loopback HttpServer and existing REST transport APIs.
|Cancellation loops / API / blocking operations|N/A|No C++ loops, deprecated cancellation API, or new production blocking operation.
|DP action labels / descriptions / options / output types|N/A|No DataProvider registration changed.
|DP API / find behavior / scheme actions / hash slices|N/A|No action or provider implementation changed.
|DP data-type classes / Fields visibility / descriptions / examples|N/A|No DataProvider types changed.
|DP allowed values / sensitive fields|N/A|No options or credentials changed.
|DP app groups / logos / descriptions / display names|N/A|No app registrations changed.
|DP short descriptions / markdown / business language / sections|N/A|No DataProvider descriptions changed.
|FactoryMap / getRecordTypeImpl signature|N/A|No factory or record-type implementation changed.
|Dependency JARs / installation|N/A|No dependencies added.
|Workarounds / stubs|Pass|Separates transport cleanup from publication of its terminal reason; no retry or relaxed assertion hides the failure.
|Exception safety|Pass|Cleanup remains guarded by the existing exception handling; error publication uses the existing completion guard. Tests preserve original descriptions and arguments.
|Thread safety|Pass|Existing mutex-protected Promise and observable completion guards remain authoritative; either abort/completion callback order produces one terminal result.
|Type safety|Pass|Uses existing typed SocketPollResultInfo and ExceptionInfo hashes, and a typed test callback.
|Performance|Pass|Constant-time error translation; no new loops or copies on successful production requests.
|Error handling|Pass|Timeouts, explicit cancellation, arbitrary transport errors, and empty completion results retain distinct outcomes. Duplicate and late callbacks cannot replace a completed Future or notify observers twice.
|Documentation|Pass|Abort comments and the durable async I/O design explain the callback contract; existing public timeout documentation and request examples remain accurate.
|QPP flags|N/A|No QPP edits.
|Security|Pass|No credentials, user-controlled format strings, or new production I/O.
|Correctness|Pass|Deterministic callback-order tests plus real blocked HTTP and SSE requests reproduce the defect and verify timeout delivery, cancellation, and subsequent client reuse.

Validation:

- The original module fails the deterministic request and SSE callback-order tests, and returns cancellation
  from the real controller-timeout test. The corrected implementation passes all six cases (89 assertions).
- Rebuilt `RestClientIo-qmod` in the existing Release build with `/usr` installation prefix before final tests.
- Completion regressions (89 assertions) and `EmbedderRetry.qtest` (96 assertions) pass in AST, IR with fallback
  warnings enabled, JIT, and tiered modes.
- Existing REST core (662), cookies (24), options (34), redirects (87), retry policy (152), SSE observable (65),
  and file-HTTP integration (176) assertions pass against the rebuilt module.
- All final tests use `--enable-debug`: 1,940 assertions total, with no failures or IR fallback warnings.
- Strict Qdx module extraction, strict QPP release-note validation, and `git diff --check` pass.
- Valgrind is N/A: this fix changes no C++ code.

All applicable checks pass. Superseded pipelines 59291 and 59294 were canceled before publishing this fix.
