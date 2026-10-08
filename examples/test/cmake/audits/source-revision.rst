Source revision and standalone fingerprint audit
================================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: archive revision export, UpdateGitRevision.cmake, create_git_revision(), digest list policy, Python regression fixtures, BUILDING and release notes. All 62 audit-changes entries reviewed:10 Pass, 52 N/A, 0 Fail. No C++ or Qore runtime changes; Valgrind is not required for this scope. Distribution evidence: qore-packaging/evidence/core-cmake-source-20261008.json.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 9. Copyright 2026 on all new files
     - Pass
     - New archive metadata, attribute rule, revision helper, tests and audit carry Copyright 2026; updated CMake macros now retain that year as well.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 13. %modern directive present
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 14. Executable permission set (chmod +x)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 24. No blocking operations without cancellation support
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 34. Response/output types use private Fields
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The revision is exported by Git and read from its immutable marker. No fabricated identifier, parent-repository fallback, warning suppression or compiler-flag workaround is used. The digest script explicitly selects the intended empty-list policy.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Argument, source-directory, Git command, archive marker and hash checks finish before the revision header is written. Negative tests require a preexisting header to survive failures unchanged; configure propagates bootstrap errors.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Output belongs to the configured build directory and source metadata is read-only. Independent builds do not share generated headers; linked worktrees read their own HEAD.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Only 40/64 lowercase hexadecimal object IDs are accepted as revisions. The unexpanded marker and absent metadata explicitly produce unknown; ambiguous/malformed markers fail.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Archives avoid invoking Git. Unchanged revision and digest content preserve output timestamps; the digest success stamp is refreshed without sleeps or unnecessary downstream rebuilds.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Tests cover empty/missing arguments, nonexistent source, invalid/multiple markers, missing Git, broken HEAD, linked worktrees, nested archive extraction, stale outputs, blank digest input lines and changed compiler inputs.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - BUILDING documents Git checkout/archive/unversioned behavior and a real archive command. Release notes describe implemented metadata and standalone-policy changes.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No new installed Qore module, QPP class, DataProvider, JNI dependency, C++ implementation or Qore test in this CMake/Python metadata scope.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - The generated C string accepts only validated hex or the fixed unknown value; no enclosing repository can inject the wrong commit. Tests run real Git fixtures in temporary directories without credentials or network.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - All 12 tests pass on Fedora CMake 4.3.0, Leap 3.31.7 and AlmaLinux 3.31.8: 36 methods total, warning-free. Negative controls prove current RPM archives had an empty header and the old helper borrowed an unrelated parent commit.
