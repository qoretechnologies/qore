# PR #5456 review follow-up audit

Copyright 2026 Qore Technologies, s.r.o.

Reviewed on 2026-09-18 against `9f0566f39817d1c76dc568632de51c0ad9beadcc`.
The complete audit-changes checklist was applied before committing this follow-up.
Scope: two existing provider classes, their two contract suites, the Azure schema-pruning
tool, and this report. The three Copilot conversations concern incomplete Textract S3
references, empty/malformed Azure helper sources, and missing pruning-tool arguments.

Textract now rejects missing, empty or non-string bucket/name fields with a field-specific
error before constructing the native document. Azure distinguishes source cardinality from
invalid URL/path values, validates HTTP(S) document URLs with a host, and does not include
document URLs or SAS credentials in the diagnostic. Valid URL bytes remain unchanged.
The pruning tool requires exactly one argument and exits with status 2 plus usage otherwise.

The encoded-URL regression exposed a test-fixture issue: the HTTP fixture passed JSON to
the formatted makeResponse overload. It now uses the data overload, preserving percent
sequences and binary responses. This was a fixture correction, not a production transport change.

## Verification

Both changed providers were rebuilt before testing. Tests used compiled module paths with
source loading excluded; Textract tests and both qualification scripts check actual qmod filenames.

|!Gate|!Result
|Release AST, IR, JIT, tiered|Per mode: Textract NativeContracts 5 cases / 348 assertions; Azure WireContracts 11 cases / 269 assertions
|Pruning usage|Missing and excess arguments return 2 with usage in all four execution modes
|Schema reproduction|The pinned Azure Swagger import passes byte-for-byte --check with the guarded pruning tool
|Fresh AOT qualification|Both provider inventories, typed contracts, complete discovery and authenticated index publication/readback pass
|Catalog parity|Azure 773 messages and Textract 637 messages, each across all 12 locales; exact IDs/sources and protected syntax pass
|Metadata and docs|qm-metadata and docs-fast targets for both changed providers pass
|Hygiene|Executable modes, copyright, diff whitespace and all 71 audit rows checked

No new live cloud mutation was needed for these validation changes. Existing successful live
coverage and its limits remain documented in 5420.md, 5421.md and 5421-live.md. The original
Debug and empty-prefix installation evidence remains applicable to unchanged packaging;
this follow-up does not claim a fresh Debug or empty-prefix install run.

## All 71 checks

Result: **47 Pass, 24 N/A, zero Fail**.

