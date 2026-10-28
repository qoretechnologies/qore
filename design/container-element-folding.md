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
| runtime variant selection | `QoreTypeInfo::runtimeAcceptsValueOrFold()` (`QoreTypeSpec::runtimeAcceptsValue()` with `fold`, cases `QTS_COMPLEXHASH`/`QTS_COMPLEXLIST`), through `QoreTypeInfo::runtimeAcceptsCallArg()` | a foldable value is `QTI_AMBIGUOUS` |
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

The fold verdict answers "would this value be accepted", not "does this value have this type".
`QoreTypeInfo::runtimeAcceptsValue()` answers the second question and does not report a fold: a
`list<list<string>>` is not an instance of `list<list<int>>`, which is what the parser concludes for
`instanceof` too, and the JIT fast paths that skip `acceptInput()` for a value that already has a
slot's type must not take a value that still has to be folded for one.  When one function answered
both, `instanceof` returned `True` at runtime where the parser warned that it always returns
`False`.  The fold loops, runtime variant selection, a soft list that wraps a single container, and
`qore_type_is_assignable_from()` (`Type::isAssignableFrom()` in the reflection module, and plugin
operator matching) ask `runtimeAcceptsValueOrFold()`.

A fold is reported as `QTI_AMBIGUOUS`, which is weaker than any match that needs no conversion, so
a variant that accepts an argument as it stands still outranks one that has to fold it and existing
calls keep the variant they had. Among candidates that can only be reached by folding, the ordinary
scoring rules decide, as they do for any other ambiguous match.

What is *not* a container fold is unchanged, and is refused by parse time and runtime alike: a
scalar element type (`list<int>` from `list<string>`), a hashdecl source for a different hashdecl,
and a plain hash assigned directly to a hashdecl lvalue (`hash<Leaf> l = some_plain_hash;`), which
is an lvalue assignment rather than the conversion of a container's elements.

## Untyped container arguments

A list or hash built at runtime without a declared element type -- `list<auto>`, `hash<auto>`, a
plain `list` or `hash`, and every container a language binding builds from a Python, Java or
JavaScript value -- has no element type for the eligibility rule above to examine, so
`runtimeAcceptsValue()` matches it against a typed container type only when it is empty (issue
#2647).  The fold accepts such a value whenever its elements fit, so runtime variant selection would
refuse an argument that binding converts (issue #5541).

`QoreTypeInfo::runtimeAcceptsCallArg()` matches call arguments for `runtimeFindVariant()` instead.
For a non-empty untyped container (not a hashdecl hash) and a typed list, softlist or hash
parameter type, it checks every element against the element type, recursing into elements that are
themselves untyped containers, and stops at the first element that does not match:

| Elements | Result |
|---|---|
| every element matches | `QTI_AMBIGUOUS`; binding converts the argument as an assignment does |
| an element does not match | `QTI_NOT_EQUAL`, so another variant can be selected |

`runtimeAcceptsValue()` is not where the elements are checked: a first fix of issue #5541
(577cc3cec6) matched the elements there, which made a `list<auto>` of integers an instance of
`list<int>` and gave the AOT method dispatch cache, keyed by argument types, a selection that depends
on the elements; the call matching below replaced it.

`QTI_AMBIGUOUS` ranks the conversion below a variant that takes the value unchanged (`list`,
`list<auto>`, `softlist<auto>`, `hash<auto>`) and above `auto` and `any`, as for a scalar that needs
a conversion.  `runtimeAcceptsValue()` does not accept such a value: `instanceof` and the JIT fast
paths that skip `acceptInput()` for a value that already has the slot's type ask it whether a value
has a type, and a `list<auto>` does not have the type `list<int>`.

Variants with equal scores are ranked by match quality before the number of parameters: the worst
element match of an element-checked container, `QTI_AMBIGUOUS` for a typed container whose element
type is not the parameter's element type (the type match scores a `list<int>` the same for
`list<float>` as for `list<int>`), otherwise the result itself, so `list<int>` is
called rather than `list<number>` for a list of integers in either order of declaration, while all
other ties are decided as before.  The softlist conversion entry, which accepts any list, does not
decide for an untyped list: a `softlist<string>` variant is not selected for a list of integers.

A typed container is matched by its type, except where that verdict depends on its elements, which
are then checked the same way before the type's result is returned:

| Typed source | Why the type does not decide |
|---|---|
| a nested container accepted by the fold rule (`list<list<string>>` for `list<list<softint>>`) | the fold accepts it by its shape |
| a nested element type that `parseAccepts()` accepts with `may_not_match` (`list<list<auto>>` for `list<list<int>>`, an exact match by type) | the elements may not match |
| any list for a `softlist<T>` type, through its conversion entry | the conversion accepts any list |

A container whose element type is the parameter's element type is not checked, so exact typed
matches pay nothing.  The result and quality stay those of the type, so a call that binding accepts
selects the variant it selected before; only a variant whose binding would fail is no longer
selected.  The check sets `content_dependent` too.

A hash element is accepted for a hashdecl element type without checking its keys, as
`retypeValue()` converts it and reports a key the hashdecl does not declare; a hash argument is not
converted to a hashdecl, as lvalue assignment does not convert it.  Empty untyped containers keep
the #2647 rule and are not checked.

The result now depends on a value's elements, not only on its type, so `runtimeFindVariant()`
reports through its `content_dependent` argument whether any element check ran, and the AOT
polymorphic method dispatch cache in `lib/JITRuntime.cpp`, which is keyed by argument types, does
not cache such a selection: `evalMethodTmpArgs()` returns no `resolved_variant` for it.

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

`examples/test/qore/functions/untyped-container-args/` covers untyped container arguments in every
execution mode and in a program compiled with qcc: dynamic calls, call references, methods, static
methods and constructors, nested containers, weak references, hashdecls, ranking and ties, softlist
selection, and call sites that get untyped lists of different element types in turn.
`examples/test/qore/functions/typed-container-args/` covers typed sources: calls that bind keep their
variant (exact matches, `list` and `auto` overloads, softlists, folded nested containers, hashdecl
elements, ties), and calls whose binding would fail select another variant or raise
`RUNTIME-OVERLOAD-ERROR`.
`examples/test/qore/operators/instanceof-containers/` covers `instanceof` for nested container types
that are folded but not accepted, together with the assignments and calls that still fold them.

`testContainerFoldingParseRuntimeAgreement()` covers the agreement above: resolved literals in
variable declarations and assignments, function and method arguments, return values and member
initializers, each paired with the same value routed through an `auto` expression so both paths are
compared directly; nested and recursive literals; empty containers; overload resolution (an exact
variant outranks a folded one, and a folded variant is still selected when it is the only
candidate); a bound source left unretyped and unchanged; an invalid field value and an unknown key
reported at runtime against the key; a soft list, which folds the elements of a list it is given but
does not fold a single value wrapped into one; and the parse-time negatives that must stay rejected.
