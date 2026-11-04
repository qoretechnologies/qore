# AOT Modules of a Build Tree and Their Sources

Copyright (C) 2026 Qore Technologies, s.r.o.

A build tree links the AOT `.qmod` files that it builds into the source `qlib` directory
(`QORE_AOT_LINK_SOURCE_MODULES`, and `cmake/EnsureSourceQmodSymlinks.cmake` in a CI test stage), where the in-tree
tests find them before the module sources: the loader takes a `.qmod` over a `.qm` in the same directory.  The module
loader checks such a link before it loads the module (`qore_build_aot_module_is_stale()` in `lib/ModuleManager.cpp`):
if one of the module's sources has changed since the module was compiled, the source module is loaded instead, with
the warning

    AOT module '<qmod>' for feature '<name>' is out of date: its source '<source>' has changed since it was compiled;
    loading source module '<qm>' instead; rebuild the AOT module

Two kinds of `.qmod` are checked (`qore_build_aot_module_is_stale()`):

- a `.qmod` that is a symbolic link, against the dependency file next to the module it links to, or, without one,
  against its own sources by modification time;
- a `.qmod` that is a regular file **with its own dependency file** (`<qmod>.d`, which qcc writes with `--depfile`, as
  every build of a tree does), with the same fingerprint and content-hash rules.  This covers the directory of the
  built modules that `run_tests.sh` puts first in `QORE_MODULE_DIR` (`build/qlib-qmod`), so that a child process of
  a test, or any script that does not prepend the source `qlib` directory, does not run a module that is out of date
  with its source either.

A regular `.qmod` without a dependency file next to it is **not checked**:

- an installed module: the install rules remove an installed `<qmod>.d` and `<qmod>.d.fingerprints`
  (`cmake/QoreMacros.cmake`), so installed and
  packaged trees are unchanged, and the check costs one `stat()` of the missing dependency file per load;
- a module compiled by hand with `qcc --compile-module` (or `qore --compile-module`) without `--depfile`: it is used
  even if its source has changed since; rebuild it, or compile it with `--depfile` to have it checked.

A dependency file is **used only for the module that its rule names**: its target (the `-o` path qcc wrote it for)
must resolve to the module that is checked, the regular file itself or, for a link, the module the link resolves
to; a relative target is taken relative to the directory of the dependency file.  A dependency file whose target is
another file, or that has no rule or a rule without a target, is ignored as if there were none (a debug build says so
at debug level 1).  A dependency file describes the build of one file: copying a build tree's `qlib-qmod` to an
installation with `cp -a` or `rsync -a` copies its dependency files too, with the modification times of the build,
and the copies then named the build's modules, whose sources changed with every later commit, so the installed
copies were loaded from their sources with a warning.  The install rules also remove an installed
`<qmod>.d.fingerprints`, which is never read without its dependency file.

A modification-time rule is not used for a regular file without a dependency file: a checkout, a copy or a package
extraction gives files new modification times without changing them (see below), and there is no fingerprint to
tell a changed source from a touched one.

The check is skipped for a Program that cannot access the filesystem (`PO_NO_FILESYSTEM`), and a file that the
filesystem policy of a sandbox does not allow to be read is not checked.  When the source module that is loaded
instead fails, `AOT-MODULE-STALE` follows its error in the exception chain (see
`design/module-parse-option-masks.md`, "Reporting a refused AOT module").

## The sources of a module

The sources are the files that the dependency file of the module lists (`<qmod>.d`, written by qcc with `--depfile`
for make): the module's own `.qm`, `.qc` and `.ql` files and those of the modules it depends on, and the binary
modules it depends on.  Without a dependency file, the main source of the module and the `.qm` and `.qc` files of its
directory are the sources.

## When a source has changed

A source's modification time cannot decide on its own: a git checkout, an artifact copy and a package extraction
give files new modification times without changing them.  In the CI flow, the test stage checks the repository out
again, so every source is newer than the `.qmod` files that come with the build stage's artifact, and an mtime rule
loaded every module from its source, with a warning in the output of every test.

So qcc records, next to the dependency file of a `.qmod`, the fingerprint of each file it lists, in
`<qmod>.d.fingerprints` (`include/qore/intern/QoreModuleSourceFingerprints.h`):

    # qore AOT module source fingerprints 1
    <size> <mtime_ns> <xxh64> <path>

with the size and the modification time in nanoseconds in decimal, the XXH64 hash (seed 0) of the content in 16
hexadecimal digits, and the path last, as it can contain spaces.  A file that cannot be read, that changes while it
is hashed, or whose path has a line break is not recorded.

For a source with a fingerprint, the loader:

1. compares the size and the modification time with the recorded ones: if both are equal, the source has not
   changed, and it is not read;
2. otherwise, if the size differs, the source has changed;
3. otherwise (the same size, another modification time) it hashes the content and compares it with the recorded
   hash: only a different content has changed.

A source without a fingerprint, or every source of a module without a fingerprint file (one compiled by an earlier
qcc), is compared by its modification time: a source newer than the module has changed.  A source that cannot be
read in step 3 is compared the same way.  A source that does not exist is not checked.

The hashes that a process computes are cached by path, size and modification time (a leaf lock with a fork handler),
so a source that many modules depend on, such as `Util.qm`, is hashed once per process; a source is hashed only when
its modification time is not the recorded one.

## Cost

When the sources have their recorded sizes and modification times, as in a build tree, the check reads the
dependency file and the fingerprint file of each linked module it loads and calls `stat()` on each source, as the
mtime rule did; nothing is hashed.  After a checkout or a copy, each source of a loaded module is hashed once per
process.

## Tests

`examples/test/ir/AOTLinkedModuleStale.qtest`: a current module is loaded; a changed source (of the same size, and of
another size) and a changed dependency are found; sources that only got new modification times are not; the
fingerprint file lists the sources of the depfile; without a fingerprint file or a depfile, the modification time
decides; a module that is not a link, or one that a Program without filesystem access loads, is not checked.
`examples/test/ir/AOTModuleStaleFallback.qtest`: a built module that is a regular file with its depfile, first in the
module path of a child process as in `run_tests.sh`, is checked (a changed source is found, a touched one is not, and
without the depfile the module is loaded); a module compiled without `--depfile` is not checked; and an out-of-date
module whose source fails reports `AOT-MODULE-STALE` after the source error.
