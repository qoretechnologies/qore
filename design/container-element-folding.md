# Container element folding

Copyright (C) 2026 Qore Technologies, s.r.o.

When a container value is assigned to a declared container type whose element type it does not
already satisfy, `QoreTypeSpec::acceptInputComplexHash()` and
`QoreTypeSpec::acceptInputComplexList()` in `lib/QoreTypeInfo.cpp` convert it one element at a
time rather than rejecting it. This is what lets generically-typed data -- a hash literal, a
parsed JSON document, the result of a `map` expression -- be adopted into a declared structure:

```qore
hashdecl Leaf {
    auto value;
    *string format;
}
hashdecl Parent {
    list<hash<Leaf>> parts;
}

# the plain child hash is converted into a hash<Leaf> as the list is folded
hash<Parent> p = new hash<Parent>(parse_json('{"parts": [{"value": "a", "format": "f"}]}'));
```

Plain lvalue assignment deliberately does no such conversion; `hash<Leaf> l = some_plain_hash;`
raises `RUNTIME-TYPE-ERROR`. The fold path is more permissive on purpose, because a container
being assigned to a declared type carries the declaration that says what its elements are meant
to be.

## Eligibility

`qore_container_value_may_convert_to()` decides whether a source element type that the target
element type does not already accept may still be converted. Eligibility is a question of
*shape*, not of value type:

| Source element | Target element | Eligible |
|---|---|---|
| complex hash (any value type) | accepts `NT_HASH` | yes |
| complex list (any value type) | accepts `NT_LIST` | yes |
| hashdecl | anything | no |
| scalar | anything | no |

The source's value type is intentionally not consulted. It is the per-element conversion in the
callers -- `QoreTypeInfo::retypeValue()` followed by `acceptInputIntern()` -- that decides
whether the values actually fit, so an incompatible field still fails, with an error naming the
offending key, and an unknown field still raises `HASHDECL-INIT-ERROR` exactly as direct
construction does.

Hashdecl sources are excluded because reinterpreting one declared structure as another is not
the adoption of generic data, and lvalue assignment rejects it too. They remain governed by
`QoreTypeInfo::parseAccepts()` and the descendant/parameterized rules.
`QoreTypeInfo::getComplexHashValueType()` returns `nullptr` for `QTS_HASHDECL`, so they do not
reach the hash branch. Scalar element types are excluded by the base-type check, so widening the
gate cannot make a `list<int>` accept a `list<string>`.

## The parse-time check

`typed_hash_decl_private::parseCheckHashDeclAssignment()` in `lib/TypedHashDecl.cpp` performs the
same check for a hashdecl initializer whose value is known at parse time, in two branches: one for
an already-evaluated hash (`NT_HASH`) and one for an unevaluated parse node (`NT_PARSE_HASH`).
Both consult `QoreTypeInfo::mayFoldContainerValueTo()` and, where the fold could convert the value,
set `runtime_check` and continue rather than raising a parse error -- the same deferral the check
already makes for a narrowed `auto` variable.

Without this the two halves disagreed, and which one a given initializer met depended on whether
its literal had been constant-folded, so the same expression was accepted in one file and rejected
in another. An initializer that cannot fold -- a `list<int>` field given a `list<string>` -- is
still a parse error, because `mayFoldContainerValueTo()` rejects scalar element types.

One diagnostic moves from parse time to runtime: an unknown key in a *nested* child hash. The
parse-time checker does not descend into nested element types, so that key was only ever reported
early as a side effect of the container type mismatch, and only for sources that happened to
narrow. It is now reported uniformly at runtime as `HASHDECL-INIT-ERROR`, naming the key.

## Why shape and not value type

Requiring the source value type to be exactly `auto`/`auto!` made acceptance depend on an
accident of inference. A literal whose values happen to share a type narrows to that type -- see
`design/optional-common-type-folding.md` for why that narrowing is deliberate -- so

```qore
{"value": 1, "format": "a"}      # infers hash<auto>           -> folded
{"value": "x", "format": "a"}    # infers hash<string, string> -> rejected
```

were treated differently although both are valid initializers for every field of `Leaf`. The
second was rejected by the container gate before any field was examined, with an error naming
the container type rather than anything the caller could act on. Nested typed structures built
from ordinary literals -- `*list<hash<WsdlMimeEntityInfo>>` in module-xml's MIME entity API, for
instance -- failed whenever their leaf values were all strings.

## Source values are not mutated

A fold never alters the value it reads from. Both callers copy a container that is not unique
before retyping it, and `retypeValue()` builds a new hash for a hashdecl target and only
installs it on success. A source that fails to fold mid-container is therefore left exactly as
it was, whether or not it was shared.

## Coverage

`examples/test/qore/misc/hashdecl.qtest::testHomogeneousContainerHashDeclFolding()` covers
homogeneous and heterogeneous literals, list and hash containers, optional and nonoptional
fields, recursive element types, empty containers, invalid field values, unknown fields, scalar
element types, shared and unshared sources, JSON-derived input, fully resolved initializers on the
parse-time path, and the two parse-time negative cases (a scalar element type, and a hashdecl
source for a different hashdecl).
`testGenericContainerHashDeclFolding()` covers the `hash<auto>`/`list<auto>` cases.
