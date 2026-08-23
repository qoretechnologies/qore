# AOT Constant Value References

## Status

Implemented. This document records how a value node that appears inside more
than one constant is serialized into an AOT object, and the rule that keeps the
resulting references acyclic.

Relevant code:

- `lib/QoreAOTBinary.cpp` — `aot_add_constant_value_reverse_mappings_impl()`
  (which constant a node is recorded under) and
  `aot_constant_reverse_path_is_better()` (the object exception),
  `QoreAOTBinaryWriter::writeValue()` (`VT_CONST_REF` emission),
  `qore_aot_resolve_constant_path_value()` and `RuntimeConstantPathRefNode`
  (the load side)
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

**A container keeps the path of whichever constant reached it first; a later
constant may not re-claim it.**

Constants are traversed when the reverse map is built, and every constant a given
pair shares is reached by both of them, so refusing the re-claim makes one
constant win *all* of them. References between any two constants then only ever
run one way, and the graph cannot contain a cycle.

Nothing breaks a cycle at load time, and both of its outcomes are build-stopping:

| Cycle reached with | Failure |
|---|---|
| the other constant still a pending shell | the value deserializes to a deferred `RuntimeConstantPathRefNode` instead of a value. A later compilation unit that folds that constant into a new one hands the writer a node it has no case for: `AOT cannot serialize class constant '…' without data loss` |
| the other constant already holding a value | `ConstantEntry::getReferencedValue()` and `RuntimeConstantPathRefNode::evalImpl()` call each other with no base case, and `qcc` dies of a stack overflow with no diagnostic at all |

### Why ranking the candidate paths does not work

Every ranking of the *paths* is a per-node decision, and a cycle needs only two
nodes ranked in opposite directions. Two failed rankings, in order:

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

Claim order is not a ranking of paths at all, which is why it does not have this
failure mode.

### Objects are the exception

An object can never be written by value, so a constant holding one has nothing to
fall back on if the recorded path names the constant currently being written —
the writer refuses a self-reference, and serialization fails. Claim order does
not reliably name an object's own constant, because the traversal walks classes
and namespaces in tree order rather than declaration order: `SoftTypeMap` in
`DataProvider` is reached before the `SoftIntType` constant holding the object it
stores. Objects therefore keep the best-path choice, which prefers the
whole-constant path every other holder can reference.

That leaves object re-claims able to form a cycle in principle, which is what the
load-side guard below is for.

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
