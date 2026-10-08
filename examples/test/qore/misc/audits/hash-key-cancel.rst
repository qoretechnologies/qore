Hash key scan cancellation audit
================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: QoreHashKeyHelper.h, its native/Qore regression, CMake target, cancellation design and release notes. All 62 checks reviewed: 21 Pass, 41 N/A, 0 failures. Debug and Release qualification: qore-packaging/evidence/core-key-cancel-postpull-20261008.json. Only the already-approved optional ngtcp2 backend notices and GCC/Valgrind DW_AT_abstract_origin diagnostic remain; no new diagnostic exception is requested.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 9. Copyright 2026 on all new files
     - Pass
     - The helper, both new tests and this audit carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 13. %modern directive present
     - Pass
     - hash-key-cancel.qtest uses %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - The qtest is executable (0755).

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The local qlib path precedes a hard relative import of ../../../../qlib/QUnit.qm.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - QUnit is delivered with Qore; no external module dependency is introduced.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - No production filesystem operation is added; test subprocess launch selects the native fixture explicitly.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - No network operation is introduced.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - No File, Dir, Socket or HTTPClient operation is added to Qore code.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - Runtime constructors provide their exception sink to the chunked scan. The outer loop checks cancellation each chunk and the inner loop is bounded to 100 bytes. The existing non-throwing invariant-only form remains separate from runtime constructors.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - Pass
     - The new check uses qore_check_cancel; no deprecated cancellation API is added.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - Pass
     - At most 100 byte inspections occur between checks. size - i bounds the chunk without addition overflow.

   * - 24. No blocking operations without cancellation support
     - Pass
     - No production blocking operation is introduced. Native fixtures request cancellation synchronously with no sleeps or polling.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 34. Response/output types use private Fields
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The missing cancellation point is fixed in the scanning operation itself. No production flag, warning filter, feature disablement or workaround is added.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Both constructors stop after scan cancellation and preserve an empty invalid helper. Runtime hash APIs clear existence outputs on errors. Test-owned Qore objects use ReferenceHolder/ProgramHelper; cancellation/interrupt guards clear state on exception exits.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Production state remains stack-local; existing cancellation APIs own synchronization. The fixture counter is used only by its single test thread.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Offsets and counts use size_t; byte inspection uses unsigned char; fixture objects and exception sinks remain strongly typed.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Scanning remains linear without copying or allocation. Default-encoding/inline paths retain constant-time selection; cancellation cost is amortized over 100 bytes.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Tests cover empty keys, chunk boundaries, non-ASCII bytes, initially true/false existence flags, thread cancellation, program interruption, cleanup deferral and recovery.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Internal API comments describe cancellation and the invariant-only predicate. The durable cancellation design and release notes document implemented behavior.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No corresponding new installed Qore module, QPP class, DataProvider registration, JNI component, sandbox I/O helper or QPP flag change in this scope.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - The chunk length never exceeds the remaining input. Tests verify input bytes survive cancellation; no credentials or new external I/O are introduced.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Both builds pass 172 new checks, 348 lookup checks and 19 Qore cases/3449 assertions, including all four execution modes and AOT. Four Valgrind runs have zero memory errors or lost allocations. The original library reproducibly completed a 1 MiB lookup with cancellation pending.
