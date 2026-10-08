Separated module path audit
===========================

Copyright 2026 Qore Technologies, s.r.o.

Scope: ModuleManager.cpp normalization, separated-module-path.qtest and corresponding design/release notes, reviewed after pulling 9f94a962f. All 62 checks reviewed: 16 Pass, 46 N/A, 0 implementation failures. Raw qualification: qore-packaging/results/core-postpull-20261008; post-pull Salesforce integration at module-grpc cdf5fd994e2baf46007bb643b59626e1a5434ea3 passes 34 offline cases/356 assertions in each mode; five credential-dependent live cases are skipped. Historical negative evidence remains in evidence/core-separated-path-fix-20261008.json. The user approved the exact diagnostics in evidence/core-separated-pcre-diagnostic-20261008.json and evidence/ngtcp2-backend-diagnostics-20261008.json on 2026-10-08. No production regex setting or warning filter changes.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 9. Copyright 2026 on all new files
     - Pass
     - New regression and audit carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 13. %modern directive present
     - Pass
     - The regression and generated module entry points use %modern; separated .qc fixture has no parse directives.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - The new .qtest is executable.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The test prepends local qlib before relative QUnit/FsUtil requires. Generated temporary-module imports intentionally exercise named search and explicit relative imports.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - Only Qore-owned QUnit/FsUtil dependencies; no external binary test dependency.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - No new direct filesystem operations: normalize the path already found and validated by the existing module lookup. Module sandbox behavior is unchanged.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - Test File/Dir operations are confined to TmpDir fixtures; cwd restoration uses on_exit and temporary directories are released even after assertions fail.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 24. No blocking operations without cancellation support
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 34. Response/output types use private Fields
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Fix the missed normalization at the separated-module search boundary, matching existing flat-source and AOT-fallback handling. No production flags, source lookup fallback or warning suppression added.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Existing ReferenceHolder and local QoreString ownership remain unchanged. Missing-entry tests verify failure; eight memory runs have no lost allocations.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - The normalized path is a local value under the existing module-loading synchronization. Sequential fixture methods restore the process cwd on every exit.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Typed string/bool/hash variables; no casts or new untyped callbacks.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - One linear normalization only after an existing directory and entry point are found; no extra search or I/O loop.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Missing module main is rejected; both explicit and search-based resolution remain covered, including spaces and dot segments.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Release notes describe the false module filename and resulting resource failures; durable design documents cwd versus importer-relative resolution.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No corresponding new installed module, QPP class, DataProvider, JNI, C++ loop/blocking operation or public API change in this loader scope.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No new permission bypass, format string, network operation or secret. Fixture-generated names and paths are trusted and bounded.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - On 9f94a962f both builds pass 25 loader cases/73 assertions and eight Valgrind runs with zero memory errors or lost allocations. Valgrind uses the separately documented regex interpreter setting; ordinary tests use default JIT. The original source reproduces the wrong module filename. Default-JIT diagnostic controls are retained separately; the user approved that exact diagnostic on 2026-10-08.
