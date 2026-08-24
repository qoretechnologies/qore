# AOT Constant Value References

## Status

Implemented. This document records how a value node that appears inside more
than one constant is serialized into an AOT object, and the rule that keeps the
resulting references acyclic.

Relevant code:

- `lib/QoreAOTBinary.cpp` — `aot_add_constant_value_reverse_mappings_impl()`
  (which constant a node is recorded under),
  `QoreAOTBinaryWriter::writeValue()` (`VT_CONST_REF` emission),
  `qore_aot_resolve_constant_path_value()` and `RuntimeConstantPathRefNode`
  (the load side)
- `include/qore/intern/ConstantList.h` — `ConstantEntry::getInitSeq()` and
  `~ConstantEntryInitHelper()` (the rule and where the sequence is stamped)
- `lib/ConstantList.cpp` — `ConstantEntry::getReferencedValue()` and
  `qore_constant_deep_resolve_in_flight()` (the load-side cycle guard)
- `examples/test/ir/AOTAliasedConstantPathOwnership.qtest` — the tests

## Why constants reference each other at all

The parser folds a constant used inside another constant's initializer into
that initializer's value, so both constants end up holding the *same* node:

```qore
const InterfaceSystemOptions = {"stack-size": {"type": "byte-size"}};
const OptionInfo = {"system": InterfaceSystemOptions};
```

Some of those shared nodes cannot be written as values at all — an object or a
closure inside a folded hash literal has no serialized form. The writer
therefore keeps a program-wide reverse map from node pointer to the constant
path the node can be recovered from, and emits a `VT_CONST_REF` naming that
path instead of a value. The load side resolves the path back to the live node,
so the sharing survives the round trip as well.

A path is either a plain constant FQN or an *encoded* path: an FQN plus a chain
of subscripts, `H<len>:<key>` for a hash key and `L<index>:` for a list index.
The number of subscripts is the path's **depth**; a plain FQN has depth 0.

## The rule

**A node is recorded under the constant with the lowest initialization sequence
that reaches it** (`ConstantEntry::getInitSeq()`, stamped when the entry's value
is finished — see `~ConstantEntryInitHelper()`).

Initializing a constant resolves its initializer, which initializes every
constant that initializer reads, so completion order is a topological order of
the "holds another constant's value" relation: the constant that *defines* a
shared value is always finished before every constant aliasing or containing it.
Two properties follow, and the rule is the only one tried so far that gives
both.

**Acyclic.** The sequence is a single total order over constants, so every node
a given pair shares is won by the same one of them; references between any two
constants run one way only.

**Resolvable across compilation units.** A reference always names a constant
declared before the one holding it, which is a constant that any unit able to
see the holder has already loaded. This is the property no ranking of *paths*
can express, because a path's shape says nothing about which source unit is
upstream — see below.

It has to be *completion* order and not declaration order. Constant initializers
are resolved lazily, so an entry is created when its declaration is parsed —
which can be long before the constant its initializer reads exists. Qorus
declares

```qore
class MapperFieldCodeTypeHelper {
    const JavaTypeMap = OMQ::MapperProgram::JavaTypeMap;   // Classes/…, source 160
}
```

115 sources ahead of the `lib/qorus.ql` that declares `MapperProgram::JavaTypeMap`
(source 275) in the same batch, so by creation order the alias comes first and
takes the value — the very failure this rule exists to prevent. Completion order
cannot be fooled that way: the alias cannot finish until the definer has.

Constants a compile inherits from its preload are stamped as they load, which
need not match how they were declared, but they are harmless: the preload is the
unit's transitive predecessor set, so a reference into it resolves for every
consumer of the unit as well.

Nothing breaks a cycle at load time, and both of its outcomes are build-stopping:

| Cycle reached with | Failure |
|---|---|
| the other constant still a pending shell | the value deserializes to a deferred `RuntimeConstantPathRefNode` instead of a value. A later compilation unit that folds that constant into a new one hands the writer a node it has no case for: `AOT cannot serialize class constant '…' without data loss` |
| the other constant already holding a value | `ConstantEntry::getReferencedValue()` and `RuntimeConstantPathRefNode::evalImpl()` call each other with no base case, and `qcc` dies of a stack overflow with no diagnostic at all |

