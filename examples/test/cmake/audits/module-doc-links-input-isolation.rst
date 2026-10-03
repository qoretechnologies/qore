Module-link and generated-input isolation audit
==============================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: replace hardcoded in-tree module index paths with lowercase intro targets,
link APIs to their qualified owners, declare documentation-only dependencies, and
isolate generated Doxygen inputs by module. Every changed line in 68 Qore module
sources is inside a documentation comment; no runtime implementation changes.

Root causes
-----------

* Relative URLs assumed obsolete layouts, bypassed tag validation, and sometimes
  linked a method label only to a module's index. Module links now use intro
  targets, while class and method links use their actual declaration owners.
* The in-tree CMake helper put every generated class file in one directory.
  ServerSentEventClient and ServerSentEventHandler each contain
  ``ServerSentEventConnection.qc``. The later initial pass overwrote the earlier
  module's input, so its final pass rendered the wrong class. Inputs now live in
  ``doxygen/qlib/<ModuleName>/``, matching the existing external-module layout.
* Malformed literal examples, outdated POP3 parameter names, and a YAML-RPC
  reference to a removed method produced additional warnings. Comments now match
  the current declarations; removed historical APIs remain plain code text.

Validation
----------

* The new same-basename regression fails deterministically before the CMake fix
  with an unresolved local class in the final pass. It passes after the fix,
  checks exact generated source content and class destinations, and repeats a
  parallel build. The fixture uses source and build paths containing spaces.
* All 37 tests in the seven documentation/CMake suites pass with CMake 4.3.0.
  The six ``test_user_module_resources.py`` tests pass with both Doxygen 1.9.5
  (used by the actual Qore build) and Doxygen 1.16.1 (the system default).
* All 57 affected qmods and their initial/final documentation were rebuilt,
  together with the language pages. All 154 latest target executions pass without
  warnings or errors. All 178 checked module/API links have existing destination
  files and anchors, and the source pages contain those exact generated links.
* Layout checks pass on 99 mainpages, 68 intro logos and 158 companion links.
  The Qorus AMQP and JSON pages resolve to the rebuilt local files; its ZIP guide
  contains a rendered intro link. The release-policy checker passes on 851 sources.
* The raw-reference scanner passes on 34,979 generated Qore HTML files. All 352
  checked initial-release entries in Qore render only ``initial release``.
* After rebasing onto the concurrent DataProvider/CSV additions, both qmods,
  their docs and the final language reference pass rebuild without diagnostics
  (six targets). All 888 DataProvider/CSV HTML files pass the raw-reference
  scan, and all 68 rebased module diffs remain confined to comments.
* Independently published external modules retain HTTPS links ending at their
  lowercase intro anchors; seven such targets are checked against locally
  generated pages. No live-site publication is part of this change.

.. list-table:: Complete audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - Pass
     - Existing module registrations are unchanged; documentation-only peer lists precede registration and preserve runtime/AOT dependencies.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - Pass
     - No new modules. Existing binary/provider intro logos and lowercase companion intro targets were verified in generated HTML.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No %include usage (deprecated for modules)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Copyright 2026 on all new files
     - Pass
     - The extended regression test and all changed Qore module copyright notices include 2026.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - %modern directive present
     - N/A
     - No qtest files changed. New Python test scripts are executable and use isolated source/build fixtures.

   * - Executable permission set (chmod +x)
     - N/A
     - No qtest files changed. New Python test scripts are executable and use isolated source/build fixtures.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No qtest files changed. New Python test scripts are executable and use isolated source/build fixtures.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No qtest files changed. New Python test scripts are executable and use isolated source/build fixtures.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No C++ changes in this Qore diff.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No C++ changes in this Qore diff.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No C++ changes in this Qore diff.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - All 68 edited Qore source files preserve executable tokens; only comments change.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No C++ changes in this Qore diff.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No C++ changes in this Qore diff.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No C++ changes in this Qore diff.

   * - No blocking operations without cancellation support
     - N/A
     - No C++ changes in this Qore diff.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Response/output types use private Fields
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Password/secret fields have "sensitive": True
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - Pass
     - No runtime app registration or resource changes in this follow-up.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new modules, QPP classes, runtime DataProvider registrations/types/actions, or JARs. The edited Qore sources have identical executable tokens.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Hardcoded layout paths and wrong API owners are replaced with tag references. Each module owns a generated-input directory, preventing identical class filenames from overwriting each other before final rendering. Final warnings remain enabled.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Python CLIs report file access errors and exit unsuccessfully. Test subprocesses have timeouts and temporary directories have scoped cleanup. No C++ or Qore runtime operations change.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Generated class inputs are isolated by module. Serial initial passes deterministically reproduce the former overwrite; final and repeated parallel builds verify the fix. Final renders preserve initial tags.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - No Qore or C++ type changes. Documentation-only dependencies are explicit lists distinct from runtime and AOT dependencies.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Tag collection visits each reachable dependency once. Only modules declaring documentation-only peers get another render pass.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Missing files, malformed initial notes, redundant navigation headings, absent logos, and unresolved final-pass links are covered by negative tests.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Module structure guide documents the implemented layout, logo, release-history and build rules. Invalid symbol references, misplaced method comments, and missing parameter docs are corrected.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No QPP or native API code changes in this Qore diff.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No network, credential, or runtime input processing changes. Paths and tool arguments are passed as separate arguments; CMake generated commands use VERBATIM.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - 37 tests across seven suites pass; the six resource/build tests also pass under CMake 4. Clean and repeated reciprocal-link rendering, immutable tags, disabled docs, unchanged AOT edges, paths with spaces, and deliberately broken final references are tested.
