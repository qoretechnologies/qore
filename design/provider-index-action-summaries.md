# Provider-index action summaries

Copyright 2026 Qore Technologies, s.r.o.

## Problem

Restoring an indexed action restores the descriptions of its option types (from the type tables of its app's
chunk, see [provider-index-type-tables.md](provider-index-type-tables.md)) and of its data provider info. Most
consumers only list or select actions by their action-level fields (names, descriptions, codes, metadata), yet the
only way to read those fields was to restore every action of every app: about 5 s and 600 MB for a real index of
6,058 actions, spent before a caller could select the few actions it needed.

## Design

### Format (metadata cache version 8)

The metadata cache has one more file, `qore-data-index-<sha256>.msgpack`, named after the digest of its contents
like the chunks and the factory info file, and referenced by the manifest's `actions` member with its `size` and
`sha256`. It holds `compress(msgpack({<app>: {<action>: <summary>}}))`, apps and actions in sorted order.

A serialized summary is the serialized action as written to its app's chunk, without `options`,
`data_provider_info`, `output_type`, `get_output_type`, and `get_dynamic_options`, plus:

- `option_names`: the names of its options, in index order;
- `has_expressions`: whether its data provider info declares expressions.

`writeDataProviderIndexByApp()` takes the summaries from each app's actions as it writes the app, and writes the
file after the chunks, before the manifest. `removeUnreferencedIndexFiles()` keeps the file the manifest references
and removes the others. `readSerializedDataProviderIndex()` verifies the file and counts its size, so that an
incomplete copy of an index is not read, but does not return it, since it is derived from the actions.

For all apps of the real index, the file is about 450 KB (2.6 MB of MessagePack); reading and casting it takes
about 0.1 s and holds about 25 MB.

### Reading

Summaries are returned as `DataProvider::DataProviderIndexedActionSummaryInfo` hashes, which inherit
`DataProviderActionBaseInfo` (the members shared by registered and indexed actions) and add `option_names` and
`has_expressions`, by `tryGetActionSummariesForApp()`, `tryGetActionSummary()`, `getAllActionSummaries()`, and
`selectActionSummaries()`.

- The summaries of the index in use are read once, when first needed (`checkActionSummaries()`), under the same
  single-reader flag as the other serialized metadata (`raw_metadata_loading`), which a reload waits for. They are
  cast when read; a summary that cannot be cast is logged and left out.
- Where they come from:
  - the action summary file (version 8);
  - for an index without it (versions 4 to 7), or when it cannot be read (reported by `getIndexReadError()`), the
    chunk of each app, whose actions are dropped as soon as their summaries are taken; no type description is
    restored, and a position in a type table is never resolved. The absence of the file is logged once;
  - without a metadata cache (an index read from its canonical file), the serialized and restored actions in memory.
- Cache edits are applied when summaries are returned, not stored in the summaries read from the index: apps removed
  with `removeCachedApp()` are hidden, and actions added with `addActionToCache()` (kept in `live_action_summaries`,
  which survives a reload like the other cache edits) replace or add to the indexed ones.
- The summaries are part of the index state: `installIndexState()` discards them, and `getConsistent()` retries a
  lookup if the index was replaced while they were read.

### Restoring one action

`tryGetActionMetadata(app, action)` restores only that action (`checkSingleActionMetadata()`): the app's chunk is
read as before, and of its type tables only the descriptions the action refers to are restored; the table object
restores each description once and shares it with the other actions of the app.

- The serialized actions of the app stay in `raw_actioninfomap` until all of them are restored; `action_restored`
  records the actions restored on their own, or that could not be restored (logged once, never retried). A later
  restore of the whole app restores only the others, and returns all of them in index order.
- All restores of one app's actions, of one action or of all of them, are serialized by the app's
  `action_metadata_initializing` marker, since the type table object of a chunk is not thread-safe; different apps
  are restored in parallel.
- An action added to the cache by the process wins over the indexed action of the same name.

The action searches (`searchCached*Action*()`) match the summaries and restore only the matching actions, from one
index state. `removeCachedApp()` no longer restores the app's actions before discarding them.

## Measurements (real index: 251 apps with actions, 6,058 actions)

| operation | time |
|---|---|
| all action summaries, first read in a process | 0.19 s |
| one action's metadata (Xero, 39 actions), first read in a process | 0.10 s |
| the other 38 actions of the app | 0.45 s |
| every action, restored | 4.5 s, about 850 MB RSS |

The first-read times include creating the deserialization context, which loads the DataProvider module.
