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

qcc writes the real path of a `.qmod` as the target of its dependency file, also when `-o` is relative (before,
a relative target was taken as relative to the dependency file's directory, not to the directory qcc ran in, so the
loader ignored the file and checked nothing); `--depfile-target` names another target.

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
for make).  For an AOT module (`.qmod`), qcc lists everything the module was compiled from
(`qore_aot_set_module_closure_dep_sink()` in `include/qore/intern/qore_aot_deps.h`):

- the module's own `.qm`, `.qc` and `.ql` files;
- the files that its `%include` directives opened (`qore_program_private::included_files`, recorded by the scanner):
  they are parsed as part of the module; qcc listed none of them before, so neither an incremental build nor the
  loader saw a change to one;
- the modules it requires **directly or not**: the module sees the declarations of what its dependencies re-export,
  and the types of their own dependencies, so a module that listed only its direct dependencies by their sources
  (`--depfile-module-deps=source`) was not rebuilt, nor found out of date, when a module that a dependency re-exports
  changed.  The closure follows `QoreModuleManager::getModuleDependencies()` (the user and binary modules a source
  module loaded, the descriptor dependencies of an AOT module, the declared dependencies of a native binary module);
  each module is listed by its sources (or its file, for a binary module), with the files its `%include` directives
  opened when it was loaded from its source and, for an AOT module of a build tree, everything its own depfile lists;
- the candidate files of the modules of its failed `%try-module` directives.

The depfile of a script object (`.qo`) is unchanged: it lists the modules the script loads directly, and its include
closure as before.  Without a dependency file, the main source of the module and the `.qm` and `.qc` files of its
directory are the sources.

## A dependency loaded from another module

The sources say what the module was compiled from, not which module the loader finds for a dependency now: a module
that appears in a directory of the module path that is searched first (an installed module, a test's stand-in) is
loaded instead of the one the module was compiled against, and the module's code would run against declarations it
was not compiled for.  So once the dependencies of an AOT module of a build tree are loaded, and before the module is
mapped (`QoreModuleManager::checkAOTModuleDependencyFiles()`, called by `loadBinaryModuleFromPath()`), every module
it was compiled with is checked:

- the dependencies of its descriptor, and every module its metadata says it was compiled with: the modules it
  requires, directly or not, and the modules they re-export.  qcc keeps `%requires(reexport)` modules out of the
  descriptor's dependencies (a re-exported module can require the module back, and the loader loads the descriptor's
  dependencies before it maps the module), and a module further down the chain can be loaded from another file while
  its dependent comes from its source, so the descriptor alone is not enough.  qcc writes the names
  (`QoreAOTModuleFileInfo::compiled_with`, format version 2 of the metadata record) from the modules loaded in the
  Program the module was compiled in: the files of the depfile do not say which of them are modules (a file that an
  `%include` directive opened can have the name of a module, `<x>.qm`);
- a module that is loaded is checked as it was loaded; one that is not loaded yet (a module the AOT module
  re-exports, which its initialization loads) is found without loading it, in the search order of
  `loadModuleIntern()` (`QoreModuleManager::findModuleFileUnlocked()`), in the module path that the module's
  initialization searches (`inheritAOTModulePathLists()`): the module's own `%prepend-module-path` directories, which
  qcc writes into its metadata, those of the Program loading it, the module path, those the Program appends, and the
  module's own `%append-module-path` directories.  The initialization inherits the lists of the Program loading the
  module (the one the loader checks with), not of the Program that happens to be current, so both find the same file.

A module is the one compiled against when:

- the file it comes from is listed in the module's depfile (a source module, a binary module, or an AOT module with
  `--depfile-module-deps=artifact`);
- or it is a copy of such a file, with the same name, size and content hash (as the fingerprint file records them; a
  link farm, an installed copy);
- or it is an AOT module of a build tree whose own sources (the file of the module's main source, or the files of its
  module directory, as its own depfile lists them) are listed there.

A native binary module counts as the one compiled against only when it is the listed file or an identical copy of it.
Another build of it, even with the same name and module API version (such as an installed one), is a different
module: AOT code depends on more of a native module than the names of its symbols (constants folded into the code,
the variants it was bound to, the hashdecls and placeholders it was compiled against, purity flags), and the module
API version in the file's name (`<name>-api-<x>.<y>.qmod`) is the version of the module API, not of the module.

An AOT module without its depfile (an installed module, or one compiled by hand without `--depfile`) is not checked:
what it was compiled from is not known, and the loader does not check such a module's own sources either.

Otherwise the module is out of date: the loader raises `AOT-MODULE-STALE`, which, as for any AOT module that cannot
be loaded, loads the source module instead with a `BINARY-MODULE-SOURCE-FALLBACK` warning.  Without a source module
the error stands: AOT code compiled against another module than the one loaded for it can misbehave, so it is not
loaded with a warning (unlike an AOT module compiled without an optional module that is available now).  A module
that differs only in its path (an identical copy) is not another module, so it never gets here.  A builtin
module, a file the sandbox does not let the loader read and a module loaded into a Program without filesystem access
are not checked.  An incremental build cannot see such a module appear (a make prerequisite cannot be a file that
does not exist); the loader reports it and asks for the module to be rebuilt.

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
`examples/test/ir/AOTModuleDependencyStale.qtest`: a changed include file and a changed module that a dependency
re-exports make a module out of date and are listed in its depfile; a dependency loaded from another module makes a
module out of date;
one loaded from its source, from its own AOT module or from an identical copy is the module compiled against.
`examples/test/ir/AOTModuleStaleFallback.qtest`: a built module that is a regular file with its depfile, first in the
module path of a child process as in `run_tests.sh`, is checked (a changed source is found, a touched one is not, and
without the depfile the module is loaded); a module compiled without `--depfile` is not checked; and an out-of-date
module whose source fails reports `AOT-MODULE-STALE` after the source error.
