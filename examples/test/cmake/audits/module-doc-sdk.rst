Installed documentation SDK audit
=================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: CMakeLists.txt, cmake/QoreConfig.cmake.in, cmake/QoreMacros.cmake,
doxygen/Doxyfile.in, documentation and test_module_doc_sdk.py on 2026-10-01.
Method: /home/david/.codex/skills/audit-changes/SKILL.md.
This is a code review record, not a release qualification or publication approval.

No C++, QPP, .qm/.qc modules, DataProviders or .qtest files change in this core fix.
Root cause: installed external-module builds lacked the language tag index and
Doxygen searched a source image directory even when that directory did not exist.
The generated index installs optionally with the SDK after docs are built;
CMake preserves module TAGFILES and adds the index only when present.
The final two-phase pass appends quoted child indexes instead of replacing
existing language and caller-provided references. Existing
source docs/doxygen image directories are quoted, including spaces in paths.

Validation: 56 CMake regression tests pass, including real Doxygen output with
language and module cross-references, absent language index, absent image paths,
and paths containing spaces. Full Release configuration uses the installed /usr
prefix. External sysconf, magic and msgpack integration exercises the installed
helper and template; module-local documentation corrections are separate changes.
Valgrind does not apply to this core change, which contains no C++.

.. list-table:: Complete skill checklist
   :header-rows: 1
   :widths: 48 8 44

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No %include usage (deprecated for modules)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Copyright 2026 on all new files
     - Pass
     - New helper, test and documentation copyright statements use 2026.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - %modern directive present
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Executable permission set (chmod +x)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No Qore runtime file or network operations in this CMake/Python documentation change.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No blocking operations without cancellation support
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Response/output types use private Fields
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Password/secret fields have "sensitive": True
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No corresponding C++/Qore module, DataProvider, QPP or .qtest changes.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - SDK now exports the real generated language index. No warning filters, fake tags, or network downloads.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - N/A
     - No native allocations or exception-sink operations. Temporary Python test directories use context-managed cleanup.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - N/A
     - Helpers are single-process packaging utilities with no shared mutable runtime state.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - N/A
     - CMake configuration and Python test code only; no Qore or native runtime types changed.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Configuration checks two image directories and one optional tag file; no source-tree scans.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Missing SDK index and image directories are covered; CMake and Doxygen failures propagate. Real Doxygen runs use WARN_AS_ERROR.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Release notes and module-structure guide describe index installation, tag/URL overrides, and an offline example.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No QPP changes.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Paths are quoted and subprocess arguments are passed as arrays. No credentials or fetched documentation.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - All 56 CMake tests pass, including real HTML link targets, caller TAGFILES preservation, missing-index behavior and paths with spaces.

