# September 2026 source dependency refresh

The refresh updates the libraries fetched or vendored by Qore while preserving existing
XXH32/XXH64 hash identities, JSON Schema recursion/cancellation checks and the musl allocator
loading contract. nghttp2 1.70.0, ngtcp2 1.25.0 and nghttp3 1.18.0 already match the supported
upstream releases, so their pins remain unchanged.

## Versions and sources

|!Library|!Previous|!Updated|!Source
|c-ares|1.34.6|1.34.8|https://github.com/c-ares/c-ares/releases/tag/v1.34.8
|Apache Arrow/Parquet|24.0.0|25.0.1|https://arrow.apache.org/release/25.0.1.html
|tree-sitter runtime and CLI|0.26.8|0.26.13|https://github.com/tree-sitter/tree-sitter/releases/tag/v0.26.13
|jemalloc (musl)|5.3.0|5.4.0|https://github.com/jemalloc/jemalloc/releases/tag/5.4.0
|jsoncons headers|1.8.1|1.9.0|https://github.com/danielaparker/jsoncons/releases/tag/v1.9.0
|xxHash|Legacy in-tree copy|0.8.4|https://github.com/Cyan4973/xxHash/releases/tag/v0.8.4

The c-ares and jemalloc release archive SHA-256 values match GitHub's published asset digests.
The Arrow archive was checked against Apache's published SHA-512 checksum; CMake pins its
computed SHA-256. The xxHash header is byte-for-byte upstream. The jsoncons tree differs from
upstream only in the same seven files that carried Qore patches before this refresh.

## Integration decisions

- Both c-ares system discovery paths require 1.34.8, so the fallback update cannot be bypassed
  by an older installed resolver. This incorporates CVE-2026-33630, CVE-2026-69184 and
  CVE-2026-69186, together with the callback signature compatibility fix in 1.34.8.
- tree-sitter uses the 0.26.13 maintenance release rather than introducing 0.27 API changes.
  Runtime discovery requires the same minimum, the CLI dependency is exact, and the grammar
  was regenerated. The parser ABI remains 15; only the generated array support header changed.
- Arrow retains system-package discovery; the source fallback is 25.0.1. Release validation
  explicitly disables system Arrow discovery to exercise the updated source build. Embedded
  Arrow does not initialize its usual option defaults, so Qore explicitly enables PIC and
  shared system codec selection; otherwise a non-PIC system Snappy archive fails to link
  into the shared DataFrame module. Missing codecs retain Arrow's source-build fallback.
  Qore also supplies the fetched source/generated header directories explicitly: Arrow's
  in-tree static targets do not export them. This prevents mixing installed Arrow 23 headers
  with Arrow 25 libraries, which the Parquet regression caught as an ABI mismatch and crash.
- jemalloc keeps `--disable-initial-exec-tls`, `--disable-cxx`, static PIC linkage and unprefixed
  allocation symbols. `--disable-dss` disables `sbrk` allocation: musl exposes that function but
  rejects all nonzero increments. The upstream DSS unit test exposed this mismatch; the final
  musl configuration passes the unit and integration suites. Existing build caches using the old
  default upstream URL migrate to 5.4.0; explicit local tarball overrides remain checksum-checked.
- jsoncons retains the bigint delegating copy constructor and all six schema recursion hooks.
  These preserve allocator ownership, stack exhaustion handling and cooperative cancellation.
- xxHash remains an internal dependency. Qore builds only the XXH32 and XXH64 algorithms it
  already uses, retaining the transparent string hasher and stack-allocated streaming API.
  The upstream `XXH_NO_XXH3` option excludes the unused XXH3 implementation, whose mandatory
  SIMD inlining fails under this repository's GCC debug/unity build flags.
  `qore-xxhash-compat-test` checks 57 fixed outputs captured from the previous implementation,
  including empty inputs, word/stripe boundaries, large inputs, three seeds, eight alignments,
  and five incremental chunk sizes. It does not derive expected values from the new library.
- The CBOR regression suite now covers truncated multibyte integer/half-float heads and UTF-8
  validation per indefinite-length text chunk, together with valid counterparts and recovery.

## Validation

- The glibc RelWithDebInfo build passes for `qore`, `qcc`, `json`, `astparser` and `dataframe`,
  with `-DCMAKE_DISABLE_FIND_PACKAGE_Arrow=ON` to exercise Arrow 25.0.1 from source.
  The debug unity build passes for `qore`, `json` and `astparser`.
- Release JSON, DNS, parser and AOT incremental dependency tests pass: 12 files, 295 cases,
  2,267 assertions. This includes repeated AOT compilation with byte-identical output.
  DataFrame passes separately: 39 cases, 1,308 assertions, including Parquet and Arrow IPC.
- Debug JSON, DNS and parser tests pass: 11 files. The expanded CBOR suite was rerun after its
  final edit and passes all 23 cases and 92 assertions.
