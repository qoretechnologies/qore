HTTP response charset review
============================

Copyright 2026 Qore Technologies, s.r.o.

Scope: HttpServer and HttpServerUtil charset generation and the new response
charset regression. Documentation generation is reviewed separately.

Validation: rebuilt both AOT module targets, then ran the charset, request
encoding and response framing suites with -b --enable-debug. All 18 cases
and 431 assertions pass without warnings. The fixture includes sysconf during
AOT compilation so its optional branch is available. No C++ changes.

.. list-table:: Full audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No %include usage (deprecated for modules)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Copyright 2026 on all new files
     - Pass
     - New test and review contain 2026 notices; existing notices retained.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - %modern directive present
     - Pass
     - The new Qore test uses %modern.

   * - Executable permission set (chmod +x)
     - Pass
     - The new qtest is executable.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The new test resolves all three required modules with explicit relative paths and sets the local qlib path first.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - The new test uses only standard Qore modules; the fixture supplies sysconf for the existing optional import.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - The regression creates isolated loopback sockets and terminates its server with on_exit.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No blocking operations without cancellation support
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Response/output types use private Fields
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Password/secret fields have "sensitive": True
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Canonical names come from the encoding registry; StringInputStream uses the same header path as in-memory text.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Managed Qore values own conversion temporaries; on_exit stops the test server even after an assertion or exception.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Only per-request values change; no shared state is added.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Typed method arguments and hashdecl response values; the test uses an explicit hash<auto> request context.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Canonicalization is bounded by the encoding registry lookup, with no added I/O or loops in response processing.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Explicit charsets remain unchanged; binary payloads receive no invented charset; invalid names retain registry errors.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Module release notes describe canonical response charsets and coverage of string stream bodies.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No new module/API class, DataProvider, C++ code or native cancellation path in this scope.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No credentials or user-controlled format strings; test listeners bind loopback with ephemeral ports.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Tests cover aliases, default charsets, explicit charset preservation, binary bodies, string streams and real HTTP responses.
