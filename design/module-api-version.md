# Binary module API version

## Rule

A binary module is compiled against the public Qore headers (`include/qore`, outside `include/qore/intern`) and
carries their inline code, their type layouts and the calls they make into exported functions.  Qore loads a binary
module only if the module API it declares (`QoreModuleInfo::api_major` / `api_minor`, set from
`QORE_MODULE_API_MAJOR` / `QORE_MODULE_API_MINOR`) is in `qore_mod_api_list` (lib/ModuleManager.cpp), which holds only
the current API, and the module file name (`<name>-api-<major>.<minor>.qmod`, derived from the header by CMake)
changes with it.  What a user sees for a module built for another API:

- looked up by its feature name (`%requires`, `load_module("name")`), the loader tries only
  `<name>-api-<supported>.qmod` and `<name>.qmod`, so the module is not found; the `LOAD-MODULE-ERROR` then names each
  `<name>-api-<major>.<minor>.qmod` file of the feature with an unsupported API in the module path (flat and split
  form; the API is read from the file name, the module is not loaded): "...; found '<path>' built for module API <old>:
  rebuild it for module API <current>" (`qore_append_unsupported_api_modules()` in lib/ModuleManager.cpp);
- loaded by its path, its description function is read and the module is refused with "API mismatch, module supports
  API <old>, however only version <current> is supported".

**Any change to the public headers that a module compiled with the previous headers would get wrong must increment
`QORE_MODULE_API_MINOR`** (or `QORE_MODULE_API_MAJOR` for an incompatible redesign of the module interface), in the
same commit, with a release note that modules must be rebuilt:

- inline code whose behavior changes (for example how a `QoreValue` is created, referenced or released);
- the layout of a type that a module creates, embeds or accesses inline (members, base classes, virtual functions);
- the meaning or the signature of an exported function or method that a module calls, or of a callback it implements.

Adding new exported functions, or new types that old modules do not use, needs no bump.

## History

- 2.0 (46560a8429, 2026-02-06, Qore 3.0 development): `QoreModuleInfo`-based registration.  2.0 never shipped in a
  release.
- 2.1 (Qore 3.0): covers all public ABI changes made during Qore 3.0 development after 2.0, none of which bumped the
  API.  They were found through the #5519 crash: f3f91cda6c (#5519) made `QoreValue::makeEnum()`, which was inline and
  created a non-owning enum value, exported and owning a reference to the enum declaration, released by `discard()`;
  a module built with the 2.0 headers created its enum constants without that reference, and the libqore that
  released them over-released the declaration (a stale installed `yaml-api-2.0.qmod` crashed at process exit in
  `QoreEnumMember::deref()`, AOTUnresolvedSymbolDiagnostics.qtest "binary-module types").  A review of the 342 commits
  to the public headers since 46560a8429 found, among others:
  - `InputStream`/`OutputStream::isIoThreadSafe()` inserted in the middle of the vtable;
  - the native iteration virtual methods appended to `QoreIteratorBase`;
  - new virtual methods in `AbstractPollableIoObjectBase`, `AbstractPollState` and `AbstractHttpPollConnection`;
  - `QoreValue`: the inline NULL bool (6da99f6f2e), the NaN-boxing tag ranges (83a5bbb500) and inline short strings
    (0304ffbec8);
  - 64-bit program IDs and the `pgm_id` layout (8004006ee2);
  - the `ExceptionSink` count semantics (9fb539f3da);
  - the `QoreStackLocation` ABI (86ee672a22).
  - `QoreModuleInfo::ns`, through which a module registers the namespace that module teardown purges, with
    `QoreNamespace::purge()` (module namespace teardown, merged with this bump).

