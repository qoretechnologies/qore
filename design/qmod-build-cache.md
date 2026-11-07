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
- `test/docker_test/qmod-cache.sh`: the CI steps (the job's cache directory, the guard, pruning, statistics)
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
|Build context|The versions of bison and flex, which generate libqore's parser and scanner; the content of the LLVM library and of every shared library libqore and qcc are linked with (their patch versions are not in `/etc/os-release`); `/etc/localtime` (the time zone of parse-time dates when `TZ` is not set)|`context`
|Build context|The content of `tools/qore-qmod-cache.py` and the cache schema|`context`
|Base key|The qcc command line (absolute paths of the source and output, optimization level, prefix maps), its working directory|`exec`
|Base key|Every environment variable named `QORE_*` or `QCC_*` except those in `ENV_EXCLUDED_NAMES`/`ENV_EXCLUDED_PREFIXES`, and `CC`, `CXX`, `LANG`, `LC_ALL`, `LC_CTYPE`, `TZ`, `MACOSX_DEPLOYMENT_TARGET`, `SDKROOT`, `LD_LIBRARY_PATH`, `LD_PRELOAD`, `DYLD_*`|`exec`
|Base key|The module's own sources: every file of a module directory (but the qmods linked into it), or the `.qm` file, and the sources the build lists for it|`exec`
|Entry|Every file the module's depfile lists, by content: the module's own sources, the sources of the modules it requires, the binary modules it loads, include files|`exec`
|Entry|For each source of another in-tree module, that module's key, which covers its own dependencies: the key is transitive (a Merkle construction)|`exec`
|Entry|For each module it requires (the dependencies in its metadata record), the files in the module path that the loader could load for it, by path: a module that appears in a directory searched earlier would be loaded instead|`exec`
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

Since qcc lists the modules a module requires directly or not (the fix of
[AOT Modules of a Build Tree](aot-linked-module-sources.md) "The sources of a module"), the depfile alone covers the
sources of the transitive dependencies; the keys of the module dependencies stay in the entry all the same: they also
carry each dependency's own candidate-file and optional-module records (which a depfile cannot express), and keep the
cache correct for a qcc without the fix.

A dependency the key cannot cover makes the module uncacheable: a file that the module's sources include with
`%include` and that its depfile does not list (a qcc from before the fix of
[AOT Modules of a Build Tree](aot-linked-module-sources.md) "The sources of a module" lists no included files; one
included by a name that cannot be resolved here, such as one starting with an environment variable, counts as not
listed; no qlib module includes a file), a missing file, a Qore module source (`.qm`, `.qc`)
of a module that was not built through the cache in this build tree (a module from outside the tree), and an AOT
module that is not one of the tree's.  A binary (C++) module is covered by its content.

### Why the candidate files of the required modules

The depfile names the files qcc loaded, not why it found them: a module file that appears in a directory searched
before the one qcc loaded a module from (a module installed in an earlier directory of `QORE_MODULE_DIR`, a binary
module built in the tree where an installed one was found before) would be loaded instead by a new compile, while
the files the entry names are unchanged.  The module loader does not check this either.  So the entry records, for
each module named in the module's metadata record, the files in the module path that are candidates for it, and a
hit needs the same list.

### Why absolute paths

The key has the absolute paths of the sources, the build tree and the module path, and is not made relative to the
source and build roots: a qmod records the absolute paths of its sources (statement locations, its depfile and
fingerprints), so a module compiled in another checkout would carry the wrong ones, and the loader would not use its
depfile (whose target must be the module).  CI keeps the checkout path stable instead (see "CI").

### Module path directives

A module whose sources have `%prepend-module-path` or `%append-module-path` (with or without environment variables)
is not stored: the modules those directives make qcc find are not in the candidate-file records.

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

- The git revision (`git-revision.h` is left out of the generated headers): since qcc no longer records it in an
  artifact's build information (`examples/test/ir/AOTArtifactReproducible.qtest`), nothing in a qmod depends on it.
  Before that, a hit supplied a qmod naming an older commit, and the audit failed after every commit.
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
2. every failed optional module must have the recorded candidates, and every module it requires the recorded
   candidate files;
3. the result key must be the digest of the entry;
4. each stored file must have the SHA-256 digest the entry records (a damaged entry is removed);
5. the stored module must contain the AOT runtime identity of the build;
6. the stored depfile must name the module's path (the loader uses a depfile only for the module its rule names).

The depfile, the fingerprints and then the module are each renamed into place, and the module and depfile are
compared with the entry's digests after the copy.  Anything unexpected makes the module a miss.

### When storing

A module is stored only when qcc wrote the module, the depfile and the fingerprints, the module contains the
build's identity, the depfile names the module, every source still has the size and modification time qcc recorded
in its fingerprint and every dependency has one (a source written during the compile, or one qcc could not hash, is
no stable input), every dependency can be covered (see
above), and the metadata record can be read.

### The CI guard: `verify`

After the build, `qore-qmod-cache.py verify --build-dir build --qcc build/qcc --require-sidecars` fails the job when:

- qcc reports another identity than the build's `qore_aot_runtime_identity.h`;
- any qmod of the build (supplied, compiled, or one the cache did not see) does not contain the build's identity, or
  its sidecar records another one;
- a module the cache supplied is no longer the module it supplied, its depfile names another module or lists other
  dependencies, a dependency changed after the module was supplied, or its unavailable optional modules differ.

### Key holes: a failed audit or a key collision

A failed audit, or a store that finds another module than qcc wrote for the same key (another job stored the key
first), means that the key misses an input, so no entry of that cache can be trusted.  The job fails, so that someone
finds the input, and the cache the job writes to is quarantined first: its directory is renamed out of the way at
once, so that no later job reuses an entry of it (the next build is cold), and a prune deletes it an hour later, when
no job can still be reading it.  Removing only the entry that was audited would leave the others that the same hole
let in.  A cache of another trust level that the job only reads is left to its own jobs, whose audits find the same.

An object that no valid entry backs (damaged, forged, or left by an interrupted store) is replaced when its key is
stored again; that is no collision.

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
an entry with a temporary file renamed over it, so concurrent builds (`make -j`, several jobs sharing a host
directory) never see a partial entry; the first object stored for a key wins.  At most 8 entries are kept under one
base key.

### Trust levels

Whoever can write to a cache directory can make a build use a module that qcc never wrote, and the module's digests
in the entry only prove that it was not changed after the entry was written.  So the CI cache has a directory per
trust level, and the runner configuration enforces who writes to which:

- `protected/<job name>`: written only by the jobs of protected refs (`CI_COMMIT_REF_PROTECTED=true`).  They sign each
  entry with `QORE_QMOD_CACHE_SIGNING_KEY` (HMAC-SHA256 of the entry, which has the digests of its files), a protected
  CI variable, so jobs of other refs never have it, and use only entries signed with it; the guard checks the
  signatures of the entries it supplied.  A runner that runs the jobs of unprotected refs mounts `protected/`
  read-only.
- `unprotected/<ref slug>/<job name>`: written by the jobs of one unprotected ref, which read the protected cache of
  their job name too (`exec --read-cache-dir`), but never write to it (no last-use mark, no removal of a damaged
  entry), and never get the signing key.  A ref cannot poison another ref's cache, nor develop's.

A job of a protected ref on a runner that mounts `protected/` read-only uses it read-only
(`QORE_QMOD_CACHE_READONLY=1`).

### Concurrent jobs and pruning

Several jobs can share the directory.  Each object has a lock file, `objects/<result key>.lock` (`flock(2)`):

- a job holds a shared lock on an entry while it reads it (a hit) or stores it;
- an entry is removed only under an exclusive lock taken without waiting, so an entry that a job is reading is skipped,
  never removed; the entry and its object are renamed into `trash/` and then deleted, and the lock file is removed
  while the lock is held: a reader that was waiting for it finds no object and compiles the module;
- the lock file's modification time is the entry's last use: it is set when the entry is stored and on every hit
  when the cache can be written (a hit from a cache that cannot be written, or with `QORE_QMOD_CACHE_READONLY=1`,
  still succeeds and records nothing);
- one job prunes at a time (`prune.lock`, taken without waiting: a job that finds another one pruning skips its own
  prune and says so).

Without working locks (a file system that does not propagate them), a removal can race a reader; the reader's checks
(each stored file's digest before the copy, the module and depfile digests after it) then make the module a miss, so
the cost is a compile, never a wrong module.

`prune` removes, in this order: orphans older than an hour (an object without an entry, an entry without an object,
an interrupted store or removal; a younger one can be a store in progress, and an object a job holds is skipped),
quarantined caches older than an hour, the entries not used for `--max-age-days`, and then the least recently used
entries while the cache is larger than `--max-size` MiB.  It never removes entries because one build did not use
them: the directory is shared.  At most 8 entries are kept under one base key, the most recently used ones.  It prints one line:
the hits and misses of `--build-dir`, and the size of the cache before and after pruning, with what it removed and the
entries it skipped because a job was reading them.

Each qmod gets a sidecar `<qmod>.qmod-cache.json` (`hit`, `miss` or `uncacheable`, the key, and for a hit the entry),
which `verify`, `stats` and `prune` read.

## Environment of `exec`

|!Variable|!Effect
|`QORE_QMOD_CACHE_DISABLE=1`|compile every module and store nothing
|`QORE_QMOD_CACHE_READONLY=1`|look modules up, store nothing
|`QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]`|audit a sample of the hits (see above)
|`QORE_QMOD_CACHE_SIGNING_KEY=<key>`|sign the entries stored, and use only entries signed with the key (see "Trust levels")
|`QORE_QMOD_CACHE_TEST_CORRUPT_RESTORE=1`|for the tests only: a restored module is damaged after the copy

## CI

The cache of the CI jobs is a directory of the host that the runner mounts into every job: a directory of professor
mounted into the macOS VM, and a directory of each Kubernetes node mounted into the build pods.  The runner names it
in `QORE_QMOD_CACHE_DIR`; `test/docker_test/qmod-cache.sh` uses the directory of the job's trust level and name in it
(see "Trust levels"), which the jobs of that level and name share, and builds without the cache when the variable is
not set or the directory of the job's level is not mounted.  The key has absolute paths, so the checkout path of a
job must not change between jobs (`GIT_CLONE_PATH`, which the runner must allow, for a runner whose default path has a
concurrency number in it).  After every build it runs the
guard and prunes the job's cache directory (`QORE_QMOD_CACHE_MAX_AGE_DAYS`, default 7, and `QORE_QMOD_CACHE_MAX_SIZE_MB`,
default 3072, set in `.gitlab-ci.yml`), and prints the statistics line.

A host directory costs no network transfer and no object store, and keeps the cache across branches and pipelines;
on Kubernetes a node only has the modules of the jobs that ran on it, so the hit rate depends on how jobs land on
nodes (the statistics line of each job shows it).  The runner configuration that mounts the directories is described
in the change that introduced it (it is not part of this repository).

## Measurements

professor (macOS 26, arm64, 24 cores), release build, `make -j8`, 392 qmods, other builds running in parallel:

|!Build|!Time|!Notes
|Without the cache|127 s|every module compiled
|Cold (empty cache)|128 s|every module compiled and stored: the cache costs about 1%
|Warm (new build tree, all hits)|34-72 s|depending on the host's load; a no-op `make` of the same tree takes 16 s, a hit about 0.1 s
|Warm with `QORE_QMOD_CACHE_AUDIT=1`|134 s|every hit compiled again: all 392 byte for byte the same
|`Mime.qm` changed|162 s|344 modules compiled: exactly Mime and the modules that require it, directly or not
|`QoreRepl.qc` changed|69 s|1 module compiled

The cache of one build is 324 MiB (126 MiB with `gzip -1`).
