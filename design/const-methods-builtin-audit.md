# Const Methods Builtin Annotation Audit

Status: v1 audit complete.

This file records the stable builtin const-method annotation subset for
`design/const-methods.md`. It is intentionally scoped to the pseudo-methods
needed for readonly value bindings and const-method bodies to be usable. A broad
audit of every service-style builtin class remains an incremental compatibility
improvement, not a language semantics blocker.

Const-method metadata is separate from `QCF_CONSTANT` and `QCF_RET_VALUE_ONLY`.
The annotations below mean only that the method does not mutate its receiver
through a self-rooted writable path. They do not imply that the method cannot
throw or that it has no external effects.

## Annotated in v1 Subset

The v1 implementation annotates common pseudo-methods used through readonly
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

## Deliberately Unannotated Alias-Risk Methods

These methods remain non-const in v1 after alias review:

| Method group | Decision | Rationale |
| --- | --- | --- |
| `<list>::first()`, `<list>::last()` | `non-const-alias-risk` | Return an element value that can be an object/container alias. Readonly propagation does not follow method-return values. |
| `<list>::iterator()`, `<list>::rangeIterator()` | `non-const-alias-risk` | Return iterator objects tied to receiver contents or iteration state. |
| `<hash>::values()`, `<hash>::firstValue()`, `<hash>::lastValue()` | `non-const-alias-risk` | Return values that can contain object/container aliases from the receiver. |
| `<hash>::iterator()`, `<hash>::keyIterator()`, `<hash>::pairIterator()`, `<hash>::contextIterator()` | `non-const-alias-risk` | Return iterator/view objects over receiver keys or values. |
| `<object>::iterator()`, `<object>::keyIterator()`, `<object>::pairIterator()` | `non-const-alias-risk` | Return iterator objects over object members. |
| `<object>::getCallReference()` | `non-const-alias-risk` | Can expose a callable reference bound to receiver behavior. |
| `<value>::iterator()` | `non-const-alias-risk` | Generic fallback iterator cannot prove detached readonly semantics for all values. |
| `<buffer>::view()` | `non-const-alias-risk` | Returns a writable view over receiver storage. |
| `<buffer>::materialize()` | `non-const-mutates` | May change receiver storage state. |
| `<callref>::exec()` | `non-const-dynamic` | Executes arbitrary code and is not required for readonly receiver usability. |

## Remaining Builtin Scope

This audit covers the v1 pseudo-method subset required for readonly value-typed
bindings to be usable. Other builtin class instance methods tagged
`QCF_CONSTANT` or `QCF_RET_VALUE_ONLY` remain candidates for later const-method
annotation. Those methods must not be marked const until each class-specific
receiver contract is reviewed. This is an incremental API usability task, not a
pending language-design issue.

The current metadata inventory shows the only unannotated pseudo-method
`QCF_CONSTANT` / `QCF_RET_VALUE_ONLY` candidates are the alias-risk rows above.
Parser tests assert that representative alias-risk calls are rejected on
readonly receivers, and reflection tests assert the const/non-const pseudo-method
metadata split.
