Parquet read API compatibility audit
====================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: df_parquet.cpp, parquet-read-api.qtest and its test documentation,
qore.spec-multi test registration, and the DataFrame release-note entry.

Functional qualification: 45 executed cases and 445 assertions across host
Release/Debug plus Fedora (Arrow 23), AlmaLinux (Arrow 17), and Leap (Arrow 25).
The existing suite is selected with --include=parquetTest; its other 38 cases
are excluded deliberately and are not counted as executed.

Memory qualification: five runs of all eight new cases retain exactly one
already-approved PCRE2 JIT report in QUnit's \.qm$ script-name check, proven
by call tracing. Zero definite, indirect or possible losses; zero other memory
errors. The previously approved GCC/Valgrind TLS DW_AT_abstract_origin notice
also remains visible in the unchanged runtime on four configurations.
No suppressions or production flag changes were used.

Evidence is retained in qore-packaging/evidence/parquet-read-api-20261008.json.
Initial harness failures (read-only bind mount, incorrect QUnit option,
missing RPM link environment and Fedora Valgrind tool) are retained separately;
they are not treated as passing tests. This audit covers the isolated module
change; complete canonical RPM and native architecture gates remain separate.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 9. Copyright 2026 on all new files
     - Pass
     - Source, regression and test documentation carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 13. %modern directive present
     - Pass
     - parquet-read-api.qtest starts with %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - The regression is executable (mode 100755).

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The local qlib path precedes explicit relative QUnit.qm and Util.qm requirements.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - All three requirements are supplied by Qore itself; dataframe is the in-tree binary module.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - The change dispatches on an already-open Arrow FileReader; no new path or file-opening operation. The public wrapper retains its FILESYSTEM domain.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - The test writes and corrupts its own per-process temporary Parquet fixture and removes it on exit.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - Pass
     - The existing readParquet entry and conversion loops retain qore_check_cancel; no deprecated cancellation API is introduced.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 24. No blocking operations without cancellation support
     - Pass
     - The same four bulk reader operations are selected, with the same existing cancellation boundaries. No additional blocking phase is introduced.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 34. Response/output types use private Fields
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The supported Result overloads are used starting at their introduction in Arrow 24. Older supported SDKs keep their public API; no warning suppression or fallback stub.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Result owns its shared table until status is checked. A successful table is moved into the existing shared_ptr; failed reads return through the existing ExceptionSink path. Reader/file/table owners retain RAII.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - The lambda captures only invocation-local state and adds no shared mutable state.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - The lambda has an explicit arrow::Result<std::shared_ptr<arrow::Table>> return type; tests use the existing typed Parquet option hashes.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - The same reader is called once. Successful table ownership is moved without copying column data.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Four corrupted-page tests verify the expected Qore exception and recovery; existing Parquet tests retain argument validation coverage.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Release notes describe the compatibility change. parquet-read-api.rst explains all branches and local test commands; existing public API examples and exceptions remain accurate.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - Not applicable: this change adds no module, QPP class/public method, DataProvider schema or action, JNI dependency, network operation or C++ loop. Existing declarations and build registrations are unchanged.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No new format strings take user-controlled formats; no manual pointer arithmetic, buffer access, credentials or network operations are added.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - All five configurations pass eight new cases/30 assertions plus the existing Parquet case/59 assertions. Original Arrow 25 compilation fails at exactly four deprecated calls; fixed compilation is clean. Five traced Valgrind runs have no lost allocations or unclassified errors.
