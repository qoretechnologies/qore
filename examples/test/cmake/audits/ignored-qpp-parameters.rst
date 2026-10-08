Ignored QPP parameters audit
============================

Copyright 2026 Qore Technologies, s.r.o.

Scope: four QPP sources, eight intentionally ignored parameters in six existing methods, actual generated-wrapper regression, documentation and RPM test registration. Full audit-changes review: 15 Pass, 47 N/A, 0 Fail. No new module, QPP class, DataProvider, Qore script or JAR. All final compiler outputs are clean; four Valgrind runs retain only the previously approved unchanged-runtime TLS debug-symbol diagnostic. Focused generator and actual wrapper execution do not replace final complete RPM/native ARM/installed qualification. Evidence: qore-packaging/evidence/ignored-qpp-parameters-20261008.json.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 9. Copyright 2026 on all new files
     - Pass
     - All new native/Python test and documentation files carry Copyright 2026. Existing edited QPP sources already include 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 13. %modern directive present
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 14. Executable permission set (chmod +x)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - Production change only omits unused argument bindings; no filesystem operation is added. Python file/compiler operations are confined to the test harness.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - No network or database operation is added. Abstract datasource wrappers always raise their existing exception.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - The native test loops have fixed bounds of two, six and nine entries; no changed C++ loop can exceed 100 iterations.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 24. No blocking operations without cancellation support
     - Pass
     - Changed wrappers perform bounded argument-free rejection or existing in-memory encoding; no blocking operation is added.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 34. Response/output types use private Fields
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No relevant construct changes: existing class and module registration, no Qore-language source/tests or DataProvider/JAR additions, no unbounded C++ loop or public method addition.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Uses the existing QPP [doc] parameter facility, already used for ignored interface arguments. No compiler warning flag is disabled, no stub method implementation is introduced, and all six method bodies are unchanged.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Native fixtures use ReferenceHolder and SimpleRefHolder for Qore values, clear checked expected exceptions, and destroy RuntimeConfig before qore_cleanup. All five Valgrind runs have zero memory errors and zero definite/indirect/possible lost allocations.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Production adds no shared state. The test check counter and inputs are confined to its single execution thread.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Parameter types and typed list/hash signatures remain identical. Actual generated wrappers are compiled; the datasource private type is only forward-declared and never dereferenced by these rejecting bodies.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Unused argument extraction is omitted without changing the call ABI or public argument conversion. No added runtime branch, allocation, copy or complexity.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Existing error codes for all three abstract bulk-load operations and the nonconstructible controller are checked, including both boolean values. Base64 URL checks cover empty and encoding-boundary inputs, embedded NUL, raw high bytes, UTF-8 and six ignored line lengths.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Test README explains all eight ignored parameters, generator comparisons, Release/Debug and distribution flags, native/Valgrind execution and limits. Public generated documentation is byte-identical; no public behavior change requires a release-note entry.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - Pass
     - Complete metadata, stubs and native method registration remain identical, including CONSTANT/NAMED_ARGS, domains, parameter types/defaults and names.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No credentials, format-string change or external access is added. Generated wrappers use the original typed API; fixed fixtures compile with -Wall -Werror under all tested flags.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Five configurations each pass three generator/compiler methods and 177 native checks, repeated under Valgrind. Baseline wrappers reproduce exactly eight unused-variable errors; fixed wrappers compile cleanly. Complete metadata, stubs, method registration and documentation match baseline. All 24 RPM metadata methods pass.
