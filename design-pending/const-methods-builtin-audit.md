# Const Methods Builtin Annotation Audit

This file records the initial builtin const-method annotation subset for
`design/const-methods.md`.

Const-method metadata is separate from `QCF_CONSTANT` and `QCF_RET_VALUE_ONLY`.
The annotations below mean only that the method does not mutate its receiver
through a self-rooted writable path. They do not imply that the method cannot
throw or that it has no external effects.

## Annotated in v1 Subset

The initial implementation annotates common pseudo-methods used through readonly
value bindings:

- `<value>`: type and scalar inspectors/conversions, excluding `iterator()`.
- `<string>` and `<binary>`: read-only inspectors, conversions, search,
  encoding, hashing, splitting, substring, regex, and comparison helpers.
- `<bool>`, `<char>`, `<int>`, `<float>`, `<number>`, `<date>`, `<nothing>`,
  `<closure>`, and `<callref>`: read-only inspectors and conversions; callref
  execution remains non-const.
- `<list>`: `typeCode()`, `complexType()`, `size()`, `empty()`, `val()`,
  `join()`, `lsize()`, `contains()`, and `sizep()`.
- `<hash>`: key and size inspectors: `typeCode()`, `complexType()`, `keys()`,
  `firstKey()`, `lastKey()`, `hasKey()`, `hasKeyValue()`, `empty()`, `size()`,
  `val()`, `compareKeys()`, and `sizep()`.
- `<object>`: public-member key and object inspectors: `typeCode()`,
  `complexType()`, `keys()`, `firstKey()`, `lastKey()`, `empty()`, `size()`,
  `className()`, `isSystem()`, `val()`, callable-method inspectors, `sizep()`,
  and `uniqueHash()`.
- `<buffer>`: read-only inspectors, copy/snapshot methods, aggregate methods,
  and the boxed-copy iterator; `materialize()` and `view()` remain non-const.
- The hand-registered fallback pseudo `typeCode()` method in
  `lib/QorePseudoMethods.cpp`.

## Deliberately Unannotated Pending Alias Review

These methods are left non-const until the returned value/reference behavior is
audited in detail:

- `<list>::first()`, `<list>::last()`, `<list>::iterator()`, and
  `<list>::rangeIterator()`.
- `<hash>::values()`, `<hash>::firstValue()`, `<hash>::lastValue()`,
  `<hash>::iterator()`, `<hash>::keyIterator()`, `<hash>::pairIterator()`, and
  `<hash>::contextIterator()`.
- `<object>::iterator()`, `<object>::keyIterator()`,
  `<object>::pairIterator()`, and `<object>::getCallReference()`.
- `<value>::iterator()`.
- `<buffer>::view()` because it returns a writable view over receiver storage.
- `<buffer>::materialize()` because it can change receiver storage state.
- `<callref>::exec()` because it executes arbitrary code and is not needed for
  readonly receiver usability.

## Remaining Builtin Scope

This audit covers the v1 pseudo-method subset required for readonly value-typed
bindings to be usable. Other builtin class instance methods tagged
`QCF_CONSTANT` or `QCF_RET_VALUE_ONLY` remain candidates for later const-method
annotation. Those methods are not marked const until each class-specific
receiver contract is reviewed.

## Unified Execution Plan

Execute this audit together with readonly-binding hardening using the unified
ordering in [`const-followup-unified-plan.md`](const-followup-unified-plan.md).
Builtin candidate inventory may start immediately, but annotation changes must
wait until the readonly lvalue/reference and IR/AOT verification foundation is
complete.
