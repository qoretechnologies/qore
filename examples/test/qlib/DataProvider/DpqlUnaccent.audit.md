# Audit: DPQL unaccent support and Unicode round trips

Copyright (C) 2026 Qore Technologies, s.r.o.

Date: 2026-10-02.

Scope: `qlib/DataProvider/DpqlParser.qc`, `DpqlSerializer.qc`, the DataProvider
3.9 and Qore 3.0 release notes, `examples/test/qlib/DataProvider/Dpql.qtest`,
`design/dpql-syntax.md`, `design/dpql-integration.md`, and this report.

Applied the full audit-changes skill at
`/home/david/.codex/skills/audit-changes/SKILL.md`, including its module structure,
sandboxing, cancellation, and DataProvider checklist/development-guide checks.
All 62 checks are resolved: **24 Pass / 38 N/A / 0 Fail**.

DPQL already registers `DP_OP_UNACCENT` and evaluates it with
`<string>::unaccent()`. No new operator implementation is required.
Tests exposed three defects in the surrounding parser/serializer:

- Quote removal passed byte counts to character-based `substr()`, retaining a
  closing quote in multibyte literals and quoted field names.
- String serialization indexed characters up to a byte count, reading beyond the
  final character and raising `RUNTIME-TYPE-ERROR`.
- Embedded double quotes were serialized with two backslashes instead of one,
  preventing the resulting expression from being parsed correctly.

Quote removal now excludes the final character explicitly. Escape decoding and
encoding use lazy character iterators, and double quotes receive one backslash.
The test suite reproduces the failures and covers unaccent parsing/evaluation,
case preservation, lowercase composition, encoding preservation, null defaults,
validation diagnostics, completion, record searches, Unicode/ASCII escapes,
quoted fields, key access, non-UTF-8 source, and malformed input.

Validation used the rebuilt Release `build/` runtime and local modules, with
Qore debugging enabled. The build prefix is `/usr`, matching `/usr/bin/qore`.
No C++ source changed; Valgrind is not required.

```sh
cmake --build build --target DataProvider-qmod -j4

env LD_LIBRARY_PATH=build \
  QORE_MODULE_DIR=build/modules/reflection:build/modules/i18n:build/modules/json:build/modules/yaml:build/modules/logger_bin:qlib \
  build/qore --enable-debug examples/test/qlib/DataProvider/Dpql.qtest
```

The same environment and command ran `DataProviderExpressions.qtest` and
`DpqlActionSession.qtest`.

|!Suite|!Cases|!Assertions|!Result
|Dpql|49|1765|Pass
|DataProviderExpressions|28|313|Pass
|DpqlActionSession|19|61|Pass
|Total|96|2139|Pass

All final test runs completed without emitted warnings or errors. Expected DPQL
diagnostics are asserted inside negative tests. The final qmod rebuild, generated
release-note conversion, documentation cross-reference checks, and
`git diff --check` passed. Initial CMake regeneration reported optional ngtcp2
QuicTLS/LibreSSL backend warnings because those backends were unavailable; the
configured OpenSSL build succeeded.

|!Check|!Status|!Evidence
|1. Module catalog entry|N/A|No new module; DataProvider already has a module catalog entry.
|2. Core/module release notes|Pass|The core 3.0 and DataProvider 3.9 release notes describe Unicode parsing, serialization, and double-quote escaping fixes.
|3. CMake module registration|N/A|Existing DataProvider module and qmod target; no new registration.
|4. QMOD registration|N/A|No new module or QMOD target.
|5. Lowercase introduction section|Pass|Existing main module uses dataproviderintro as its first section.
|6. Modern module directives|Pass|DataProvider.qm uses %modern without redundant modern-mode directives.
|7. No directives in separated classes|Pass|DpqlParser.qc and DpqlSerializer.qc have no file-level parse directives.
|8. No deprecated includes|Pass|No %include is introduced.
|9. 2026 copyright|Pass|The new audit has a 2026 copyright; changed module/class notices already end in 2026.
|10. Directory-module layout|Pass|DataProvider.qm remains under qlib/DataProvider/.
|11. No duplicate module entry|Pass|No duplicate DataProvider.qm is added.
|12. QPP namespace alignment|N/A|No QPP class changes.
|13. Modern test directive|Pass|Dpql.qtest uses %modern.
|14. Executable test permissions|Pass|Dpql.qtest retains executable mode 100755.
|15. Local module path and relative requirements|Pass|Dpql.qtest prepends local qlib before relative requirements for all in-repo Qore modules.
|16. External dependency handling|Pass|No new external module dependency; reflection is bundled and remains a hard requirement.
|17. C++ filesystem sandboxing|N/A|No C++ or filesystem operations changed.
|18. C++ network sandboxing|N/A|No C++ or network operations changed.
|19. Sandbox helper usage|N/A|No external resource access requires a sandbox helper.
|20. Qore resource access|Pass|Changes use string and in-memory provider operations, with no File/Dir/Socket/HTTPClient access.
|21. Cancellation in long C++ loops|N/A|No C++ loops changed.
|22. Current cancellation API|N/A|No native cancellation API changed.
|23. Cancellation check frequency|N/A|No native loop requires a check interval.
|24. Blocking-operation cancellation|Pass|No blocking operations; Qore foreach retains runtime cancellation behavior.
|25. Action names and descriptions|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|26. Action options|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|27. Action output types|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|28. API action request support|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|29. Find-action options and record type|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|30. Scheme action path/class rules|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|31. Single-key hash slices|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|32. Typed request/response classes|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|33. Public request Fields|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|34. Private response Fields|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|35. Field display name/type/description|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|36. Input examples|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|37. Typed allowed values|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|38. Sensitive password fields|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|39. AppGroup enums|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|40. App logo resources|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|41. Business-facing app descriptions|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|42. App display names|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|43. Plain short descriptions|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|44. Markdown descriptions|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|45. Business-language descriptions|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|46. Quoted boolean/missing literals|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|47. Quoted field/option references|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|48. Long description structure|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|49. FactoryMap registration|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|50. Record-type method signature|Pass|The reused test provider has the required private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto>) signature.
|51. Committed dependency JARs|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|52. JAR install rules|N/A|No action/app registration, provider schema, factory, or JNI dependency changes; DataProvider.qm changes only release notes.
|53. No workarounds or stubs|Pass|Fixes byte-count/character-offset mismatches and excess double-quote escaping at their source; no workaround, stub, TODO, or FIXME.
|54. Exception safety|Pass|Only managed Qore values and a local iterator are used; exceptions propagate without resource leaks or shared-state changes.
|55. Thread safety|Pass|All added parsing state is local to each call; no mutable shared state.
|56. Type safety|Pass|Typed strings, bool, hashes, expression hashdecls, and completion/diagnostic lists; no new untyped API.
|57. Performance|Pass|Quote removal uses character-relative bounds. Escaping and unescaping use lazy character iteration; strings without escapes return directly during parsing.
|58. Error handling|Pass|Tests cover empty and malformed input, wrong arity/types, and null handling; existing tokenizer rejection remains intact.
|59. Documentation and examples|Pass|Syntax and integration guides give working accent/case/null examples and provider-support caveats; both release-note locations updated. No new public method.
|60. QPP flags|N/A|No QPP methods changed.
|61. Security|Pass|No new I/O, credentials, dynamic code evaluation, user-controlled format strings, or raw buffer accesses.
|62. Correctness|Pass|Regression suite verifies concrete expected Unicode results, round trips, encoding, escapes, diagnostics, completions, and record filtering.