|!#|!Check|!Status|!Evidence
|1|Module documentation index|N/A|No new module or public capability; existing entries unchanged.
|2|Release notes|N/A|No new module; existing app-completion release notes remain accurate.
|3|CMake module registration|N/A|No new module or packaging change.
|4|QMOD registration|N/A|No new module; both existing qmod targets rebuilt successfully.
|5|Lowercase intro sections|N/A|No qm entrypoint changed.
|6|Modern module syntax|N/A|No qm entrypoint changed.
|7|Separated class parse directives|Pass|Neither edited qc file introduces a parse directive.
|8|Deprecated includes|Pass|No include directive introduced.
|9|Copyright|Pass|All changed sources and the new report carry Copyright 2026.
|10|Module directory layout|N/A|No module file moved or added.
|11|Duplicate module entrypoint|N/A|No qm file added.
|12|QPP namespace path|N/A|No C++ or QPP change.
|13|Modern tests|Pass|Both edited qtest files and the pruning script use %modern.
|14|Executable tests|Pass|Both qtest files and the pruning tool retain mode 100755.
|15|Module prepend ordering|Pass|Source tests prepend before requires; isolated qualification intentionally excludes source paths.
|16|External optional test modules|Pass|Tests require only project-delivered modules; no optional external dependency added.
|17|C++ filesystem sandboxing|N/A|No C++ change.
|18|C++ network sandboxing|N/A|No C++ change.
|19|Sandbox manager helper|N/A|No C++ change.
|20|Qore I/O justification|Pass|Existing File reads handle explicitly requested document/tool inputs; invalid sources fail before I/O, fixture HTTP is local only.
|21|C++ long-loop cancellation|N/A|No C++ change.
|22|Cancellation helper choice|N/A|No C++ change.
|23|Cancellation check frequency|N/A|No C++ change.
|24|Blocking C++ cancellation|N/A|No C++ change.
|25|Action descriptions|Pass|No metadata edits; original audits and fresh exact action qualification retained.
|26|Action options|Pass|Existing field-derived options unchanged; compiled providers accept valid and reject invalid source inputs.
|27|Action output types|Pass|Existing typed outputs unchanged; full native/wire suites and qualification pass.
|28|API request capability|Pass|Azure helper keeps supports_request and doRequestImpl; Textract helper execution tested through its provider.
|29|Find-action search contract|N/A|Only API actions are affected.
|30|Scheme/path registration|Pass|No registration change; all actions still qualify and resolve through existing paths.
|31|Single-key hash slices|Pass|No single-key hash slice introduced; req{field} intentionally selects one value.
|32|Typed request/response classes|Pass|Existing HashDataType and schema-derived contracts preserved; serialization qualification passes.
|33|Public input fields|Pass|Existing public Fields and schema-derived options retained; no type metadata edits.
|34|Private response Fields|N/A|No response type changed; Azure's existing public compatibility constant is explicitly documented.
|35|Field presentation metadata|Pass|No field metadata changed; exact catalog sources and IDs retained.
|36|Useful input examples|Pass|Existing URL, local-file and S3 examples retained; no new field introduced.
|37|Structured finite choices|Pass|Existing allowed-value structures unchanged; Textract and Azure serialization regressions pass.
|38|Sensitive fields|Pass|Credential and document-URL sensitivity retained; new diagnostics never echo supplied values.
|39|Application groups|Pass|Existing AppGroup registrations unchanged and qualified.
|40|Separate app logo|Pass|Existing packaged logo resources unchanged and module loading succeeds.
|41|Application Markdown|Pass|No application prose changed; original app audits and exact source parity retained.
|42|Friendly application name|Pass|Existing user-facing application names unchanged.
|43|Plain short descriptions|Pass|No short description changed; original audits remain applicable.
|44|Description Markdown|Pass|No presentation description changed; catalog sources are identical.
|45|Business descriptions|Pass|Existing document-processing descriptions and examples retained.
|46|Boolean/nothing prose|Pass|No new presentation prose with bare language literals.
|47|Field names in prose|Pass|No presentation prose added; error strings intentionally identify the input field.
|48|Long description structure|Pass|No long description changed.
|49|FactoryMap|Pass|aws-textract and azure-documentintelligence remain mapped in DataProvider.qc; fresh qualification passes.
|50|Record-type signature|N/A|No record/search provider method changed.
|51|Committed dependency JARs|N/A|Neither changed provider uses JNI.
|52|JAR install rules|N/A|No Java dependencies.
|53|Early lightweight inventory|Pass|Inventory registration unchanged; fresh qualification checks the complete original inventories.
|54|Structured qualification failures|Pass|Both fresh qualification reports complete with zero failures; no discovery catch path changed.
|55|Authenticated index publication|Pass|Both qualification scripts publish and read back through the existing qualified discovery path.
|56|Versioned import boundary|Pass|Pinned Azure import reproduces byte-for-byte; no schema or choice normalization changed.
|57|Static recursive presentation|Pass|Only request execution validation changed; no metadata callback or extraction behavior added.
|58|Stable and nested IDs|Pass|Exact catalog ID/source checks pass for both apps.
|59|Complete locales|Pass|All 12 locales pass for each app, without source fallback.
|60|Installed artifact isolation|Pass|Fresh checks exclude source module paths and assert actual AOT filenames; prior empty-prefix evidence retained for unchanged packaging.
|61|JNI dependency inventory|N/A|No Java dependencies.
|62|No incomplete workarounds|Pass|All three review issues fixed directly; no TODO, stub or disabled assertion added.
|63|C++ exception safety|N/A|No C++ allocation or exception path changed.
|64|Thread safety|Pass|Validation state is local to each request; existing fixture shared state remains synchronized.
|65|Type safety|Pass|S3 fields checked before soft conversion; URL parsing produces a typed hash; fixture uses the unformatted data overload.
|66|Performance|Pass|Two fixed S3 checks and one linear URL parse; no repeated schema materialization or additional network I/O.
|67|Input/error handling|Pass|Regressions cover absent/null/empty/non-string S3 fields, version-only S3, empty/conflicting Azure sources, malformed URLs and invalid argument counts.
|68|Public documentation|Pass|No new public method; buildDocument's existing documented input-error contract covers the checks; CLI usage now documents the required file.
|69|QPP flags|N/A|No QPP method changed.
|70|Security|Pass|No credentials added or values echoed; synthetic URL signature only; fixture body is no longer interpreted as a format string.
|71|Correctness|Pass|Full affected contract suites pass in AST/IR/JIT/tiered, including valid HTTP(S) URLs and encoded query bytes, plus importer and qualification checks.
