Operator root-effect audit
==========================

Copyright 2026 Qore Technologies, s.r.o.

Scope: four existing internal operator template predicates, their actual-class C++/Python regression, documentation, and RPM test registration. Full audit: 14 Pass, 48 N/A, 0 Fail. Evidence: qore-packaging/evidence/operator-root-effect-20261008.json. Final full RPM/native ARM/installed gates remain separate.

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 9. Copyright 2026 on all new files
     - Pass
     - The modified header and all new tests/documentation carry Copyright 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 13. %modern directive present
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 14. Executable permission set (chmod +x)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - Pass
     - The production change queries C++ inheritance only. Test-harness compiler/filesystem work is outside the interpreter API.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - Pass
     - No production or fixture network operation added.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - No production loop added. The native operand loops each have four iterations; all shapes and hierarchies are explicitly bounded.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 24. No blocking operations without cancellation support
     - Pass
     - No blocking operation in the changed C++ methods; the fixture runs inline queries and normal object destruction.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 34. Response/output types use private Fields
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Use the language-level pointer convertibility trait to handle known public lvalue bases and preserve dynamic_cast in every other case. No suppression, flags change, substitute runtime implementation or workaround.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Production queries cannot allocate or throw. ValueHolder retains operand ownership; real operator destructors release their reference even when assertions throw. Native lifetime counters and five Valgrind runs prove zero lost allocations.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Production change has no mutable state. Fixture counters belong to a single main-thread execution; objects are never published to another thread.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Const pointer convertibility exactly covers the unambiguous public upcast case; the other branch retains RTTI, including multiple-inheritance cross-casts. No layout or virtual signature change.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Known upcasts become an explicit constant; other cases use the same RTTI operation. No copy, allocation or extra runtime traversal.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Tests compare actual root predicates to independent RTTI, cover negative plain-node results and ignored return values, and reject old source with eight exact address errors per compiler. No parser/evaluator behavior is changed.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - operator_root_effect.rst documents behavior, native fixture limitations, Release/Debug/Valgrind commands, and RPM qualification scope. This is an internal diagnostic correction with unchanged public behavior.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No new Qore module, QPP class, DataProvider, Qore script, unbounded loop, external operation or JAR is introduced. This checklist item has no applicable construct in the scoped change.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No user-controlled format, external input, buffer operation or new runtime memory ownership. Fixture varargs use initialized QoreSimpleValue values.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - 64 actual-class cases and 449 checks pass per configuration: 2,245 native plus 2,245 Valgrind checks across five configurations, with zero errors/loss. All fixed compilations are silent; only the already approved unchanged-runtime DWARF diagnostic occurs in four Valgrind outputs.
