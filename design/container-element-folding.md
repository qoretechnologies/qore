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

What is converted is a container's *elements*. Assigning a plain hash directly to a hashdecl
lvalue is not a fold and deliberately does no conversion: `hash<Leaf> l = some_plain_hash;` is
refused. The fold path is more permissive on purpose, because a container being assigned to a
declared type carries the declaration that says what its elements are meant to be.

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

## One verdict, reached in one place

The fold runs for *every* container assignment -- a variable, a function argument, a return value, a
member initializer -- so the checks that run before it must not reject a value it would go on to
accept. Three places ask the question, and all three answer it with the same eligibility rule:

| Check | Where | How |
|---|---|---|
| parse-time type match | `match_container_element_type()`, `lib/QoreTypeInfo.cpp` | a foldable element pair is `QTI_AMBIGUOUS` with `may_need_filter` and `may_not_match` set, not `QTI_NOT_EQUAL` |
| runtime variant selection | `QoreTypeSpec::runtimeAcceptsValue()`, cases `QTS_COMPLEXHASH`/`QTS_COMPLEXLIST` | a foldable value is `QTI_AMBIGUOUS` |
| the conversion itself | `acceptInputComplexHash()`/`acceptInputComplexList()` | `qore_container_value_may_convert_to()` |

`match_container_element_type()` wraps `match_type()`, the element matcher every complex container
parse check funnels through, so reporting the fold there covers assignment, argument binding, return
values and member initializers at once; there is no per-site list to keep in step. The match handlers
in `lib/QoreTypeSpecMatchHandlers.cpp` call it exactly where the source is itself a container whose
elements the loops convert one at a time -- complex hash from complex hash, complex list from complex
or soft list.

The distinction matters for a soft list, which has a second conversion: it wraps a single value into
a one-element list. That value is matched against the element type through `match_type()` directly,
because nothing folds it, and `softlist<hash<Leaf>> s = {"value": "x"};` is refused by parse time and
assignment alike -- as a plain hash assigned to a hashdecl always is. Asking the fold question there
would accept at parse time what assignment then refuses, which is the very divergence this is meant
to remove.

Where these disagreed, acceptance turned on whether the value's type happened to be known
statically. The identical literal was a parse error where it was written out and folded silently
where it arrived through an `auto` expression:

```qore
list<hash<Leaf>> parts = ({"value": "x", "format": "f"},);   # was PARSE-TYPE-ERROR
auto v = ({"value": "x", "format": "f"},);
list<hash<Leaf>> parts = v;                                  # ... folded
```

and an argument the parser accepted was then refused by `runtimeFindVariant()` with
`RUNTIME-OVERLOAD-ERROR` before binding could convert it.

A fold is reported as `QTI_AMBIGUOUS`, which is weaker than any match that needs no conversion, so
a variant that accepts an argument as it stands still outranks one that has to fold it and existing
calls keep the variant they had. Among candidates that can only be reached by folding, the ordinary
scoring rules decide, as they do for any other ambiguous match.

What is *not* a container fold is unchanged, and is refused by parse time and runtime alike: a
scalar element type (`list<int>` from `list<string>`), a hashdecl source for a different hashdecl,
and a plain hash assigned directly to a hashdecl lvalue (`hash<Leaf> l = some_plain_hash;`), which
is an lvalue assignment rather than the conversion of a container's elements.

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

`testContainerFoldingParseRuntimeAgreement()` covers the agreement above: resolved literals in
variable declarations and assignments, function and method arguments, return values and member
initializers, each paired with the same value routed through an `auto` expression so both paths are
compared directly; nested and recursive literals; empty containers; overload resolution (an exact
variant outranks a folded one, and a folded variant is still selected when it is the only
candidate); a bound source left unretyped and unchanged; an invalid field value and an unknown key
reported at runtime against the key; a soft list, which folds the elements of a list it is given but
does not fold a single value wrapped into one; and the parse-time negatives that must stay rejected.
