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

Chunks of versions 4 to 6 hold complete descriptions and are read as before. The canonical YAML
file always holds complete descriptions.

## Remaining cost

After the change, the first read of all the actions of the same index takes about 5.5 s and 630 MB
(before: 17 s and 3.1 GB). Most of the remainder is restoring the 21,150 distinct type descriptions
and 14,494 distinct field descriptions. Two thirds of the distinct type descriptions differ only in
their unique keys, which `ProviderIndex` numbers per option description; numbering them per app
instead was measured and stores more distinct type descriptions, because structurally identical
descriptions of different type objects then get different keys.
