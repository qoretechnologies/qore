Hash lookup error output audit
==============================

Copyright 2026 Qore Technologies, s.r.o.

Scope: local changes to lib/QoreHashNode.cpp, include/qore/QoreHashNode.h, CMakeLists.txt, release notes and the native regression after pulling 9f94a962f. All 62 checks reviewed: 19 Pass, 43 N/A, 0 implementation failures. Raw post-pull qualification: qore-packaging/results/core-postpull-20261008 and core-hash-docs-postpull-20261008. The previously approved GCC/Valgrind DW_AT_abstract_origin diagnostic remains visible. The user approved the two exact optional ngtcp2 backend configure diagnostics on 2026-10-08, as recorded in evidence/ngtcp2-backend-diagnostics-20261008.json. Full updated RPM/OBS qualification is a later gate.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 9. Copyright 2026 on all new files
     - Pass
     - The new native test, Qore wrapper, test guide and audit carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 13. %modern directive present
     - Pass
     - hash-lookup-native.qtest uses %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - The Qore wrapper is executable (0755).

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The local qlib path is prepended before the relative ../../../../qlib/QUnit.qm requirement.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - QUnit is delivered by Qore and uses a hard relative requirement; no external module is added.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - Production lookup changes have no filesystem operations; the native fixture only writes its test result.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - No networking is introduced.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - The test wrapper checks for and executes its local native fixture using existing runtime APIs; no production I/O is added.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - Production changes add no loops; native fixture loops are fixed at at most four iterations per level and are outside interpreter execution.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 24. No blocking operations without cancellation support
     - Pass
     - No blocking operation is added to the runtime. The wrapper waits for its finite native child through the existing process API.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 34. Response/output types use private Fields
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The remote 9f94a962f QoreHashKeyHelper is retained. This local fix assigns false to exists on conversion and invalid-member errors; it adds no alternate lookup or suppression.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - QoreHashKeyHelper owns converted storage; failures return before lookup. Native fixtures use ReferenceHolder, unique_ptr and TypedHashDeclHolder and check exception sinks.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - No shared state is added. Lookups retain existing const/read-only and caller synchronization contracts; fixture counters are single-threaded.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Existing typed QoreString, QoreValue, bool-reference and ExceptionSink APIs remain intact. Test values and encoding pointers are explicit types.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Only a bool output assignment is added on existing error paths. Key conversion and successful lookup costs remain those of the pulled implementation.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Both initial existence states, malformed encoding, missing keys, invalid hashdecl members, empty keys and recovery after errors are exercised.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Public API parameter/result/exception-sink contracts describe converted keys and false error outputs; the realistic account example compiles and runs with both libraries. Release notes and the fixture guide are updated.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No corresponding new module, QPP class, DataProvider, JNI, sandbox helper or runtime cancellation operation in this lookup scope.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Converted bytes remain live through the lookup; malformed UTF-16 is rejected. Fixture shell quoting escapes apostrophes, format strings are literal, and no credentials are present.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - On 9f94a962f, each mode passes 348 native checks, 18 Qore cases/2023 assertions across four hash suites, and clean native Valgrind. The remote encoding suite covers AST/IR/JIT/tiered/AOT. Historical negative controls reproduce the two remaining error-output defects; post-pull documentation examples also pass.
