# Installed i18n Catalog Reconciliation

## Status

Implemented. This document records the ownership model the install rules hold
themselves to, because the failure they prevent is not visible from any one
file: it takes an *earlier* generation of the sources, a *later* one, and a
runtime that correctly refuses to choose between them.

Relevant code:

- `cmake/QoreMacros.cmake` — `QORE_INSTALL_USER_MODULE_CATALOGS()` records what
  it installs; `QORE_FINALIZE_CATALOG_INSTALL()` emits the reconciliation pass
- `cmake/QoreReconcileCatalogInstall.cmake` — the pass itself, run at install time
- `CMakeLists.txt` — calls `qore_finalize_catalog_install()` after every module
- `modules/i18n/src/ql_i18n.qpp` — `i18n_discover_catalog_files()`, the consumer
- `qlib/DataProvider/DataProviderPresentation.qc` —
  `DataProviderPresentationCatalog`, which rejects conflicting fragments
- `examples/test/ir/CMakeBuildHelpers.qtest` — the install-lifecycle tests

The user-facing view of the same subject is `modules/i18n/docs/catalogs.dox.tmpl`
(catalog discovery and composition).

## The defect this fixes

Catalog installation used to be additive and nothing else. Every fragment any
past generation of the sources installed stayed on disk forever, `install()`
having no notion of what it installed last time.

Discovery then does exactly what it should: `i18n_discover_catalog_files()`
scans both supported on-disk layouts under every search root

| Layout | Path | Written by |
|---|---|---|
| Flat | `<domain>/<locale>.json`, `<domain>.<locale>.json` | hand-placed catalogs; Qore before the fragment layout |
| Fragment | `<domain>/<locale>/<Owner>.json` | `QORE_INSTALL_USER_MODULE_CATALOGS()` |

and returns every file it finds. `DataProviderPresentationCatalog` then composes
them and throws `DATA-PROVIDER-PRESENTATION-CATALOG-ERROR` when two of them
define one message id differently — which is the correct behaviour, because
nothing in the file tells it which of the two owners is obsolete.

So a successful build and install could leave an unusable catalog tree. Measured
on one developer machine before this change: of 268 installed domains, 107 failed
to compose. 106 of those were Qore's own layout migration — the commit that
introduced the fragment layout shipped no cleanup, so 267 `<domain>/<locale>.json`
files written by the previous release were still on disk and still discovered.

This is an installation lifecycle defect. Neither discovery nor conflict
detection is changed by the fix.

## Reconciliation is per project, not per root

Several projects install into one catalog root **by design** — that is what the
owner-qualified fragment layout exists for. Qore's shared root holds fragments
from downstream provider modules that have no source in this repository at all.

The rule that follows is absolute: **a project may remove only the fragments it
installed itself.** Reconciliation is therefore scoped by a manifest per
(project, catalog root), never by "everything under the root that this build did
not just write".

Two consequences worth stating plainly:

- Qore's reconciliation fixes Qore's own residue, and the mechanism — shipped in
  the installed `QoreMacros.cmake` and reached by every downstream repository
  through `QoreConfig.cmake` — fixes each other project's residue as soon as
  that project installs again.
- An orphan installed into Qore's root by a downstream module can only be
  removed by that module's own project. Qore must not remove it, because from
  Qore's side an unknown fragment and a legitimately-shared fragment are the
  same file.

### What "its own" means

Two things identify a project's fragments, and the second is what makes the
mechanism work on a root that predates it:

| Signal | Covers | Blind to |
|---|---|---|
| The project's manifest | Every fragment the last recorded generation installed | Anything installed before the project ever wrote a manifest |
| The owner name in `<domain>/<locale>/<Module>.json` | Every fragment of a module this generation installs catalogs for | A module this project no longer ships at all |

The owner name is decisive because a fragment is named after the module that
owns it, and one module name means one module everywhere: two modules of the
same name cannot both be installed, or `%requires` could not resolve either. So
the project installing catalogs for `Foo` owns every `Foo.json` below the root,
whether or not any manifest recorded it.

That is what retires residue from generations that predate this mechanism —
including, for a downstream module, apps it described before they migrated to a
native provider. The two signals are used together: the manifest catches
fragments whose owner module the project has dropped entirely, and the owner
name catches fragments no manifest ever saw.

The manifest project key defaults to `CMAKE_PROJECT_NAME` and is overridable
(`QORE_FINALIZE_CATALOG_INSTALL(<key>)` or `QORE_CATALOG_MANIFEST_PROJECT`).
Two projects sharing a root must not share a key: the key is the only thing
keeping each project's removals confined to its own fragments.

## What one pass does

`QORE_FINALIZE_CATALOG_INSTALL()` emits one `install(CODE)` per catalog root
(per install component using it), which includes
`QoreReconcileCatalogInstall.cmake` with the root, the manifest path, and the
generated manifest of the current generation. At install time it, in order:

