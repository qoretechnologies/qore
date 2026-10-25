# Provider-index type description tables

Copyright 2026 Qore Technologies, s.r.o.

## Problem

An indexed action describes each option type with plain data (`DataTypeInfo`), so that reading
the index never loads the provider modules that define the types. `AbstractDataProviderType::getInfo()`
shares the description of a type that is reachable by more than one path in memory, but a
serialized description is a tree: every path repeats the complete description of every type below
it. A record type whose types refer to each other (an invoice with contacts, credit notes, and
payments that refer back to invoices) is also truncated at a different depth on each path to each
of its cycles, and each truncation is described completely too.

For the apps of a real index, this made the option type descriptions about 20 times larger than
their distinct descriptions (Xero: 39 actions with 72 MB of MessagePack data holding 73,181 type
descriptions, of which 2,933 are distinct). Reading an app's actions unpacked and restored
(`TypedHash` cast) every one of them, so the first read of all the actions of the index took about
17 s and 3 GB, all of it before a caller could select the actions it needed.

## Design

Since metadata cache version 7, each app's chunk stores its option type descriptions in two tables:

- `types`: each distinct type description, whose field descriptions and nested type descriptions
  (`element_type`, `union_types`, `default_field_type_info`) are positions;
- `fields`: each distinct field description, whose `type` is a position in `types`.

A chunk is `{"index": <DataProviderIndexInfo>, "types": list, "fields": list}`, and each option's
`type_info` is a position in `types`. `IndexedTypeTableBuilder` adds nested descriptions before the
descriptions that refer to them, and compares descriptions by their exact MessagePack form, so only
identical descriptions are shared; the unique keys of the descriptions are not changed.

`IndexedTypeTable` reads the tables:

- when actions are restored (`getIndexedAction()`), each raw action refers to its chunk's table
  object with the internal `type_table` key, which is removed when the action is restored. Each
  description is restored once, when it is first needed, and every description that refers to it
  shares the restored hash, so restoring an app takes time and memory in proportion to its
  distinct descriptions. The same `TypedHash` casts validate each description as before;
- the serialized readers (`readSerializedDataProviderIndex()`, `dumpIndexYaml()`, and the
  ProviderIndex worker fragments) get complete descriptions, built once per position and shared,
  so their output is unchanged.

A position that is not in its table, a description that refers to itself, or a position without
tables is a `PROVIDER-INDEX-ERROR`; the action is left out and the error is logged, like any other
action whose metadata cannot be read.

Chunks of versions 4 to 6 hold complete descriptions and are read as before.

## Version 10: one type table per app

A complete description (`getInfo()`) is still a tree of the paths to each type, and for record types
that refer to each other, such as NetSuite's, the number of paths is in the millions: describing,
checking and storing them took gigabytes in the index build, and the restore check of the largest
NetSuite option type about 7 GB (the module-v8 provider qualification was OOM-killed). Since metadata
cache version 10, `ProviderIndex` describes the option types of an app with one
`DataProvider::DataTypeGraphBuilder`: each type with fields and each union type is described once in a
type table under a key, and every reference to it is a description with the key in `type_ref`; types
and fields described in place have an empty `unique_key`, so the keys do not depend on the process.

Forms of an app's actions (`IndexedTypeGraphForm`):

- self-contained, in memory: each option type description that refers to the table gives it in
  `type_graph` (all descriptions of an app share the table value), so that
  `AbstractDataProviderType::get()` restores the type from the description alone, as
  `QorusIndexedAction` restores it; readers and the serialized readers give this form;
- stored, in a chunk and in the canonical YAML: the descriptions refer to one table of the app,
  which is stored once. Its types are those that the actions refer to, keyed `"1"`, `"2"`, ... in
  the order in which the actions, by name, and their options refer to them, so the stored form does
  not depend on the keys that the types were described with. Descriptions of an app that give
  different tables (several index workers) are stored with one table, each table's types once.

A version 10 chunk is `{"index", "types", "fields", "type_graph"}`, where `type_graph` gives the
position in `types` of each type of the app's table; `IndexedTypeTable` restores (or expands) the
table once and gives it to the descriptions that refer to it. The canonical YAML file and
`dumpIndexYaml()` hold the tables in `typegraphmap` (app -> table), and the canonical reader gives
them to the descriptions.

The restore check restores the option types of an app in one restoration (as the fields of one hash
type described with the app's table), describes the restored types with one table of their own, and
compares each description with the description of its restored type: the keys of the two tables are
matched as the descriptions refer to them, one to one, so a type restored twice is a difference, and
each pair is compared once. A type of the table that is not plain data leaves out the actions whose
option types refer to it. Unique keys are only replaced in factory info.

Worker fragments record the digest of each action in the stored form and of the fragment's table;
when two workers describe an action with other digests, the parent reads both fragments and compares
the action's option types independently of the keys of their tables, so that the same action
described with other keys does not conflict, and a difference in a type that it refers to does.

Qorus validates a serialized index value by value with a limit per app (`QorusSystemToolCatalog`);
it walks the table that an app's descriptions share once, and compares it once between two indexes.

Measured (2026-10-25):

- module-v8 provider qualification, CI image of the module-v8 commit that was OOM-killed (cgroup
  `memory.peak`): 20.6 GB and 1,442 s on develop at the time, 6.8 GB and 259 s before the regression,
  5.9 GB and 617 s with complete descriptions and the DataProvider that restores each type once, 5.0 GB
  and 103 s with the type graph form (260 apps, 6,016 actions; index 7.8 MB -> 6.7 MB); the option types
  decoded from the two indexes (24,523 options) restore types with equal type graph descriptions, the
  rest of the actions is equal, and the restore check reports the same 363 options;
- private `qctl update-index --index-workers=4` of all installed apps, both with the DataProvider that
  restores each type once, complete descriptions -> type graph form: build 343 s -> 91 s; the
  TypeScript worker (w1, 107 apps) 2.89 GiB, 309 s -> 1.65 GiB, 28 s; the Xero worker (w46) 1.72 GiB,
  131 s -> 0.78 GiB, 7 s; the merging process 0.52 GiB -> 0.48 GiB; index 7.9 MB -> 6.7 MB; the restore
  check reports the same 918 options in both (causes in the producers' types).

## Remaining cost

After the change, the first read of all the actions of the same index takes about 5.5 s and 630 MB
(before: 17 s and 3.1 GB). Most of the remainder is restoring the 21,150 distinct type descriptions
and 14,494 distinct field descriptions. Two thirds of the distinct type descriptions differ only in
their unique keys, which `ProviderIndex` numbers per option description; numbering them per app
instead was measured and stores more distinct type descriptions, because structurally identical
descriptions of different type objects then get different keys.

Consumers that only list or select actions do not pay this cost: see
[provider-index-action-summaries.md](provider-index-action-summaries.md).
