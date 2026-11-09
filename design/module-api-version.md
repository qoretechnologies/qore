# Binary module API version

## Rule

A binary module is compiled against the public Qore headers (`include/qore`, outside `include/qore/intern`) and
carries their inline code, their type layouts and the calls they make into exported functions.  Qore loads a binary
module only if the module API it declares (`QoreModuleInfo::api_major` / `api_minor`, set from
`QORE_MODULE_API_MAJOR` / `QORE_MODULE_API_MINOR`) is in `qore_mod_api_list` (lib/ModuleManager.cpp), which holds only
the current API; otherwise loading raises `LOAD-MODULE-ERROR` with the API mismatch, and the module file name
(`<name>-api-<major>.<minor>.qmod`, derived from the header by CMake and `%qore_module_api` in rpm/macros.qore) changes
with it.

**Any change to the public headers that a module compiled with the previous headers would get wrong must increment
`QORE_MODULE_API_MINOR`** (or `QORE_MODULE_API_MAJOR` for an incompatible redesign of the module interface), in the
same commit, with a release note that modules must be rebuilt:

- inline code whose behavior changes (for example how a `QoreValue` is created, referenced or released);
- the layout of a type that a module creates, embeds or accesses inline (members, base classes, virtual functions);
- the meaning or the signature of an exported function or method that a module calls, or of a callback it implements.

Adding new exported functions, or new types that old modules do not use, needs no bump.

## History

- 2.0 (Qore 3.0 development): `QoreModuleInfo`-based registration.
- 2.1 (Qore 3.0): f3f91cda6c (#5519) made `QoreValue::makeEnum()`, which was inline and created a non-owning enum value,
  exported and owning a reference to the enum declaration, released by `discard()`.  A module built with the 2.0
  headers created its enum constants without that reference, and the libqore that released them over-released the
  declaration: a stale installed `yaml-api-2.0.qmod` crashed at process exit in `QoreEnumMember::deref()`
  (AOTUnresolvedSymbolDiagnostics.qtest "binary-module types").  The API was not bumped then; 2.1 refuses such modules.

## Guard (proposal)

A cheap CI check: a script that hashes the public headers (`include/qore/*.h`, excluding `intern/`) after removing
comments and whitespace, and compares the hash with the one recorded next to `QORE_MODULE_API_MINOR` (for example in
`include/qore/module-api-headers.sha256`).  A change of the hash without a change of the API version fails the check
and asks the author to either bump the API or, for a change that cannot affect a compiled module (a new function, a
comment-only change that the normalization misses), to re-record the hash with a justification in the commit message.
This catches the forgotten bump of #5519 at review time; it cannot decide by itself whether a change is ABI-relevant.

Test: examples/test/module-cpp-api/module-api/module-api-version.qtest loads a module that declares API 2.0 (refused,
with the API mismatch text) and one built with the current headers (loads, reports 2.1), in every execution mode.
