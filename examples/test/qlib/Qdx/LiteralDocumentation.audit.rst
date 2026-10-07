Qdx literal documentation audit
===============================

Copyright 2026 Qore Technologies, s.r.o.

Scope: Qdx comment generation/collection, AstProcessor regressions and release notes on develop.

Summary: 15 Pass, 47 N/A, 0 Fail. Validation: cmake --build build --target Qdx-qmod -j2;
build/qore -b --enable-debug on all three Qdx suites in AOT and isolated source modes,
with LD_LIBRARY_PATH=build and local astparser/XML module paths. Evidence is recorded in
the packaging repository at evidence/qdx-develop-20261007.json. The source-only run uses
an isolated copy without Qdx.qmod; other modules and the main checkout remain intact.

No C++ code changed, so Valgrind is not required by the project instructions.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 9. Copyright 2026 on all new files
     - Pass
     - Qdx and all new test code carry 2026 copyright.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 13. %modern directive present
     - Pass
     - AstProcessor.qtest declares %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - AstProcessor.qtest remains executable (100755).

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - Relative Qdx, QUnit and FsUtil imports follow the local module-path prepend.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - astparser is a bundled module and uses its hard import; no new external module import is added.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - No new production I/O. Doxygen integration uses owned TmpDir/File objects and the existing shellQuote helper.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 24. No blocking operations without cancellation support
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 34. Response/output types use private Fields
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Root-cause fixes for numeric-entity rendering, literal-block contents and escaped-command counting; no suppression or relaxed checks.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - All parser state is local and managed by Qore; TmpDir/File objects clean up on exceptions. Test command errors are asserted.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Only local state and immutable input are used; no shared mutable state is added.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Typed strings, list<string>, integer positions and nullable literal-block state. Existing public API unchanged.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Each line is decoded once through splitChars; indexed list access and at most twelve-character command lookahead avoid repeated UTF-8 prefix scans.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Tests cover both introducers, escaped and longer commands, inline and multiline blocks, end-of-line boundaries and 12,288 Unicode codepoints. Three original-code controls fail.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Module/core release notes updated; realistic MIME and URL examples are checked in actual Doxygen HTML.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No new module registration, C++ code, public API, DataProvider action/type or JAR is introduced in this scoped change.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No production filesystem/network operations or credentials. Fixture paths are temporary and shell-quoted.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - 68 cases / 372 assertions pass in both AOT and source modes (136 / 744 total), with no warnings or skips. Original code fails exactly three new regressions.
