# Optional common type folding

Copyright (C) 2026 Qore Technologies, s.r.o.

`QoreTypeInfo::matchCommonType()` retains an existing common type when that type
already contains the next type. Its subset check works in both operand orders
before rejecting incompatible types with multiple return alternatives. Additional
`NOTHING` values leave an already optional common type intact.

For example, combining `*hash<Identity>` with `hash<Identity>` retains
`*hash<Identity>`, whether the optional type occurs first or second. A list
containing an identity, `NOTHING`, and another identity therefore keeps a usable
optional hash element type. It does not degrade to `list<auto>` and then fail a
method taking `list<*hash<Identity>>`.

Retaining the optional type makes a two-`NOTHING` literal behave like a one-`NOTHING`
literal: `{"a": string, "b": NOTHING, "c": NOTHING}` is `hash<string, *string>`, where the
second `NOTHING` used to reach the "multiple return alternatives" path and collapse the whole
literal to `auto`. Code that built a container from string-ish values and then added a hash or
a list under a new key was therefore only ever working by accident; the declaration for a
container that genuinely holds mixed types is `hash<auto!>` / `list<auto!>`, which does not
narrow. A plain `hash<auto>` still narrows to the element type of the value assigned to it.

The generic hash-field map specialization (`MapHashKeyValue`) infers its result's common
element type from the actual values. It collects into an unconstrained list and
publishes the inferred type after the loop, matching the AST map's value-type
tracking. Type comparison is incremental, so the operation remains linear and
requires no second pass. The IR interpreter and JIT/AOT call the same helper;
its partial result is reference-managed, and its loop checks cancellation.
The change does not allow
unrelated hash declarations through typed argument validation, or coerce mixed
values into the first element's type. The common-type comparison itself introduces
no allocation or shared mutable state.

`examples/test/ir/OptionalMapTypeFolding.qtest` runs the companion fixture in AST,
IR and JIT modes and as a source-stripped AOT executable. Cases include optional
hash, integer and list values, empty input, leading/middle/trailing `NOTHING`,
iterator maps, map-select, either operand order and unrelated declarations.
Top-level maps exercise synchronous JIT compilation, heterogeneous results and
cleanup after an invalid hash member following a successfully mapped item.

The native companion enters the map helper with cancellation already pending.
It checks the exact `THREAD-CANCELLED` error after partial construction, clears
cancellation, and verifies a subsequent 205-item map succeeds. Both normal and
checked append paths run, without timing or scheduling dependencies.

## Identical type metadata

The static `QoreTypeInfo::superSetOf(first, second)` shortcut accepts identical
metadata pointers only after both `hasType()` checks. A metadata object has the
same ordered accept/return specifications as itself, so the structural walk is
redundant in that case. Pointer inequality still uses the full structural
comparison; null and `auto` metadata retain their existing asymmetric rules.
This is not a new conversion rule and does not bypass element assignment,
soft-type conversion, hashdecl validation, or ownership checks.

`examples/test/ir/IRTypedContainerChecks.qr`, driven by
`IRPerformanceSemantics.qtest`, compares AST, IR, JIT, tiered, and stripped-source
AOT behavior for typed and optional containers, soft conversions, rejected
assignments, boxed full-width integers, aliases, nested containers, and untyped
fallbacks. The wrapper also repeats the fixture with fixed-list scalarization
and fixed-hash construction specialization disabled.
