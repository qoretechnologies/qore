BSON array compatibility and cancellation audit
===============================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: MongoDB BSON-array API compatibility and cooperative cancellation; native regression, CMake/RPM test registration and release notes. No new Qore module, QPP class, DataProvider, JAR, parser or Qore-language test. All 62 skill checks reviewed: 17 Pass, 45 N/A, 0 Fail. Evidence: qore-packaging/evidence/bson-array-20261008.json. Final compile logs are clean; configuration retains only the already approved ngtcp2 alternative-backend messages, and four Valgrind runs retain only the already approved unchanged-runtime TLS debug-symbol notice. This audit does not claim MongoDB server, full rebuilt RPM or native ARM qualification.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - Pass
     - Core and MongoDB release notes describe supported libbson APIs and cancellation behavior.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 9. Copyright 2026 on all new files
     - Pass
     - New native test, test documentation and this audit carry Copyright 2026; edited converter updated through 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 13. %modern directive present
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 14. Executable permission set (chmod +x)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - Changed converter and native test perform in-memory BSON operations only; no filesystem access is added.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - No socket or database-server operation is added. The regression uses actual libbson without a MongoDB server.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - The changed array loop checks cancellation at entry and every 100 elements. Every unbounded new fixture loop calls checkCancel at the same interval; other loops have at most seven iterations.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - Pass
     - Production and fixture checks call qore_check_cancel, preserving both thread cancellation and sandbox interruption.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - Pass
     - Tight array and fixture loops check every 100 iterations. There is no new blocking or expensive I/O loop.

   * - 24. No blocking operations without cancellation support
     - Pass
     - Only in-memory conversion is changed; no blocking operation is introduced.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 34. Response/output types use private Fields
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Uses the supported 2.3 API and the original API on older libbson. The old function directly delegates to the new one upstream. No stub, suppression or altered compiler policy. Actual generated ObjectId code supplies class symbols.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Caller-owned BSON buffers retain their original ownership. Failed partial documents are discarded by callers. Fixture Document, ReferenceHolder, CancelReset and InterruptReset guards clean up during exceptions. Invalid encodings and cancellation recovery pass Valgrind with zero lost allocations.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - The converter adds no shared mutable state; thread/program requests use the established runtime cancellation API. Fixture counter and BSON values are confined to its single thread.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Typed Qore list/hash holders, bson iterators and enums are retained. The index has its existing size type; no unchecked reinterpretation or new C-style cast is added.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Linear traversal is retained; the added work is one inexpensive periodic cancellation check, without copies of the array.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Tests cover empty arrays, key boundaries, nulls, nesting, invalid encodings after buffer growth, pending cancellation, cleanup deferral, sandbox interruption and recovery. Existing error returns propagate unchanged.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Test documentation gives Release/Debug and Valgrind commands, compatibility behavior and partial-document ownership. Both release notes are updated. No new public Qore method is introduced.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No applicable construct is introduced or changed: no new user module/QPP class, Qore-language test, DataProvider registration, JAR, or filesystem/network operation requiring a sandbox helper.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Sequential keys are formatted with the existing constant format. Actual libbson validation checks type, bounds and content. No credentials, user-controlled format string or external access is added.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Each of five configurations passes 10517 native checks and the same 10517 under Valgrind, with zero errors and zero definite/indirect/possible loss. Original-source control ignores cancellation; fixed source rejects it before parent mutation. Leap original compiler control reports exactly one deprecated API error; fixed compile is clean. All 24 distribution RPM metadata methods pass.
