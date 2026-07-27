# Migrating module-json into Qore

Tracking issue: [#5366](https://github.com/qoretechnologies/qore/issues/5366)
Consumer: [#5367](https://github.com/qoretechnologies/qore/issues/5367) (builtin `avro`), via
[#5365](https://github.com/qoretechnologies/qore/issues/5365) (Salesforce Pub/Sub event source).

## Goal

Move the entire `module-json` repository into Qore — the binary module and all 16 user modules —
and retire the repository. Three things fall out of it: the QM-metadata bootstrap workaround is
deleted rather than repaired, `%try-module json` / `%ifdef NoJson` disappears from the tree, and a
public C++ JSON API becomes available to every builtin module.

## Decisions

### 1. The codec core stays in the module and is published as a C++ API

**Superseded, and then reverted.** As first implemented this decision moved the codec into
libqore; it is back in the module now that
[#5371](https://github.com/qoretechnologies/qore/issues/5371) exists. Both states are recorded
here because the reasoning for the move is what motivated #5371.

`ql_json.qpp` lines ~411-1193 are a hand-written JSON parser and serializer with **no jsoncons
dependency** (jsoncons is used only by `JsonSchemaImpl.cpp`, `QC_JsonSchema.h` and `ql_cbor.qpp`),
and `parse_json()` / `make_jsonrpc_request*()` were already `DLLEXPORT` in `src/ql_json.h`.

That core moved to `lib/QoreJson.cpp` with an installed header `include/qore/QoreJson.h`, because
`modules/avro` needed a C++ JSON parser and "the alternative — leaving the codec in the module and
having `avro` resolve symbols across `dlopen()`ed modules — was rejected as fragile". That
judgement was correct about *raw* symbol resolution, which fails as a lazy-binding process abort
rather than as an error; the module C++ API mechanism (`design/module-cpp-api.md`) makes it a
versioned, on-demand resolution with a clean exception on mismatch instead.

So the codec is back in `modules/json/src/QoreJson.cpp` with a module-private
`modules/json/src/QoreJson.h`, and `include/qore/QoreJsonApi.h` publishes it as `QoreJsonApi`.
libqore never used the codec — the only `parse_json` / `make_json` references anywhere under
`lib/` were two comments in `QoreAOTRuntime.cpp` — so the core library loses ~950 lines it had no
use for, and the natural layering is restored. JWT (OpenSSL), `JsonSchema`, CBOR and TOON were
never affected either way.

The consequences that mattered are preserved: jsoncons never enters libqore, and `modules/avro`
still has no link-time dependency on `modules/json`; it resolves `QoreJsonApi` on first use, which
loads the `json` module on demand. `bin/qore-extract-qm-metadata` uses JSON at the *Qore* level
(`QoreApiMetadata`'s `MetadataStore::toJson()`), so the qm-metadata bootstrap never depended on
the codec's location — only on `json` remaining a builtin module, which it is.

Removing `<qore/QoreJson.h>` is an API break only once 3.0.0 has released. It has not, so the
window was open; after 3.0.0 ships, the header would have had to stay until the next major
version.

### 2. jsoncons is vendored at v1.8.1, not fetched and not discovered

Vendor `modules/json/third-party/jsoncons/{include,LICENSE,README.md}` — 3.8 MB of headers, pinned
at **v1.8.1** (2026-06-09, newest stable). `doc/`, `test/`, `examples/`, `examples_boost/` and
`fuzzers/` are not vendored (~14 MB dropped). No git submodule: the Qore tree has none today.

**Why not FetchContent.** Every FetchContent user in this tree is either optional or backs a
library distros actually ship (nghttp2, c-ares, tree-sitter). jsoncons is packaged only on
Debian unstable / Ubuntu (`libjsoncons-dev`); it is absent from Fedora, Alpine, Homebrew and
MacPorts, and no CI deps image installs it. A "fallback" would therefore be the path taken on
essentially every build, making a GitHub round-trip at configure time a hard dependency of the
core language build on 3 of 4 CI targets — a strictly worse failure mode than the metadata gate
being deleted. It would also fire twice per image, since `qore-test-base/prep-*-build.sh` and
`qorus/docker/*/prep_base*.sh` both configure qore twice; the `QORE_JEMALLOC_URL` local-tarball
override in qore-test-base exists precisely because of that pain.

**Why not system discovery.** module-json's `find_path(JSONCONS_INCLUDE_DIR ...)` has no version
guard, so on a modern Ubuntu box it silently picks up system headers of a different major version.
That is a latent bug, not a feature to preserve; the `find_path` block is dropped.

**Port cost: compiles clean, but not behaviour-identical.** All three jsoncons-dependent
translation units (`JsonSchemaImpl.cpp`, `QC_JsonSchema.qpp`, `ql_cbor.qpp`) compile against
v1.8.1 with no source changes, and every API name in use (`make_json_schema`, `schema_error`,
`validation_message`, `walk_result`, `ser_error`, `basic_json_visitor`, `json_type`,
`string_view`) still exists. **A clean compile was not sufficient**: `Cbor.qtest` crashed with
`free(): invalid pointer` under 1.8.1 while passing under 0.176.0.

Root cause: jsoncons does not guarantee that returning `false` from a `visit_begin_*()`
callback ends the parse immediately. Under 0.176.0 it did; under 1.8.1 the CBOR parser still
delivers the matching `visit_end_*()` callback. `CborToQoreVisitor::visit_end_object()` then ran
`stack.back().releaseHash()` on an empty container stack. The only thing guarding that invariant
was an `assert()`, which is compiled out of release builds — so it read a garbage pointer and
freed it.

That was a latent bug in the module, not a jsoncons defect: the code depended on undefined
behaviour being benign and was one library-behaviour change away from heap corruption. Fixed by
giving the visitor explicit `aborted` state, checked at the top of every callback, and by
enforcing the empty-stack and `has_key` invariants in release builds instead of via `assert()`.
`addValue()` now returns a status and takes ownership of the value on every path.

Lesson for any future dependency bump here: compile success proves nothing about visitor
contracts. Run the suites.

Note for later: v1.8.1 ships `jsoncons_ext/toon/`, which overlaps the module's hand-written
`ql_toon.qpp`. Out of scope here; worth its own issue.

### 3. The module versions with Qore

No builtin module sets its own `VERSION`; they all inherit the top-level Qore version, so `json`
reports 3.0.0 rather than module-json's 1.12.0. This satisfies the existing
`%try-module json >= 1.5` / `>= 1.11` guards in `qlib` for free during the transition, and Phase 5
drops the version constraints entirely. module-json 1.12.0 remains the last standalone release.

### 4. Tests split by kind, and go where the runner will find them

`run_tests.sh` only scans `examples/test`, so suites under `modules/<name>/test/` (protobuf,
dataframe, tokenizer, astparser, i18n, krb5) are **not** part of the standard run. For a module
that is now mandatory that is not acceptable, so the binary suites go to
`examples/test/modules/json/`, following the convention `sshutil` already uses
(`examples/test/modules/sshutil/`). The 23 user-module suites go to
`examples/test/qlib/<Module>/`.

`JsonRpcClient.qtest` depends on the `JsonRpcClientIo` and `JsonRpcConnection` user modules, so it
moves with them in Phase 4 rather than with the binary module.

The C++ API smoke test lives separately at `examples/test/json/json_cpp_api.cpp`: it exercises
libqore's public JSON API, not the module, and is a C++ target rather than a `.qtest`.

All Qore suites convert from module-json's `%requires ../qlib/JsonLd` idiom to the Qore
convention: `%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"` + plain `%requires`.

## Obstacles found during survey

### The CMake target name `json` is already taken

nghttp2's FetchContent doc subdir defines `add_custom_target(json)` — confirmed at
`build/CMakeFiles/TargetDirectories.txt:1408` (`_deps/nghttp2-build/doc/CMakeFiles/json.dir`).
`QORE_BINARY_MODULE_INTERN2` derives the `.qmod` filename from the target name
(`cmake/QoreMacros.cmake:371`), so renaming our target is not an option.

Fix: nghttp2 gates that subdirectory on `ENABLE_DOC`, so no source patching is needed — Qore now
forces `ENABLE_DOC OFF` (saved and restored symmetrically) alongside the `BUILD_TESTING` /
`ENABLE_LIB_ONLY` overrides it already applies before `FetchContent_MakeAvailable`. We do not
build nghttp2's docs, so the subdirectory has no business being configured at all.

Related: `QORE_FINALIZE_USER_MODULE_DEPENDENCIES` (`cmake/QoreMacros.cmake:1236-1241`) carries a
`/_deps/` `SOURCE_DIR` guard written specifically for this collision. Once the builtin target
exists it becomes the thing that wires `%requires json` -> the `json` target correctly, so it needs
re-verifying, not deleting.

### `QORE_QM_METADATA_DEPENDS` alone is not sufficient

Issue #5366 proposes adding `json` to the builtin module list at `CMakeLists.txt:3020`, which gives
build *ordering* via `QORE_AOT_BINARY_MODULE_TARGETS` -> `QORE_QM_METADATA_DEPENDS`. But AOT and
metadata runs *resolve* modules through `QORE_QM_METADATA_MODULE_DIRS`
(`CMakeLists.txt:2978-2990`). `${CMAKE_BINARY_DIR}/modules/json` must be added there too, or every
one of the ~112 qlib modules that `%requires json` fails AOT parse-commit with `LOAD-MODULE-ERROR`.

### A stale installed `qore-json-module` shadows rather than conflicts

`QoreModuleManager::addStandardModulePaths()` (`lib/ModuleManager.cpp:904-919`) searches
`MODULE_VER_DIR` before `MODULE_DIR`, so a builtin `json` wins over a leftover external
`json-api-2.0.qmod`, and the versioned user-module dir likewise shadows the old 16. Not a runtime
hazard, but packaging should still `Obsoletes`/`Replaces` the old package so upgrades don't leave
dead files behind.

## Scope

| Item | Count |
|---|---|
| Files referencing `NoJson` (repo-wide) | 307 |
| — in `qlib/` | 165 |
| — in `examples/` | 126 |
| — in `bin/` | 9 |
| — in `modules/*/test/` | 4 |
| — `cmake/QoreConfig.cmake.in`, `doxygen/lib/90_cmake.doxygen`, `tools/hl7v2-schema-gen.qr` | 3 |
| Files with `%try-module json` | 253 |
| Files referencing `NoCbor` (gated on `json >= 1.11`) | 3 |
| User modules moving | 16 |
| `.qtest` suites moving | 32 (9 binary + 23 user-module) |
| `design/` docs moving | 9 |

No name collisions: none of the 16 module names exist in `qlib/` today. No dependency obstacle:
all of `ConnectionProvider`, `DataProvider`, `FileLocationHandler`, `HttpClientIo`,
`HttpClientStreamingIo`, `HttpServer`, `HttpServerUtil`, `HttpStreamClient`, `Logger`, `Mime`,
`RestClient`, `RestClientIo`, `ServerSentEventClient`, `ServerSentEventHandler` and `Util` are
already Qore-delivered. `uuid` stays external and keeps its `%try-module` treatment in 3 modules.

## Phases

Each phase must build and test green before the next starts.

1. **Codec core into libqore.** `lib/QoreJson.cpp` + `include/qore/QoreJson.h`. `module-json`
   keeps building against the installed header; nothing else changes yet.
   *Reverted by decision 1 above:* the codec is back in `modules/json/src/QoreJson.cpp` and is
   published as `QoreJsonApi` through the module C++ API mechanism instead.
2. **`modules/json/`.** Vendor jsoncons v1.8.1; move 7 QPP sources, 3 `.cpp`, headers,
   `docs/mainpage.dox.tmpl`; `CMakeLists.txt` modelled on `modules/protobuf`; resolve the nghttp2
   target collision; 9 binary suites to `modules/json/test/`.
   *Gate:* `qore -l json` from the build tree, 9 suites green.
3. **Delete the build workaround.** Builtin module list + `QORE_QM_METADATA_MODULE_DIRS`;
   unconditional `QORE_METADATA_DIR`; delete `CMakeLists.txt:3031-3072` and the stale comment at
   2967-2975; drop `json` from `_qore_aot_probe_modules` (3088); delete the `JSON-NOT-AVAILABLE`
   fallback in `bin/qore-extract-qm-metadata:46-70`.
   *Gate:* from-scratch build in a container with **no qore installed** produces
   `share/qore/metadata/` — the case that silently fails today.
4. **The 16 user modules**, their 23 suites and fixture dirs, and the 9 design docs.
   *Gate:* `./run_tests.sh -d qlib` subset green.
5. **Consumer cleanup.** The 307/253/3 file sweep above. By inspection, not by `sed`: several
   sites are hash-literal members (e.g. `qlib/RestClient.qm:800`) rather than whole statements.
   `RestClient.qm` has 9 branch points and is the most involved single file.
6. **Docs, licence, packaging.** `120_modules.dox.tmpl` and `900_release_notes.dox.tmpl`, with
   every module placed **alphabetically** in its list. Licence reconciliation (module-json is dual
   LGPL/MIT, jsoncons is BSL-1.0). `Obsoletes`/`Replaces` in `qore.spec-fedora` and
   `debian/control`.
7. **valgrind.** One pass at the end over the json binary suites and the moved user-module suites.

## Cross-repo work

### qore-test-base

- Drop `json` from `CMAKE_MODULES`: `prep-alpine-deps.sh:26`, `prep-ubuntu-deps.sh:34`,
  `prep-macos.sh:273`.
- The two-pass qore build **stays** — `yaml xml uuid msgpack process imagemagick sysconf oracle`
  still drive `%ifdef No<Name>` AOT branches. But the comments at `prep-alpine-build.sh:30,140`,
  `prep-ubuntu-build.sh:35,135` and `build-macos.sh:69,144` claiming pass 1 disables metadata
  because json isn't loadable become false: pass 1 will now produce metadata. Rewrite them.
- `package-docs.sh` needs no change; json tags simply arrive under `qore-module-docs/qore/`
  instead of `qore-module-docs/json/`. That fact drives the qorus Doxyfile edits below.

### qorus

- `CMakeLists.txt:359` — remove `json` from `QORE_CMAKE_MODS` (also removes it from the
  doc-symlink loop at 3567 automatically).
- `CMakeLists.txt:2299` — `FOREACH (it ${QORE_CMAKE_MODS} ${QORE_AUTOTOOLS_MODS} reflection
  astparser)` generates Java bindings for binary modules; `json` must be added to the trailing
  explicit list or the Java API classes silently disappear.
- `QORUS_QO_LOAD_MODULES` at 2632 / 2735 / 2846 / 2924 / 2971 — **no change**; module names and
  install paths are unchanged.
- `build-config.qembed:38-41` — delete the `"json"` entry.
- `docker/ubuntu/prep_base.sh`, `prep_base_debug.sh`, `prep_base_go.sh`,
  `docker/alpine/prep_base.sh`, `prep_base_minimal.sh` — remove `json` from `CMAKE_MODULES` and
  `QORE_AOT_BOOTSTRAP_MODULES` (10 lines).
- `docker/common/verify_aot_qmods.sh` — its sentinel is `A2aServerHandler`, justified by
  "ships in module-json/qlib/... and module-json is in every prep_base.sh CMAKE_MODULES list".
  That premise dies; repoint the sentinel or pick a still-external module. Must not be left
  silently passing.
- `tools/copy-libs.sh:3` — drop `module-json`.
- `doxygen/Doxyfile.cmake:2087-2099` — move the 13 tags out of the `qorus/json/` block:
  `qore/modules/json/json.tag=../qore/modules/json/html` for the binary module and
  `qore/<Module>.tag=../qore/modules/<Module>/html` for each user module, inserted alphabetically
  into the existing `qore/` block. The current block lists only 13 — `FhirRestClient`,
  `FhirRestDataProvider`, `JsonFileDataProvider` and `JsonRpcClientIo` are missing today and
  should be added while there.
- `doxygen/Makefile:98` — remove `json` from `DOC_MODULES`.
- `doxygen/doxyfile.tmpl:1463-1465` — legacy autotools doxyfile, same treatment.
- `distrib/qorus.spec.tmpl:33,49` — drop `BuildRequires`/`Requires` on `%{name}-qore-json-module`.
- `bin/qdb` — the one qorus file carrying a json guard.
- `test/lsp-websocket.qtest:10797` asserts completion results contain no json-module symbols;
  verify that still holds once json is builtin.

### module-json: `develop` only, repository stays live

Issue #5366 calls for retiring the repository outright -- archiving it on GitHub and removing the
GitLab mirror, CI pipeline and `github-ci-helper` branch. **That is wrong and must not be done.**
module-json branches are named after the **Qore** release line they support, not the module
version, and the branches for earlier lines are still built and released from there because Qore
and Qorus versions on those lines depend on them:

| Branch | Qore release line | json module version |
| --- | --- | --- |
| `develop` | superseded by this migration | 1.12.0, its final release |
| `2.x` | Qore 2.x | 1.9.2 |
| `1.19.x` | Qore 1.19.x | 1.8.3 |
| `1.12.x` | Qore 1.12.x | 1.8.2 |
| `0.9.x`, `0.9.4`, `0.9.3`, `0.8.13`, `0.8.12`, `master`, ... | earlier Qore releases | per branch |

So only `develop` is superseded. The GitHub repository, the GitLab mirror, its CI pipeline and the
`github-ci-helper` branch all stay in place to serve the maintenance branches.

The only change made there is on `develop`: its plain `README` is replaced with a `README.md`
recording that Qore 3.0 and later ship the module in-tree, where each piece went, what it means
for consumers on 3.0+ (nothing to install, no `qore-json-module` dependency, no `%try-module json`
guards) and on earlier releases (nothing changes), and stating explicitly that the repository is
not retired.

## Sequencing risk

The three repos cannot merge independently. qorus CI clones modules from `build-config.qembed`
against qore `develop`; the moment qore `develop` ships builtin json, a qorus image that still
clones and installs `module-json` gets two json modules. Harmless at runtime given the module
search order, but the AOT bootstrap loop and `verify_aot_qmods.sh` will misbehave.

Land qore first, then qore-test-base and qorus within the same window, and rebuild the deps images
before the next qorus pipeline runs.