- The three standalone grammar tests pass 50 brace/regex, 821 keyword identifier and
  39 parse directive checks. The JSON C++ API smoke test passes all checks.
- `qore-xxhash-compat-test` passes in release, debug and musl builds, and in a standalone
  ASan/UBSan build. `qore-json-bigint-test` passes in release/debug and under Valgrind,
  including allocator ownership and allocation failure cases.
- On Alpine/musl, jemalloc 5.4.0 with Qore's configure flags passes 97 unit suites
  (27 platform/configuration skips, no failures) and all 15 integration suites. A separate
  `dlopen` test passes threaded cross-library `malloc`/`free` and `new`/`delete`; the linked
  library has no initial-exec TLS relocations. Qore builds and evaluates `1 + 1` successfully.
- Release archive checksums, the unmodified xxHash header and the seven retained jsoncons
  patches were checked against pristine upstream archives. `git diff --check` passes for
  Qore changes; whitespace in vendored/generated upstream files is retained verbatim.

The main runtime regression commands (from the repository root) were:

```sh
QORE_BINARY="$PWD/build/qore" LIBQORE_BINARY="$PWD/build/libqore.so" \
    ./run_tests.sh -d modules/json -d qore/classes/Socket/resolve-addrinfo.qtest \
    -d ../../modules/astparser/test -d ir/AOTIncrementalDeps.qtest
QORE_BINARY="$PWD/build/qore" LIBQORE_BINARY="$PWD/build/libqore.so" \
    ./run_tests.sh -d ../../modules/dataframe/test/dataframe.qtest
QORE_BINARY="$PWD/build-debug/qore" LIBQORE_BINARY="$PWD/build-debug/libqore.so" \
    ./run_tests.sh -d modules/json -d qore/classes/Socket/resolve-addrinfo.qtest \
    -d ../../modules/astparser/test
```

## Audit checklist

This applies the audit-changes skill and the repository module structure, sandboxing and
cooperative cancellation design guidance to the dependency integration changes. Upstream
library code retains upstream conventions; it is not rewritten to Qore coding conventions.

|!Check|!Status|!Evidence
|Module documentation index|N/A|No new Qore modules
|Release notes|Pass|Dependency versions and security minimum documented in 900_release_notes.dox.tmpl
|CMake registration and QMOD lists|Pass|Existing modules retained; standalone hash regression target added
|Module sections, directives, includes and directory layout|N/A|No .qm or .qc changes
|New-file copyright|Pass|Qore wrapper/test code carries 2026 copyright; vendored and generated files retain upstream notices
|QPP namespace registration|N/A|No new QPP classes
|Qore test directives, executable mode, module paths and optional imports|Pass|CBOR suite has %modern, a module path before imports, executable mode and only required builtin/project modules
|Filesystem and network sandbox checks|Pass|No new Qore I/O entry points; dependency discovery is build-time; runtime callers retain existing checks
|Cooperative cancellation API and polling frequency|Pass|Schema hooks preserved; hashing remains an internal library operation with existing caller boundaries; standalone test loops do not run in Qore
|Blocking operations and interruption|Pass|No new blocking Qore operations; DNS integration unchanged
|DP action registration and capability flags|N/A|No DataProvider changes
|DP field types, allowed values, examples and sensitive flags|N/A|No DataProvider changes
|DP app groups, logos, descriptions and scheme paths|N/A|No DataProvider changes
|DP Markdown and short descriptions|N/A|No DataProvider changes
|FactoryMap and getRecordTypeImpl signatures|N/A|No DataProvider changes
|JNI JAR inventory, checksums and install rules|N/A|No Java dependencies
|Qualification inventories, structured failures and authenticated publication|N/A|No qualification changes
|Schema normalization, static extraction, IDs, locales and installed app artifacts|N/A|No app presentation changes
|Workarounds, stubs and incomplete features|Pass|Upstream build options select supported allocator operations and the hash algorithms actually used
|Exception safety|Pass|Bigint allocator patch preserved; regression covers allocation failure and ownership; no new Qore allocations
|Thread safety|Pass|Hash state remains per operation; no new mutable shared state
|Type safety|Pass|Upstream hash types used; fixed-width golden vectors and explicit casts
|Performance|Pass|No new copying or algorithm changes in Qore wrappers; hashes remain linear in input size
|Error handling|Pass|Existing JSON/DNS error propagation retained; standalone regression exits nonzero on mismatch
|Public API documentation|N/A|No new public runtime methods; internal dependency decisions documented here
|QPP flags|N/A|No QPP changes
|Security and credentials|Pass|Fixed c-ares minimum on both discovery paths; verified archive checksums; no credentials added
|Correctness|Pass|Legacy hash golden vectors, sanitizers, bigint allocator checks, grammar checks, JSON/CBOR/DNS/Parquet/IPC and AOT regressions pass
