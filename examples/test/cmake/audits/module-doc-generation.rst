Module documentation generation review
======================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: QPP enum documentation and invalid-enum error propagation; the explicit
QORE_GENERATE_JAVA_BINDINGS CMake option and associated tests/documentation.
HttpServer source changes are outside this review.

Validation: all 64 CMake/documentation tests pass. Both enum regression tests
also pass with QPP under Valgrind, including four rejected declarations; no
memory errors or lost allocations are accepted. Doxygen resolves the generated
integer/string enum values with warnings treated as errors.

.. list-table:: Full audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No %include usage (deprecated for modules)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Copyright 2026 on all new files
     - Pass
     - New Python regression test and review carry 2026 notices.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - %modern directive present
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Executable permission set (chmod +x)
     - Pass
     - The new Python integration test is executable; no Qore tests in this scope.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No blocking operations without cancellation support
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Response/output types use private Fields
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Password/secret fields have "sensitive": True
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Repair enum serialization and propagate its existing validity result; explicitly select requested Java output.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - The enum is owned by unique_ptr until insertion in the owning source list succeeds; invalid input exits before output creation.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - No concurrent or shared-state behavior added to the single-process generator.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - The new declaration owner is unique_ptr<Enum>; CMake uses a boolean cache option.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - One newline per documented enum member and one validity check per declaration; no new runtime hot path.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Missing enum name/brace, unsupported base type and premature EOF now return failure without output artifacts.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - BUILDING gives the no-JNI HTML command; release notes describe enum anchors, rejected inputs, and the Java option.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No new Qore module, QPP API class, DataProvider or runtime path in this build-tool change.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Generated labels use constant fprintf formats; test paths are temporary and no credentials are used.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Real QPP and Doxygen verify explicit, implicit, undocumented and string enum values; the existing Java invocation and disabled branch both build.