### Why ranking the candidate paths does not work

Every ranking of the *paths* is a per-node decision, and a cycle needs only two
nodes ranked in opposite directions. Three failed rules, in order:

- **encoded-string length.** A container constant with a short enough name
  encodes shorter *despite* the extra subscript:
  `ObjectOptionsRestClass::OptionInfo["system"]["stack-size"]` encodes 3 bytes
  shorter than `ObjectOptionsRestClass::InterfaceSystemOptions["stack-size"]`,
  so the container won its own holder's interior. That was the reported Qorus
  build failure.
- **path depth.** Containment shifts every node of the held constant down by the
  same amount, so preferring the shallowest path does make the owner win —
  *unless* the two constants are related in more than one way at once. A
  constant that both merges and embeds the same target shares that target's
  interior nodes at **equal** depth (a merge shifts nothing) and its root node
  one level down, so depth hands the interiors to the tie-break and the root to
  the owner. Each constant then wins one, and they reference each other. That
  shape crashed `qcc` outright.

- **claim order.** Keeping the node under whichever constant the traversal
  *reached* first is not a ranking of paths, so it has neither failure above. But
  the traversal walks namespaces and classes in tree order, which has no relation
  to which source unit is upstream. `MapperFieldCodeTypeHelper` sorts before
  `MapperProgram`, so the alias

  ```qore
  const JavaTypeMap = OMQ::MapperProgram::JavaTypeMap;
  ```

  claimed the value its own definer declares, and `MapperProgram`'s object was
  written referencing a constant declared in a unit that *depends* on it. A
  consumer is preloaded with a unit's predecessors, and the alias is not one, so
  nothing could resolve that reference: the Qorus incremental build stopped with
  `sibling .qo cross-resolution failed: … cannot resolve const_ref`.

Initialization order keeps claim order's freedom from path-shape cycles and adds
the direction claim order had no way to see.

### Objects need no exception

An object can never be written by value, so a constant holding one has nothing to
fall back on if the recorded path names the constant currently being written —
the writer refuses a self-reference, and serialization fails. Objects therefore
have to be recorded under the constant that *declares* the object, not under one
that merely stores it: `SoftTypeMap` in `DataProvider` holds the object the
`SoftIntType` constant declares.

Initialization order gives that for free — `SoftIntType` is finished before the
map that stores it can be — so objects follow the same rule as every other node
and need no exception. Under claim order they did need one, because tree order reaches
`SoftTypeMap` first, and the best-path fallback that provided it was itself able
to form a cycle in principle.

## The load-side guard

`ConstantEntry::getReferencedValue()` records the entry it is materializing in a
per-thread in-flight stack (`qore_constant_deep_resolve_in_flight()`), and
`RuntimeConstantPathRefNode::resolvePath()` refuses to resolve a reference back
into an entry already on it, raising `RECURSIVE-CONSTANT-REFERENCE` instead. The
state is per-thread because a cycle is always contained in one thread's call
stack, which also keeps concurrent reads of the same constant race-free.

This mirrors the `rt_in_init` guard that `RuntimeConstantRefNode::evalImpl()`
already applies to runtime constant-value cycles. It is a backstop, not the fix:
a cycle should never be written in the first place, and reaching this means the
artifact is defective. It exists so that a defect of this family is a reported
error naming both ends rather than a stack overflow with no diagnostic.

## Why the failure needs more than one compilation unit

The writer never emits a reference into the constant it is currently writing
(`aot_constant_path_belongs_to_fqn()` against `current_const_path`), so a
single-unit compile folds the shared node in by value and looks correct. The
inverted reference is only written when the two constants are serialized as
peers, and it is only *read* when a separate unit compiled against that object
folds the constant into a new one — the incremental `qcc -c -L <preload>` model.
Tests for this shape must therefore compile two objects, not one.

## Reporting

`QoreAOTBinaryWriter::writeValue()` records the position and type of the
innermost value it could not write (`current_value_path`,
`value_failure_detail`), and the constant writers append it to the error. A hash
of plain strings and a hash holding a deferred reference are otherwise
indistinguishable in the message, which is most of the diagnostic work when this
does go wrong.
