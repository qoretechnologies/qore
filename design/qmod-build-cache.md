# The qmod Build Cache

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented, off by default.  A build configured with `-DQORE_QMOD_CACHE_DIR=<dir>` looks every in-tree AOT module
(`qlib/*.qm`, `qlib/<Module>/`) up in a content-addressed cache in `<dir>` before qcc compiles it, and stores what qcc
writes after a compile.  CI uses it to keep the qmods of the build jobs between pipelines.

Relevant code:

- `tools/qore-qmod-cache.py`: the cache (`context`, `exec`, `verify`, `stats`, `prune`)
- `cmake/QoreMacros.cmake`: `QORE_QMOD_CACHE_SETUP()` (the context target and the launcher) and
  `QORE_USER_MODULE_AOT_RULES()` (puts the launcher before the qcc command of each module)
- `CMakeLists.txt`: the `QORE_QMOD_CACHE_DIR` option
- `test/docker_test/qmod-cache.sh`: the CI steps after a build (guard, statistics, pruning)
- `examples/test/cmake/test_qmod_cache.py`: the tests

Related: [AOT Modules of a Build Tree and Their Sources](aot-linked-module-sources.md) (the loader's staleness
rules), [AOT Object Files and Module Artifacts](aot-object-files-and-module-artifacts.md) ("AOT Module Runtime
Identity", "Module Metadata Section").

## The rule

A cached module is used only when qcc, run again, would write the same module, and the module loader would use it.
A miss costs one compile; a wrong hit tests or ships code that no compiler of the commit wrote.  So every input that
can change what qcc writes is part of the key, inputs are compared by content, and anything that cannot be checked
makes the module a miss (it is compiled as without the cache) or *uncacheable* (compiled and not stored).

## The key

A module's dependency list is known only after it is compiled, so the lookup has two steps, as ccache's direct
mode: the **base key** covers the inputs known before the compile; under it, each earlier compile left an **entry**
with the dependencies it found and their content.  An entry whose dependencies all still have that content is a hit,
and its **result key** (the base key, the dependency records and the optional module records) is the module's key.

|!Part|!Inputs|!Where
|Build context|The qcc format fingerprint `qcc-format.stamp`: the AOT runtime identity's sources (`_qcc_format_sources`) and `@build-config` (build type, compiler and its version, compiler flags, system, processor, pointer size, LLVM version)|`context`
|Build context|The AOT runtime identity that qcc reports (`qcc --version`); the build fails before any qmod if it reports none|`context`
|Build context|The content of every file in `lib/` and `include/`, of `qcc-main.cpp`, `CMakeLists.txt` and `cmake/third-party-sources.cmake`, of the generated headers in `<build>/include` (but `git-revision.h`), and the compile flags and link commands of libqore and qcc (`CMakeFiles/{libqore,qcc}.dir/{flags.make,link.txt}`)|`context`
|Build context|The Qore options of the CMake cache, the build type, the C and C++ compilers, `LLVM_DIR` and the macOS deployment target, SDK and architectures|`context`
|Build context|The probe of optional external modules (`qore-aot-probe.stamp`) and the AOT link configuration (`aot-link.conf`)|`context`
|Build context|The link driver qcc runs (`cxx` of `aot-link.conf`) and its version, the linker and its version, the macOS SDK version, the operating system (`ID`, `VERSION_ID`, macOS version), the C library (glibc or musl and its version), the system and machine|`context`
|Build context|The content of `tools/qore-qmod-cache.py` and the cache schema|`context`
|Base key|The qcc command line (absolute paths of the source and output, optimization level, prefix maps), its working directory|`exec`
|Base key|Every environment variable named `QORE_*` or `QCC_*` except those in `ENV_EXCLUDED_NAMES`/`ENV_EXCLUDED_PREFIXES`, and `CC`, `CXX`, `LANG`, `LC_ALL`, `LC_CTYPE`, `TZ`, `MACOSX_DEPLOYMENT_TARGET`, `SDKROOT`, `LD_LIBRARY_PATH`, `LD_PRELOAD`, `DYLD_*`|`exec`
|Base key|The module's own sources: every file of a module directory (but the qmods linked into it), or the `.qm` file, and the sources the build lists for it|`exec`
|Entry|Every file the module's depfile lists, by content: the module's own sources, the sources of the modules it requires, the binary modules it loads, include files|`exec`
|Entry|For each source of another in-tree module, that module's key, which covers its own dependencies: the key is transitive (a Merkle construction)|`exec`
|Entry|For each `%try-module` request that failed when the module was compiled (read from the module's metadata section), every candidate file for that module in `QORE_MODULE_DIR` and the loader's standard module directories, with its content|`exec`

### Why the whole of libqore, not just the AOT runtime identity

The AOT runtime identity covers the contract between generated code and the runtime: the loader refuses a module
compiled for another identity.  It does not cover what qcc *emits*: qcc parses the module with the whole of libqore
(the parser, parse-time type resolution, the signatures of the builtin functions and classes, constant folding,
which can call builtin code), so a change to any of them can change the generated code or make a module fail to
compile, with the identity unchanged.  An in-tree incremental build accepts that (it rebuilds qmods only when
`qcc-format.stamp` changes), and `QORE_GET_QCC_DEPS()` does not for a build against an installed Qore, for the same
reason.  CI compiles every module with the commit's qcc today; a cache keyed on the identity alone would test modules
compiled by an earlier qcc whenever libqore changed outside the identity's sources (28% of the develop commits of
October 2026, against 11% that changed the identity), and a change that breaks the compile of a qlib module would
pass.  So the build context covers every libqore and qcc source, and the cache hits for the commits that change no
libqore source (about 60% of them), and for every module not affected by a qlib change.

### Why the transitive dependencies

qcc's depfile (`--depfile-module-deps=source`) lists the sources of the modules a module requires *directly*; the
module also sees the declarations of what those modules re-export, and their own dependencies.  A key built from the
depfile alone would keep `AftershipRestClient` (which requires `ConnectionProvider`) when `Util` (which
`ConnectionProvider` re-exports) changes.  The key of each in-tree module dependency is in the entry, and that key
covers the module's own dependencies, so a change anywhere below a module changes its key.  The cache writes each
module's key into `<build>/qmod-cache-index/<digest of the module's source root>.json` when it compiles or supplies
the module; a dependent finds a source's module as the nearest indexed directory above the source, or the source
itself, and uses the key only while the qmod is the one recorded (size, modification time, inode).

A dependency the key cannot cover makes the module uncacheable: a missing file, a Qore module source (`.qm`, `.qc`)
of a module that was not built through the cache in this build tree (a module from outside the tree), and an AOT
module that is not one of the tree's.  A binary (C++) module is covered by its content.

### Why the failed optional modules

The loader refuses an AOT module compiled when a module it requested with `%try-module` was unavailable, once that
module can be loaded (`AOT-MODULE-STALE`, `QoreModuleManager::findAvailableOptionalModule()`).  Such a module is not
in the depfile.  qcc records the failed requests in the module's metadata record (`QoreAOTModuleFileInfo`), which
the cache reads from the module (the record is found by its magic, must parse completely and name the module, and
must be the only one; otherwise the module is uncacheable), and records every candidate file of each such module in
the module path (`<name>-api-<x>.<y>.qmod`, `<name>.qmod`, `<name>.qm` in each directory and in a directory of the
module's name).  An entry is used only while the candidates are the same: a module that appears is a miss.  The
directories searched are those of `QORE_MODULE_DIR` and the loader's standard ones, even with `QORE_MODULE_DIR_ONLY`,
which can only make more modules misses.

### What is not an input

- The git revision: no qmod or binary module records it (`git-revision.h` is left out of the generated headers).
- The build directory of the cache, the cache settings (`QORE_QMOD_CACHE_*`), and the test variables of CI
  (`QORE_TEST_OPTS` and others, `QORE_DB_CONNSTR*`, which may hold credentials): neither qcc nor libqore reads them,
  which `test_qmod_cache.py` checks against every `"QORE_..."` literal in `lib/`, `include/` and `qcc-main.cpp`.
- `PATH`: the link driver and linker are recorded as the files that qcc runs, with their versions.
- The kernel version of the build host, and the versions of the system libraries libqore links other than the C
  library: they do not change what qcc writes for a given compiler, LLVM and linker.  An image that changes them
  changes the operating system version, which is in the context.

## Checks

### On a hit

Before a cached module is used:

1. every dependency of the entry must still have its recorded SHA-256 digest (stricter than the loader, which
   compares the size and the XXH64 hash of each source when its modification time differs), and each in-tree module
   dependency its recorded key;
2. every failed optional module must have the recorded candidates;
3. the result key must be the digest of the entry;
4. each stored file must have the SHA-256 digest the entry records (a damaged entry is removed);
5. the stored module must contain the AOT runtime identity of the build;
6. the stored depfile must name the module's path (the loader uses a depfile only for the module its rule names).

The depfile, the fingerprints and then the module are each renamed into place, and the module and depfile are
compared with the entry's digests after the copy.  Anything unexpected makes the module a miss.

### When storing

A module is stored only when qcc wrote the module, the depfile and the fingerprints, the module contains the
build's identity, the depfile names the module, every source still has the size and modification time qcc recorded
in its fingerprint (a source written during the compile is no stable input), every dependency can be covered (see
above), and the metadata record can be read.

### The CI guard: `verify`

After the build, `qore-qmod-cache.py verify --build-dir build --qcc build/qcc --require-sidecars` fails the job when:

- qcc reports another identity than the build's `qore_aot_runtime_identity.h`;
- any qmod of the build (supplied, compiled, or one the cache did not see) does not contain the build's identity, or
  its sidecar records another one;
- a module the cache supplied is no longer the module it supplied, its depfile names another module or lists other
  dependencies, a dependency changed after the module was supplied, or its unavailable optional modules differ.

### The audit

`QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]` makes `exec` compile a sample of the hits again (a rate from 0 to 1, chosen
by the module's key and the seed) beside the restored module, and fail the build unless qcc writes the same module
byte for byte and the same dependency list.  qcc's output is reproducible: the same inputs give the same bytes
(measured for every qlib module on macOS/arm64, and the test with the real qcc checks it), and the output path is not
in the module.  A failed audit means that the key misses an input (or that qcc stopped being reproducible); either
way no hit can be trusted, so the build fails.  CI audits a small sample in every pipeline; `1` audits every hit.

## The cache directory

`<dir>/qore-qmod-cache-1/entries/<base key>/<result key>.json` and `<dir>/qore-qmod-cache-1/objects/<result key>/`
(`qmod`, `qmod.d`, `qmod.d.fingerprints`).  An object is written to a temporary directory and renamed into place,
an entry with a temporary file renamed over it, so concurrent builds (`make -j`, several jobs on one cache) never see
a partial entry; the first object stored for a key wins.  At most 8 entries are kept under one base key.  A hit does
not write to the cache directory, so a CI cache whose content a build did not change need not be uploaded again.

`prune --keep-build <build>` keeps the entries that the modules of a build tree came from or were stored as;
`--unused-since` removes the entries stored before a time; `--max-size` removes the oldest while the cache is larger.

Each qmod gets a sidecar `<qmod>.qmod-cache.json` (`hit`, `miss` or `uncacheable`, the key, and for a hit the entry),
which `verify`, `stats` and `prune --keep-build` read.

## Environment of `exec`

|!Variable|!Effect
|`QORE_QMOD_CACHE_DISABLE=1`|compile every module and store nothing
|`QORE_QMOD_CACHE_READONLY=1`|look modules up, store nothing
|`QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]`|audit a sample of the hits (see above)

## CI

Each Linux build job and the macOS job keep `.qmod-cache/` in a GitLab cache keyed by the job name and the branch,
falling back to the develop branch's cache; `QORE_QMOD_CACHE_DIR` points at it.  After the build,
`test/docker_test/qmod-cache.sh` runs the guard, prints the hits and misses and the cache size, and prunes the cache
to the entries of this build, so the cache holds one build's modules.

The GitLab cache needs a distributed cache (S3 or another object store) in the runners' configuration: without one,
the Kubernetes executor keeps a cache in the job's pod and the macOS job in its VM, and both are gone after the job.
Branches can fall back to the develop branch's cache only when the project does not use separate caches for
protected branches (Settings, CI/CD, General pipelines).

## Measurements

See the commit that introduced the cache for the measured cold and warm build times and the cache size.