1. reads the previous manifest, if any;
2. removes the files it lists that the new manifest does not — files only, only
   below the root, and only entries that cannot escape it;
3. removes every `<domain>/<locale>/<Module>.json` below the root whose owner
   module this generation installs catalogs for and whose path the new manifest
   does not list;
4. performs the flat-layout sweep described below, on the bootstrap generation
   only;
5. prunes locale and domain directories the removals emptied;
6. publishes the new manifest last, and atomically (staged copy + `rename`).

Step 3 runs on every install, not only the bootstrap generation: the invariant it
rests on does not weaken over time, and running it always makes reconciliation
self-healing when a manifest is lost, or when an older package is installed over
a newer one and then upgraded again.

Step 6 is last because a manifest that lists fragments whose removal never ran
would strand them permanently: the next generation would diff against a
generation that never happened. Atomicity gives the same guarantee against an
interrupted install.

Removal correctness does not depend on install-rule ordering — a path in both
manifests is never removed, and the sweep targets a layout no current generation
writes — so a project whose catalogs are installed from subdirectories stays
correct even though its reconciliation rule runs before them. Only the
"manifest published last" property wants the finalize call at the end.

## Why the flat sweep is safe

Owner adoption covers orphans in the *fragment* layout on the first install, but
nothing identifies an owner in the flat layout: `<domain>/<locale>.json` carries
no owner name at all. Those files need a rule of their own, or the residue that
motivated this work survives every install.

The rule is narrow and provable: **no current generation of
`QORE_INSTALL_USER_MODULE_CATALOGS()` ever installs `<domain>/<locale>.json`**,
under any root — every install is owner-qualified. Below a Qore-managed catalog
root (a root equal to `QORE_CATALOG_DIR`) the only thing that ever installed that
layout was Qore's own pre-fragment-layout macro. Such a file is therefore
obsolete by construction, and the sweep removes it.

Two deliberate restrictions:

- The sweep runs only on the **bootstrap** generation — the one that finds no
  manifest of its own. The flat layout remains a supported *discovery* layout,
  so a catalog placed below the root by hand after the migration is left alone.
- It is scoped to `QORE_CATALOG_DIR`. A downstream project's own catalog root
  never had a pre-fragment-layout generation, so there is nothing there to sweep,
  and the other flat spelling (`<domain>.<locale>.json`) is not touched anywhere.

A downstream project that installs into Qore's shared root does run the sweep on
its own first install. That is intended: the files it can remove are exactly the
ones Qore's own macro left behind.

## The manifest

Path: `<catalog root>/.qore-catalog-manifests/<project>.manifest`.

It holds a comment header and the fragment paths of the generation, relative to
the catalog root, sorted. Relative paths keep a `DESTDIR`-staged install and a
live install byte-identical, and let a relocated tree stay reconcilable.

The content is a pure function of the recorded fragment set, so reinstalling an
unchanged source tree removes nothing and rewrites the same bytes. The manifest
directory begins with a dot, so it is not a catalog domain and the sweep's
`<root>/*/*.json` glob cannot reach it; discovery ignores it because it holds no
`.json` files.

`$ENV{DESTDIR}` is applied to the root exactly as elsewhere in `QoreMacros.cmake`
(see `QORE_AOT_APPEND_INSTALL_REMOVE_PATH` and `QORE_INSTALL_QMOD_ATOMIC`), so a
package build reconciles the staged tree and never touches the live prefix.

## Known limitations

- Fragments installed by another project are never removed, by design (above).
  A project that stops installing catalogs *entirely* also registers no catalog
  root, so its last generation's fragments stay until it installs catalogs again.
- Owner adoption reaches only modules the current generation still installs
  catalogs for. A module dropped from a project before that project ever wrote a
  manifest leaves fragments neither signal can claim; they must be removed by
  hand, once.
- A component-scoped install (`cmake --install --component <c>`) reconciles with
  the manifest of the whole configured generation, not of that component alone.
  For a root fed by several components this can record a fragment the run did not
  install; the effect is a later removal that finds nothing, which is harmless.
- One root spelled two ways (absolute in one call, relative in another) is two
  roots and two manifests. Both reconcile correctly against their own records.
- Two projects that ship a module of the same name can install the same fragment
  path into one root; the last one to install owns the file, and either one's
  reconciliation may remove it. Module names are already required to be unique
  for module loading, so this is not a new namespace.

## Tests

`examples/test/ir/CMakeBuildHelpers.qtest` configures, builds and installs
fixture CMake projects into temporary prefixes and covers: an app migrating
between owner modules; every installed domain composing afterwards; an unrelated
app keeping its old owner's fragment; another project's fragment in the same root
and the same domain surviving; an orphan retired by owner name on a first
install that no manifest could have seen, alongside a same-vintage fragment of
another project's module that survives it; the flat-layout sweep; an unchanged
reinstall
removing nothing and leaving a byte-identical manifest; a manifest entry that
would reach outside the catalog root being refused; and `DESTDIR` staging
reconciling the staged root only.
