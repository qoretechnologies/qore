Binary module metadata ownership audit
======================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: the internal QoreBuiltinModule metadata owner, loader handoff, producer fixture, metadata copy tests, and release notes. Validation uses /usr prefix and the uninstalled Debug runtime through LD_LIBRARY_PATH=build-debug. Results: qore-packaging/results/core-module-metadata-fixed.json.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 9. Copyright 2026 on all new files
     - Pass
     - All changed sources and the audit use 2026 copyright.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 13. %modern directive present
     - Pass
     - The affected Qore test uses %modern without redundant warning directives.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - The existing qtest retains executable mode.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - QUnit and cppapisrc use relative in-repository source paths; fixture binary modules resolve from build-debug/modules.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - Only same-repository test binary modules and QUnit are required.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 24. No blocking operations without cancellation support
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 34. Response/output types use private Fields
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The owning RAII member fixes the unreleased module metadata; no suppression or diagnostic exception is used.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - A ReferenceHolder takes ownership only during member initialization. Its ExceptionSink outlives the holder, including constructor unwinding.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Metadata is immutable after initialization and queried with existing copy-on-write hashes. No new shared mutation is introduced.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - The constructor takes a typed ReferenceHolder<QoreHashNode>&; tests use typed hashes/lists.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Ownership is transferred without copying metadata; queries retain existing behavior.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Existing negative module resolution cases and absent metadata in the consumer module pass alongside the new metadata case.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Release notes document ownership cleanup and unchanged copy-on-write queries.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No corresponding module, data-provider, API, I/O, or loop implementation is added or modified.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No filesystem/network access, credentials, or new untrusted format strings are introduced.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - An unpatched installed-core fixture leaks 480 bytes on shutdown. Patched Debug suites pass, including 8 module API cases/46 assertions; Valgrind reports zero errors, lost bytes, or suppressions.
