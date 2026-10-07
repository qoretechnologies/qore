DateTime addition and signed-duration audit
===========================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: lib/DateTime.cpp, include/qore/DateTime.h, include/qore/intern/qore_date_private.h, CMakeLists.txt, release notes and the date-add-native tests. All 62 audit-changes checklist entries were reviewed. All relevant qualification checks are complete.

Validation completed on 2026-10-07
----------------------------------

* Debug and Release builds use the installed /usr prefix. Both build the
  native regression without compiler warnings.
* Each build passes 1,836 native addition cases, normally and under Valgrind.
  Valgrind reports zero errors and zero definite, indirect or possible lost
  allocations. Existing process-lifetime library caches remain reachable.
* Each build passes date-add-native, date, date-utc-offset and date_durations:
  15 reported cases and 1,899 assertions. The date suite explicitly skips its
  Windows-only case on Linux; no applicable case is skipped.
* The focused relative-time control fails against the original header and
  passes all 679 representable signed-duration boundary cases against the fix,
  including under UBSan and Valgrind. Native addition controls also fail
  against the original local library and all three original distribution SDKs.
* A compiler negative control proves that restoring the pointer argument is
  rejected by the explicit private constructor; the corrected source compiles.
* The new Qore wrapper passes 2 cases and 29 assertions under Valgrind, with
  zero lost allocations and only the previously approved external PCRE2 report
  described below. The native child is checked separately under Valgrind.
* The invoice example extracted from DateTime.h compiles with -Wall -Werror
  and produces the expected November payment date. Additional timezone-error
  and formatting suites pass 9 cases and 72 assertions without diagnostics.
* The new Qore test uses %modern, a relative in-tree QUnit requirement and
  executable permissions. CMake builds its native fixture by default without
  installing the fixture. Whitespace and the complete staged diff were reviewed.

Existing diagnostic approvals
-----------------------------

The Debug library retains the exact GCC 16 / Valgrind 3.27 warning about a
missing DW_AT_abstract_origin for thread-local wrappers, already approved in
qore-packaging/evidence/core-close-rpm-qualification-20261002.json.

The QUnit startup path triggers the already approved Fedora PCRE2 10.47 x86_64
JIT suffix scan for the pattern ``\.qm$``. GDB confirms the same aligned vector
scan and intermediate branch before logical-end validation, on the qtest
filename. The date tests add no error context. This scope is recorded in
qore-packaging/evidence/zmq-pcre2-diagnostic-20261006.json. No suppression or
library workaround is introduced.

Raw commands, source hashes and result records are retained under
qore-packaging/results/core-date-final-verification-20261007.json and the
associated core-date and core-relative-sign result directories. Qualification
harness checks were corrected to recognize QUnit's singular test-case summary
and its explicit Windows-only skip. The formatting test was rerun successfully
with the required local XML binary-module path. These setup corrections do not
change the runtime or the test assertions.

Result: 18 Pass, 44 N/A, 0 Fail across all 62 checklist entries.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 9. Copyright 2026 on all new files
     - Pass
     - The new native test, Qore test, test guide and this report carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 13. %modern directive present
     - Pass
     - date-add-native.qtest begins with %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - date-add-native.qtest has mode 0755.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The local qlib path is prepended before the relative ../../../../qlib/QUnit.qm requirement.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - QUnit is delivered in this repository and uses a hard relative %requires. There are no external module requirements.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - The production changes perform only date arithmetic and allocation; they add no filesystem access.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - The production changes add no network access.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No production filesystem or network operation requires a sandbox helper.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - No File, Dir, Socket or HTTPClient operations are added. The test launches its trusted native fixture using a shell-quoted path.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - Production changes contain no loops. Bounded standalone native test enumeration runs outside a cancellable Qore program.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new runtime cancellation point is needed for constant-time arithmetic.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No production iteration or expensive repeated operation is added.

   * - 24. No blocking operations without cancellation support
     - Pass
     - Production additions are bounded arithmetic with no blocking operation. Test process execution uses the existing runtime backquote implementation.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 34. Response/output types use private Fields
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding module, QPP, DataProvider or JNI feature changes in this scope.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The operand is dereferenced at the faulty call and the private converting constructor is made explicit. Relative units receive mathematically equivalent sign normalization. No suppression, fallback or partial implementation is added.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - The DateTime result is owned by std::unique_ptr until the successful return; inputs remain borrowed and unchanged. Native fixtures use unique_ptr and check the timezone ExceptionSink. Failure paths unwind owned dates before qore_cleanup.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Arithmetic modifies the newly allocated result or the already-owned relative value. No new mutable shared state is introduced; the native test counter is used by its single main thread.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - The explicit private constructor prevents pointer-to-bool-to-date conversion. The bounded remainder uses int64 and explicit narrowing casts. Qore tests use typed nested lists.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Normalization adds a fixed number of integer operations and constant storage. DateTime addition still allocates exactly one result, with no additional date copy.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - The pointer overload documents its existing non-null precondition. Native negative controls detect the old wrong result; a compiler negative control rejects restoration of the pointer conversion. The test checks fixture errors before use.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Public overloads document input, ownership, allocation failure, timezone behavior and a monthly invoice example. The test guide explains local builds and memory checks. Release notes describe both behavior corrections.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No QPP method or flag changes.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No new buffer or index access is introduced. Time remainders stay within one hour before conversion; the hour adjustment moves toward zero. The trusted native executable path is single-quote escaped; no credentials are present.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Independent integer-microsecond expectations, calendar/DST fixtures, self-aliasing and input-immutability checks cover the addition paths. Signed-duration equivalence and subtraction are checked in Qore; the focused internal control includes representable INT_MIN/INT_MAX hour boundaries. Final test evidence is recorded above.
