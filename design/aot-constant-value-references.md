# AOT Constant Value References

## Status

Implemented. This document records how a value node that appears inside more
than one constant is serialized into an AOT object, and the rule that keeps the
resulting references acyclic.

Relevant code:

- `lib/QoreAOTBinary.cpp` — `qore_aot_add_constant_value_reverse_mappings()` and
  `aot_constant_reverse_path_is_better()` (which path a node is recorded under),
  `QoreAOTBinaryWriter::writeValue()` (`VT_CONST_REF` emission),
  `qore_aot_resolve_constant_path_value()` and `RuntimeConstantPathRefNode`
  (the load side)
- `examples/test/ir/AOTAliasedConstantPathDepth.qtest` — the tests

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

**A node must be recorded under the shallowest path that reaches it.**

A node reachable from two constants is reachable from the one that *holds* the
other only by going through it, so the holder's path is always strictly deeper.
Recording the deeper path inverts the ownership: the owning constant's own
serialized value then references the holder, while the holder references the
owner back. Nothing breaks that cycle at load time, and both of its outcomes are
build-stopping:

| Cycle reached with | Failure |
|---|---|
| the other constant still a pending shell | the value deserializes to a deferred `RuntimeConstantPathRefNode` instead of a value. A later compilation unit that folds that constant into a new one hands the writer a node it has no case for: `AOT cannot serialize class constant '…' without data loss` |
| the other constant already holding a value | `ConstantEntry::getReferencedValue()` and `RuntimeConstantPathRefNode::evalImpl()` call each other with no base case, and `qcc` dies of a stack overflow with no diagnostic at all |

Depth is therefore the primary ordering in
`aot_constant_reverse_path_is_better()`, ahead of the encoded-string length
heuristic that breaks ties between paths of equal depth. Length alone is not a
usable proxy for depth: a container constant with a short enough name encodes
shorter *despite* the extra subscript, which is exactly how the inversion was
reached in practice —
`ObjectOptionsRestClass::OptionInfo["system"]["stack-size"]` encodes 3 bytes
shorter than `ObjectOptionsRestClass::InterfaceSystemOptions["stack-size"]`.

Whole-constant paths still win over encoded paths regardless of depth, since
resolving one needs no subscript walk.

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
