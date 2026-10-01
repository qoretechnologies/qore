ONNX async pool statistics review
=================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: completion publication in OnnxModel.cpp, its private declaration,
public API notes and two regression cases in the existing ML suite.

The native Leap aarch64 RPM test observed async_completed == 1 after both
futures returned. Both success paths incremented the counter after waking
Future::get() waiters; dynamic rejection had the same ordering issue.
The shared resolver now holds the statistics mutex across promise publication
and outcome accounting. A reader/resetter awakened by that publication cannot
pass the mutex until the update is visible. Promise::set() has no user
continuation; produced results contain plain output data or native Tensor
objects. Its cancellation failure path preserves error accounting without
an optimistic completion increment and rollback. Rejection increments its
error count before waking the waiter.

Validation logs in the packaging workspace:

* ml-async-stats-full-1.log: all CPU-applicable ML tests pass in the workstation
  configuration, 3029 assertions; accelerator availability is reported explicitly.
* ml-async-stats-cpu-full-2.log: 335 executed cases, 11 accelerator skips,
  3027 assertions, using the portable CPU configuration.
* ml-async-stats-valgrind-tests-2.log and ml-async-stats-valgrind-2.log:
  four async cases, 588 assertions, zero memory errors or lost bytes.

The first memory-check attempt used the workstation CUDA build and reached
NVIDIA driver initialization from the existing GPU-memory statistics probe.
Those retained logs report driver allocations and a PCRE2 JIT padding read;
they do not qualify that configuration. The passing check uses the RPM CPU
configuration and PCRE2 built with its upstream Valgrind instrumentation,
with JIT still enabled and no suppression. No shipped PCRE2 replacement is
introduced. Native OBS architecture qualification remains a separate gate.

.. list-table:: Full audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 9. Copyright 2026 on all new files
     - Pass
     - The ML source/test headers and this review carry 2026 copyright notices.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 13. %modern directive present
     - Pass
     - ml.qtest uses %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - ml.qtest retains executable mode 100755.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - The existing suite prepends the local qlib directory and requires QUnit, FsUtil and QoreOnnxExport by relative path.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - ml is delivered in this repository; no new external Qore module dependency.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - Pass
     - No new unbounded loop. Existing dynamic result delivery retains cancellation checks; rejection must deliver the original error to every pending promise before cleanup can finish.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - Pass
     - Existing dynamic dispatch retains qore_check_cancel(); no deprecated interrupt API introduced.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - Pass
     - Dynamic assembly/delivery retains its every-100-item checks; new regression loops are bounded at 16 cycles.

   * - 24. No blocking operations without cancellation support
     - Pass
     - Only a short statistics critical section is added. Promise::set() takes its own short lock and broadcasts; it invokes no continuations. Future::get() releases its lock before callers can read pool statistics.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 34. Response/output types use private Fields
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new module/class, DataProvider registration, I/O operation or Java dependency in this change.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Corrects publication ordering at the producer. Tests read/reset immediately after Future::get(), without polling, sleeps or suppressed diagnostics.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - The shared resolver consumes each referenced result exactly once through Promise::set(), checks the exception sink and releases the lock through RAII. Cancellation still records an error; no optimistic increment/rollback or underflow.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Success/failure accounting for set() shares the existing mutex with getPoolStats/resetPoolStats. Rejection counts are sequenced before promise publication. Atomic counters retain their existing synchronization.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - The helper accepts QorePromise* and QoreValue; request payloads, futures and test counters use declared types.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - One constant-time mutex critical section per completed inference, no polling, copies or new quadratic work.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Regression covers missing-input errors for runAsync, runTensorsAsync and runBatchAsync, rejected dynamic batches, empty batches and repeated counter resets.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - Public getPoolStats documentation states the completion guarantee and explains early cancellation. ML 1.1 release notes describe the race and measurement-interval correction; existing inference examples remain valid.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - Pass
     - getPoolStats retains RET_VALUE_ONLY; mutating inference/reset methods gain no incorrect constant/pure flags.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - No new I/O, format-string input, credentials or buffer access.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Full Debug CPU ML suite: 335 executed cases and 11 platform-feature skips, 3027 assertions. Four affected async cases pass 588 assertions under Valgrind with zero errors and zero definite, indirect or possible lost bytes.
