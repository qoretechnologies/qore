Module documentation markup audit
=================================

Copyright 2026 Qore Technologies, s.r.o.

The shared Doxygen configurations disable Markdown. Markdown links, emphasis,
and inline-code delimiters in documentation comments therefore appeared literally,
without warnings. URL auto-linking could further disguise a broken link by making
only its URL clickable. Documentation prose now uses HTML anchors and formatting;
in-tree module links use lowercase intro targets and API links use qualified symbols.
Runtime application descriptions and executable example bodies retain their original text.

The rendered-documentation checker accumulates text across inline HTML elements,
so a URL auto-link cannot hide the surrounding Markdown. Block boundaries separate
paragraphs. Inline literal contents are ignored while the surrounding prose remains
checkable, including bold markers spanning code; preformatted examples stay excluded. Regressions cover
line reporting, literal examples, symlinked documentation trees, missing inputs,
the command-line prefilter, and actual Doxygen rendering with Markdown disabled.
The design guide documents the supported syntax and the post-build validation command.

Validation
----------

* All 39 tests in the seven documentation/CMake suites pass. The final six
  rendered-markup regressions pass with both Doxygen 1.9.5 and 1.16.1; tests assert
  the generated labels, formatting, and literal examples as well as diagnostics.
* All 153 affected Qore qmods and their initial/final documentation were rebuilt,
  together with affected native, language, and public C++ documentation. The latest
  runs of 354 distinct targets have no warnings or errors. Regenerated language
  inputs were byte-identical after copyright-only edits, so that successful build
  was retained instead of repeating it.
* Complete documentation builds passed for all 11 affected external repositories:
  git, grpc, jni, kalman, pdf, proj, ssh, ssh2, v8, vss, and xml. All 37 external
  documentation trees pass the rendered-markup check (5,754 HTML files).
* All 122 originally malformed links now have their exact intended labels and
  destinations; local targets exist with their anchors. Additional HttpClientIo
  and AgUi reverse API links resolve after their documentation-only final passes.
* Layout checks pass on 99 mainpages, 68 intro logos and 158 companion links.
  All 423 rendered initial-release entries contain only ``initial release``;
  the release-note source checker passes on 946 sources.
* The final rendered scan passes on 39,882 HTML files across Qore and all 37
  external documentation trees, with no unprocessed references, Markdown links,
  emphasis, inline code, or fenced blocks outside literal examples.
* 456 Qore/C++ source files preserve executable text, application
  descriptions and example bodies. Only documentation and copyright comments change.
  The AOT host example uses a supported C++ code block instead of a Markdown fence.
* A clean public C++ documentation rebuild removed obsolete generated pages for
  internal headers excluded by the current configuration. No internal-header
  implementation changes were needed. Qorus's composed pages for all 11 changed
  external modules point to their rebuilt local files.

.. list-table:: Complete audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - Pass
     - Existing module registration and intro targets remain; documentation-only peer dependencies support final-pass links.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - Pass
     - Existing module registration and intro targets remain; documentation-only peer dependencies support final-pass links.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No %include usage (deprecated for modules)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Copyright 2026 on all new files
     - Pass
     - The checker, regression tests, audit, and updated source copyright notices include 2026.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - %modern directive present
     - N/A
     - No Qore test scripts changed; Python regressions exercise rendered HTML and the checker CLI.

   * - Executable permission set (chmod +x)
     - N/A
     - No Qore test scripts changed; Python regressions exercise rendered HTML and the checker CLI.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - No Qore test scripts changed; Python regressions exercise rendered HTML and the checker CLI.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - No Qore test scripts changed; Python regressions exercise rendered HTML and the checker CLI.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - No blocking operations without cancellation support
     - N/A
     - C++ and Qore executable text is unchanged; edits are confined to documentation and copyright comments.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Response/output types use private Fields
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Password/secret fields have "sensitive": True
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - Pass
     - Automated before/after comparison confirms runtime application description strings are unchanged.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - Pass
     - Automated before/after comparison confirms runtime application description strings are unchanged.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - Documentation-only source edits; no new modules, runtime behavior, DataProvider registrations, or JARs.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - The fix uses supported source markup; Markdown remains disabled. No post-render rewrite or runtime workaround.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - No runtime allocation or ownership changes; the checker handles parser errors and missing paths through existing CLI behavior.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - No shared mutable state or threading changes. Each HTML parser instance owns its buffers.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - No runtime type changes. Parser text, line numbers, and findings retain their existing shapes.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Text is buffered per prose block; finding-to-source mapping advances through chunks monotonically.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Negative tests cover raw markup, missing paths, empty directories, and symlink cycles.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Supported markup and regression command documented in the implemented module design guide.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - No public C++ API signatures or QPP method flags change.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No credentials, network operations, or executable application strings are changed.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Actual Doxygen regression tests and rendered-page checks validate labels, destinations, and literal examples.
