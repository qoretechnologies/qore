# Provider-index app and action failures

Copyright 2026 Qore Technologies, s.r.o.

## Problem

Provider-index discovery was fail-closed: any app or action whose initialization failed made
`DataProviderActionCatalog::sealDiscovery()` throw `DATA-PROVIDER-DISCOVERY-ERROR`, so no index was published at
all. One app whose optional schemas were missing, or whose schema cache hit an unrelated bug, blocked the index of
every other app, and consumers (such as Qorus' startup regeneration and `qctl update-index`) had no index to report
the failure from.

## Design

### Classification

`DataProviderActionCatalog::isAppDiscoveryFailure()` identifies a failure that belongs to one app or action only:
it has an `app`, and is one of:

| phase                       | kinds                | cause                                                     |
|-----------------------------|----------------------|-----------------------------------------------------------|
| `registration`              | `app`, `action`      | a retained initialization failure                         |
| `expected-inventory`        | `app`, `action`      | a declared identity that was not registered               |
| `publication`               | `app`, `scheme`      | no module path, or the app's scheme is not registered     |
| `serialize-action-metadata` | `action`             | the action's metadata is not plain data                   |

Everything else still blocks the generation: `source` and `factory` failures (a discovery module that cannot be
loaded), inventory callback failures, stale catalog, scheme registry or inventory snapshots, worker failures and
worker merge conflicts, and any failure without an app.

### Sealing

`sealDiscovery(session, record_app_failures = False)`: with `record_app_failures`, app failures move from
`failures` to the report's `unavailable` list; the token is issued if `failures` is empty. `complete` is
`!failures && !unavailable`, so a generation with unavailable identities is qualified for publication but never
reported as complete. Without the argument the behavior is unchanged (strict), which release and installed
qualification scripts use.

### Building (ProviderIndex)

Single-process and worker builds seal with `record_app_failures`. A worker seals its own generation the same way,
exits 0 when only app failures occurred, and returns its `unavailable` list with the module names of their owners
(`failure_modules`), since the building process does not load the worker's modules. The building process merges
the workers' lists with its own `expected-inventory` checks over the merged state, and drops a worker's
`expected-inventory` entry whose identity another worker registered.

`getFailureMap()` turns the list into the index's `failuremap`: app -> list of `ProviderIndexFailureInfo` records
(`kind`, `app`, `action`, `phase`, `error`, `description`, `module`), the app's own records first, then its actions
in name order. Exact duplicates are dropped, and an `expected-inventory` record is dropped when the same identity
has another record, which caused it. `argument` is not recorded: it need not be plain data. `module` is the
registry's module name for the owner path, or the owner itself when it is already a module name; an unresolvable
path is left out rather than guessed.

What stays in the index for a failed app is whatever it registered: an app that registered its info and scheme
before failing (for example a TypeScript app whose action schemas are unavailable) is indexed with its info,
`appmap` and `schememap` entries, and the actions that did not fail; an app that never registered has only its
failure records. No routing entry or info is invented for it.

The summary gains `failed_apps` (apps with an app-level record) and `failed_actions` (distinct failed actions); one
warning names up to 20 failures with their errors and the remaining count, and each failure is logged at debug
level. Index I/O errors (`PROVIDER-INDEX-ERROR`) are raised exactly as before.

### Format (metadata cache version 9)

The metadata cache has one more file, `qore-data-index-<sha256>.msgpack`, referenced by the manifest's `failures`
member with its `size` and `sha256`, holding `compress(msgpack({<app>: [<record>, ...]}))` with apps sorted. It is
written only when the writer passes `failuremap` (the builder always does, empty when nothing failed), so a
manifest without the member means "failures not recorded", like `factories`. The file is removed with the index
that references it and is verified and counted by `readSerializedDataProviderIndex()`, which returns it as
`index.failuremap`, so a read index can be written again with its failures.

The canonical index file does not contain failures: it exists for earlier ProviderIndexUtil versions, whose typed
casts reject unknown keys. The routing index is unchanged (version 4).

### Reading

`getIndexedFailures(*app)`, `tryGetAppFailure(app)`, `tryGetActionFailure(app, action)` (the action's record, or
else its app's), and `hasIndexedFailureInfo()`. The file is read once per index state, when failures are first
requested (`failure_info_checked`, reset by `installIndexState()`/`resetIndexState()`), without reading any app's
metadata. Each record is cast to `ProviderIndexFailureInfo` when the file is read; a file that does not match its
digest or holds an invalid record is reported by `getIndexReadError()`, the failures are then unavailable
(`NOTHING`), and the apps are unaffected.

### Compatibility

- Indexes of metadata cache versions 4 to 8 are read unchanged; they do not record failures
  (`hasIndexedFailureInfo()` is `False`).
- Earlier ProviderIndexUtil versions cannot read a version 9 metadata cache; they read a canonical index file if
  one was published (`write_canonical_yaml`), otherwise they report the index as unreadable.
- `DataProviderDiscoveryReportInfo.unavailable` defaults to an empty list, so existing report constructors and
  strict callers are unaffected.
