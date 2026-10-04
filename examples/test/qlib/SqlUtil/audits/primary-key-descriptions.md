# Audit: primary-key descriptions

Applied `/home/david/.codex/skills/audit-changes/SKILL.md` on 2026-10-04.
Copyright 2026 Qore Technologies, s.r.o.

Pipeline 59269 exposed an `UNSUPPORTED` error in `DbRecordMutation.qtest` after SQLite primary-key introspection
was enabled. `setupTable()` requested primary-key alteration SQL while constructing metadata for schema alignment.
The fix retains key validation and construction while generating SQL only for callers that request it. An additional
no-key regression test exposed `getPrimaryKey()` returning an uninitialized value for manual descriptions; it now
constructs the driver's empty key object without querying a table that may not exist.

Scope: `SqlUtil/AbstractTable.qc`, module and global release notes, and `Sqlite3SqlUtilKeys.qtest`.
The separate issue 5474 audit remains in `../../GoogleDataProvider/audits/5474.md`.

|!Check|!Status|!Evidence
|Module docs (`120_modules.dox.tmpl`)|N/A|No new module.
|Release notes|Pass|Updated SqlUtil 3.2.0 notes and global notes for the schema-alignment fix.
|CMake registration / QMOD list / Makefile|N/A|Existing module and targets; no new source entry point or resource.
|Introduction section|Pass|Existing first section is lowercase `sqlutilintro`.
|Modern directives|Pass|Module and test use `%modern`; no redundant directives added.
|Separated sources / includes|Pass|No parse directives or deprecated includes added to `AbstractTable.qc`.
|Module layout / duplicate modules|Pass|Existing directory module retained; no duplicate entry point.
|Copyright|Pass|Changed sources, test, and new audit identify 2026.
|QPP namespace paths|N/A|No C++ or QPP changes.
|Test directives / permissions|Pass|Executable test uses local relative requirements after `%prepend-module-path`.
|External dependencies|Pass|Existing external sqlite3 dependency uses `%try-module`; in-repo requirements are hard requirements.
|C++ filesystem / network sandbox checks|N/A|No C++ changes.
|Qore filesystem / network operations|Pass|No File, Dir, Socket, or HTTPClient operations added; tests use an in-memory database.
|Cancellation loops / API / blocking operations|N/A|No C++ loops, cancellation APIs, or new blocking operations.
|DP action labels / descriptions / options / output types|N/A|No DataProvider registration changed.
|DP API / find behavior / scheme actions / hash slices|N/A|No action or provider implementation changed.
|DP data-type classes / Fields visibility / field descriptions / examples|N/A|No DataProvider type definitions changed.
|DP allowed values / sensitive fields|N/A|No option or credential definitions changed.
|DP app groups / logos / descriptions / display names|N/A|No app registrations changed.
|DP short descriptions / markdown / business language / sections|N/A|No DataProvider descriptions changed.
|FactoryMap / getRecordTypeImpl signature|N/A|No factory or record-type implementation changed.
|Dependency JARs / installation|N/A|No dependencies added.
|Workarounds / stubs|Pass|Metadata construction no longer requires unsupported DDL; real SQLite key alterations still fail explicitly. No TODOs or incomplete operations added.
|Exception safety|Pass|Key publication follows validation and successful construction. Negative tests cover missing, empty, and non-string key columns, and verify failed DDL does not publish a key.
|Thread safety|Pass|Existing table mutex covers metadata construction and lazy initialization; no shared state added.
|Type safety|Pass|Typed boolean controls SQL generation; existing AbstractPrimaryKey and Sqlite3Table types retained. Empty metadata returns the documented object type.
|Performance|Pass|No additional loops or copies in production; metadata construction avoids unused SQL generation. Empty manual keys initialize once without database introspection.
|Error handling|Pass|Column, option, and key validation remain in the common path; only SQL generation is conditional. Unsupported DDL remains observable.
|Documentation|Pass|`setupTable()` explains metadata-only primary-key construction and includes a schema-alignment example; release notes cover both fixes. No public signatures changed.
|QPP flags|N/A|No QPP changes.
|Security|Pass|No credentials, dynamic format strings, new external I/O, or bounds-sensitive code added.
|Correctness|Pass|Single-column, ordered composite, absent, and invalid keys are covered; unchanged keys produce no alignment SQL. The original field rename preserves records and transaction ownership.

Validation:

- Rebuilt `SqlUtil-qmod`, `Sqlite3SqlUtil-qmod`, and `DbDataProvider-qmod` before testing.
- `Sqlite3SqlUtilKeys.qtest`: 7 cases, 37 assertions; `DbRecordMutation.qtest`: 15 cases, 112 assertions.
  Both pass in AST, IR with fallback warnings enabled, JIT, and tiered modes.
- SQLite expression tests: 162 assertions; schema alignment transactions: 26; foreign-constraint quoting: 26;
  native bulk SQL with the local dbitest module: 67; streamed bulk SQL: 38.
- Rechecked the issue 5474 suites: offline schemas, 524 assertions; offline index, 30 assertions.
- All tests run with `--enable-debug`; the above runs total 1,469 assertions.
- Strict Qdx extraction of SqlUtil, strict QPP release-note table validation, and `git diff --check` pass.
- Valgrind is N/A because no C++ code changed.

All applicable checks pass. Pipeline 59269 was canceled before publishing the replacement commit.