- 2.3 (Qore 3.0, #5831; provisional until batch assembly): appends native callback dispatch entries to `ResolvedCallReferenceNode`; modules built
  for the previous API are refused. See the lexical callback entries below.

## Guard

`tools/qore-module-api-headers.py` hashes the installed public header set: the files that
`install(DIRECTORY include/qore ...)` in CMakeLists.txt installs, read with its `PATTERN ... EXCLUDE` list (so `intern/`
and the excluded files are not hashed, and the headers without an extension, such as `qlist`, `safe_dslist`,
`vector_map` and `vector_set`, are), with comments removed and whitespace kept only between two word characters.  In a
git checkout it hashes the files of `include/qore` that git tracks (`git ls-files`), so that untracked files in a
development tree (`.DS_Store`, `*.orig`, `*.rej`, editor swap files) do not fail it; in a source tarball, every file of
the directory.  The
hash is recorded with the module API it was recorded for in `cmake/module-api-headers.txt`.  The check fails when:

- the hash differs and the API does not: the author either increments `QORE_MODULE_API_MINOR` (a change that a
  compiled module would get wrong), or, for a change that cannot affect a compiled module (a new function or type
  that old modules do not use), records the headers with `tools/qore-module-api-headers.py --update` and gives the
  reason in the commit message;
- the API differs from the recorded one: the API was incremented, and `--update` records the headers for it.

It runs as `examples/test/cmake/test_module_api_headers.py` (the CMake tests of CI, of the RPM `%check` and of the
Debian build, and locally with `python3 -m unittest discover -s examples/test/cmake`).  It catches a forgotten bump such
as that of #5519 at review time; it cannot decide by itself whether a change is ABI-relevant, which is the reviewer's
decision.  An abidiff baseline of libqore and of the headers could decide part of it (a follow-up).

## Stale module files in build trees

When the API changes, CMake writes `<name>-api-<new>.qmod` but does not remove `<name>-api-<old>.qmod`, which then stays
in an incrementally built tree, where tools that look for a module file by a pattern find it.
`qore_remove_stale_module_api_files(<target>)` (cmake/QoreMacros.cmake) adds a post-build step that runs
`cmake/QoreRemoveStaleModuleApiFiles.cmake` to remove the other `<name>-api-<major>.<minor>.qmod` files of the module
from its output directory (the module's file base name is `$<TARGET_FILE_BASE_NAME>`, which follows
`LIBRARY_OUTPUT_NAME` and `OUTPUT_NAME`; CMake 3.14, which lacks it, uses `OUTPUT_NAME` or the target name); the
`QORE_BINARY_MODULE*` macros call it (and so external modules built with the installed
macros), as do the in-tree modules that set their suffix themselves (dbi_test, the module-cpp-api test modules).
Test: examples/test/cmake/test_stale_module_api_files.py.

## Packaging

- RPM: `%global module_api` in qore.spec-multi gives libqore `Provides: qore-module(abi) = <api>`, and
  `%qore_module_api` in rpm/macros.qore names the module files of module RPMs; rpm/qore-requires.py makes each module
  RPM require the API of the qore that built it (`qore --latest-module-api`).
- Debian: libqore20 `Provides: qore-module-abi-<major>.<minor>`, from the header through debian/module-api.py in
  `override_dh_gencontrol`, so that a module package can depend on the module API it was built for.
- examples/test/cmake/test_module_api_version.py checks that these name the API of the header.

Test: examples/test/module-cpp-api/module-api/module-api-version.qtest, in every execution mode: apiold (declares API
the preceding minor, built as `apiold-api-<previous>.qmod`) is not found by its feature name and the error names its file, and is refused by
its path with the API mismatch; apicur (the current headers) loads and reports the current API; the hint names each file of a
feature with an unsupported API in the flat and the split form and no other file.

## Lexical callback entries (#5831)

`ResolvedCallReferenceNode::invoke()` now calls the appended `dispatchValue()` slot. The base implementation
scopes canonical native compatibility semantics and calls the module's existing `execValue()` override; an
explicit source-bound invocation uses the appended `dispatchLexicalValue()` slot. Native subclass source does
not need to change. Internal Qore references and closures place their existing execution bodies in the new
ordinary slot, preserving one virtual dispatch and using their declaration-bound argument filters.

The public vtable changes, so `QORE_MODULE_API_MINOR` is incremented in the same change. The provisional assigned version is 2.3. `qore_mod_api_list` continues to contain only the current API, and older
modules are refused by the existing API-mismatch path. Rebuild every external module against the new headers.
No AOT serialization layout, opcode, or feature bit is added; the runtime identity changes.
