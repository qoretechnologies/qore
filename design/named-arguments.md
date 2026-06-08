# Qore Named Arguments — Design

## Status: **Implemented** (Qore 3.0.0, branch `feature/5164_jit`)

Call-site named arguments are implemented and shipped in Qore 3.0.0. Callers
may bind arguments by parameter name:

```qore
connect(host: "example.com", timeout: 30s)
```

against a signature such as:

```qore
sub connect(string host, int port = 443, *timeout timeout = NOTHING)
```

The feature is "named arguments", **not** "call by name": names are bound to
positional parameter slots at parse time, and the call still executes through
Qore's single positional `QoreListNode*` calling convention. No new runtime
call ABI, no new IR opcode, and no AOT wire-format change were introduced
(the v1 scope; see [Deferred work (v2)](#deferred-work-v2)).

Core landing commits (anchors below are starting points and will drift):

- `dc152ec85` — parse node, grammar, binder, source-order IR lowering.
- `f3f9e9eee` — builtin gating via `QCF_NAMED_ARGS`, reflection surface.
- `e114166c7` — astparser / tree-sitter mirror.
- `2b9d25ca6`, `eb9683489` — language docs and API-metadata exposure.
- A long series of `feat: enable named ...` commits through `d05c0eb97`
  rolled out the audited builtin opt-in allowlist.

The builtin opt-in process is operational, not part of this design; it lives
in [`design/named-argument-builtin-review-checklist.md`](named-argument-builtin-review-checklist.md).

User-facing documentation: `@ref function_named_arguments` (functions),
`@ref overloading_named_arguments` (overloading), `@ref NAMED_ARGS` (code
flag), and the `qore_3_0_0` release notes.

Tests:

- `examples/test/qore/vars/named-arguments.qtest` (~2,044 lines): dispatch,
  defaults, source-order evaluation, overload selection, references,
  varargs interaction, parse failures, and the reviewed builtin allowlist.
- `examples/test/qore/misc/reflection.qtest`: `isNamedCallable()` /
  `getNamedParameterNames()` over user and builtin variants, including
  negative cases for varargs-only / non-opted-in variants.

## Summary

Named-argument syntax binds supplied arguments to the selected signature's
positional slots before the target executes. Binding is limited to call
targets whose signature and variant are resolved at parse time.

```qore
connect(host: "example.com", timeout: 30s)
```

lowers, after variant selection and source-order argument evaluation, to the
same positional list the standard call path already uses:

```qore
connect("example.com", NOTHING /* default port */, 30s)
```

This reuses the existing runtime ABI, the C++ module interface, and the
existing default-argument and type-filtering machinery. The implementation
extends the existing parse-list node rather than introducing a second call
object.

## Surface Syntax

`name: expression` inside call argument lists:

```qore
sub connect(string host, int port = 443, *timeout timeout = NOTHING, bool tls = True) {
    ...
}

connect(host: "example.com");
connect(host: "example.com", tls: False);
connect("example.com", timeout: 30s);   // positional then named
```

The syntax is visually consistent with Qore hash syntax but is parsed only
in call-argument context; it is not a generic expression form.

### `f(a: 1)` vs `f((a: 1))`

Because Qore allows paren-form hash literals (`(a: 1)` is a one-member hash),
the extra parens change meaning at a call site:

- `f(a: 1)` — **named call** binding parameter `a` to `1`.
- `f((a: 1))` — **positional call** with one argument: the hash `{a: 1}`.

Code that passes a paren-form option hash as `f((opts...))` and is edited to
drop the inner parens silently changes meaning. This is called out in the
language documentation.

## Grammar

The grammar uses a call-argument-specific production rather than the generic
expression list, so `name: expr` is enabled only at call-arg slot starts and
does not perturb the parse of existing code (ternary, switch labels, hash
literals, map-operator key shorthand).

- `lib/parser.ypp` (~3043–3078): `named_call_args` and `named_arg_list`
  productions. `named_call_args` allows leading positional `exp`s followed by
  a `named_arg_list`; `named_arg_list` is `IDENTIFIER ':' exp` repeated, with
  a trailing-comma case and an explicit positional-after-named error case.
- `lib/parser.ypp` (~1765–1780): helpers `add_named_call_arg()` (calls
  `QoreParseListNode::addNamed()`, raises `NAMED-ARG-DUPLICATE` on a repeated
  name in the same call) and `append_call_args()` (merges positional + named
  lists via `appendFrom()`).
- The same shape is used by function calls, method calls, static method
  calls, constructors, base-constructor calls, and call-reference calls.

Mirrored in the astparser tree-sitter grammar so IDE tooling and `qdx`
agree with the core parser:

- `modules/astparser/grammars/tree-sitter-qore/grammar.js`: a
  `named_argument` rule (`field('name', identifier) ':' field('value', expr)`)
  used inside call expressions.

## Parse-Node Representation

The implementation **extends the existing `QoreParseListNode`** rather than
introducing a separate `QoreCallArgsNode`. This avoids touching every
positional walker (IR lowering, verifier, AOT handlers, constructor
lowering, `VarRefNewObjectNode`); those paths continue to see a positional
parse list.

`include/qore/intern/QoreParseListNode.h`:

- `typedef std::deque<std::string> arg_name_vec_t;` — names stored parallel
  to the existing value entries (`arg_names`), empty string for positional
  slots.
- `bool named_args` — set when any named entry is present.
- `addNamed(const char* name, QoreValue v, const QoreProgramLocation* loc)`
  — append a named entry and set `named_args`.
- `hasNamedArgs()`, `hasArgName(i)`, `getArgName(i)`,
  `getArgNamesVector()` — accessors used by the binder and IR lowering.
- `appendFrom(QoreParseListNode*)` — merge positional + named lists,
  propagating the `named_args` flag.
- `QoreParseListNode::initArgs()` (`lib/QoreParseListNode.cpp`) carries the
  names through to the runtime list so the binder can run during variant
  resolution.

## Semantics

1. Positional arguments may appear first; named arguments may follow.
2. A positional argument after a named argument is an error
   (`NAMED-ARG-POSITIONAL-AFTER-NAMED`).
3. A duplicate named argument is an error (`NAMED-ARG-DUPLICATE`).
4. A named argument must match a declared parameter name in the selected
   variant; an unknown name makes the candidate ineligible and, if no
   candidate remains, raises `NAMED-ARG-UNKNOWN` with the accessible
   parameter names listed.
5. A named argument that targets a slot already filled positionally is an
   error (`NAMED-ARG-OVERWRITES-POSITIONAL`).
6. Named arguments do not bind varargs / `argv`.
7. Omitted parameters are defaultable missing slots: they are filled with
   `NOTHING`, and existing default-argument processing replaces `NOTHING`
   with the declared default.
8. Parameter names are case-sensitive (normal Qore identifiers).
9. A named call to a target with no parse-time-resolved signature is rejected
   (`NAMED-CALL-NOT-SUPPORTED`).
10. Named binding to a `reference<T>` parameter preserves reference
    semantics — the `\lvalue` expression is bound exactly as if positional.

### Default-argument ordering

Default expressions may reference earlier parameters
(`sub f(int a = 1, int b = a * 2, int c = a + b)`). Supplied named values are
written into the bound positional vector in **source-evaluation order**;
default-argument processing then runs in **parameter order** over that
vector. Therefore `f(c: 100)` yields `a=1, b=2, c=100`. This relies on the
existing parameter-order default loop and is exercised by the test matrix.

## Evaluation Order

Visible evaluation order is **source order**, not parameter order:

```qore
f(b: side_effect_1(), a: side_effect_2())   // side_effect_1 runs first
```

Source order is preserved by separating the evaluation container from the
final positional container via a per-call permutation map:

- At parse time, after the variant is selected,
  `lib/FunctionCallNode.cpp` (~277–280) stores the binder's mapping onto the
  runtime args list: `qore_list_private::setCallArgEvalMap(source_to_param,
  result_size)`.
- `include/qore/intern/qore_list_private.h` (~607–626) holds it:
  `setCallArgEvalMap()`, `hasCallArgEvalMap()`, `callArgEvalMapHasHoles()`
  (true when `pos_map->size() < result_size`), `getCallArgEvalMap()`,
  `getCallArgEvalResultSize()`.
- IR lowering (`lib/QoreIRLowering.cpp` ~9174–9207) evaluates supplied
  expressions in source order, scatters each result into its mapped
  positional slot, and fills unfilled slots with `const_nothing` (defaultable
  holes). The call opcode's operand vector is therefore positional and
  carries no names.
- Fast-call paths consult `callArgEvalMapHasHoles()` so binder-inserted
  `NOTHING` for omitted slots is treated as a defaultable hole, not as an
  explicitly supplied `NOTHING` argument.

This is the real ABI/opcode invariant: no call opcode carries names, no AOT
call-site metadata grows a name payload, ordered/no-hole named calls lower
like positional calls, and reordered/sparse named calls preserve source-order
and default semantics even when their lowering is not bit-identical to a
hand-written positional call.

## Overload Resolution

Parameter names are variant-specific, so binding runs per candidate ahead of
the existing positional scorer.

`lib/Function.cpp`:

- `bind_named_call_args(sig, source_types, names, binding, failure)` —
  per-candidate binder. It rejects positional-after-named, unknown names,
  overwrite-of-positional, and duplicate-to-same-parameter, and otherwise
  produces:
  - `NamedArgCandidateBinding { arg_types, supplied, source_to_param,
    result_size, omitted_defaultable }` — the per-candidate plan;
  - `NamedArgBindFailureReason` / `NamedArgBindFailure` — the rejection
    reason and offending name for diagnostics.
- `parseFindVariantNamed(...)` — iterates accessible variants, skips
  non-`isNamedCallable()` and varargs-only variants, calls the binder, then
  runs the existing positional type scorer over the bound vector. The
  winning plan is persisted on the call node as
  `QoreNamedArgBinding { std::vector<size_t> source_to_param; size_t
  result_size; }` (`include/qore/intern/Function.h`).

### Tie-breaking

Implemented in the candidate comparison (`lib/Function.cpp` ~2097–2135), in
order:

1. Existing positional type-match score (`pscore` / `max_pscore`) decides
   first.
2. Then more perfect (`QTI_IDENT`) parameter matches (`nperfect`).
3. Then **fewer defaulted-omitted slots** (`omitted_defaultable`) — the more
   specific signature wins.
4. Then fewer total declared parameters.
5. Otherwise the existing positional-overload ambiguity behaviour applies
   (error with the candidate list).

The defaulted-slot tie-break composes *after* the existing scorer and never
overrides a strictly-better type match.

## Diagnostics

Concrete error codes for IDE / qls consumption:

| Code | Raised when | Location |
| --- | --- | --- |
| `NAMED-ARG-DUPLICATE` | same name supplied twice in one call | `lib/parser.ypp` ~1766; `lib/Function.cpp` ~2229 |
| `NAMED-ARG-UNKNOWN` | name matches no parameter on any candidate | `lib/Function.cpp` ~2219 (lists accessible parameter names) |
| `NAMED-ARG-POSITIONAL-AFTER-NAMED` | positional arg follows a named arg | `lib/parser.ypp` ~3073; `lib/Function.cpp` ~2226 |
| `NAMED-ARG-OVERWRITES-POSITIONAL` | named arg targets an already positionally-bound slot | `lib/Function.cpp` ~2222 |
| `NAMED-CALL-NOT-SUPPORTED` | no parse-time signature / varargs-only / builtin without `QCF_NAMED_ARGS` / runtime-only dispatch | `lib/Function.cpp` ~2204; `lib/FunctionCallNode.cpp` ~271; `lib/CallReferenceNode.cpp` ~162 |

When an existing, more informative call error applies, it is preserved
rather than replaced by a generic named-call failure.

## Runtime ABI

The C++ ABI is unchanged. No alternate builtin signature (e.g. a
hash-taking `q_named_func_t`) was added. Name binding happens before
execution and every target is called with the same `QoreListNode*` it
receives today. This keeps dispatch, module APIs, reflection, call
references, closure execution, and typed-callable compatibility single-path.

## Builtin Opt-In

User-defined functions, methods, and closures are named-callable whenever
the signature carries names (always, for user code) — names in user code are
inherently part of the public API.

Builtins are named-callable **only** when the variant opts in:

- `QCF_NAMED_ARGS = (1 << 7)` — `include/qore/QoreLib.h:143`.
- `AbstractQoreFunctionVariant::isNamedCallable()` returns
  `is_user || (flags & QCF_NAMED_ARGS)` — `include/qore/intern/Function.h`
  (~647).
- `.qpp` flag keyword `NAMED_ARGS` (e.g.
  `[flags=RET_VALUE_ONLY,NAMED_ARGS]`); `lib/qpp.cpp` registers it in the
  flag set and `hasNamedArgsFlag()` emits `QCF_NAMED_ARGS`. Hand-written
  registrations pass `QCF_NAMED_ARGS` directly.

Builtin parameter names were never previously a calling-interface contract,
so opt-in is gated behind an audit. The audit/sanitize/allowlist process —
parameter-name conventions, exclusions (varargs, dynamic forwarding,
option-hash keys, deprecated/NOOP, ambiguous overload sets), tests, and the
core/binary-module migration record — is maintained separately in
[`design/named-argument-builtin-review-checklist.md`](named-argument-builtin-review-checklist.md).
That checklist is the authoritative operational reference; this design does
not duplicate it.

## Reflection And API Metadata

Reflection (`modules/reflection/src/QC_AbstractVariant.qpp`;
`include/qore/QoreReflection.h:59`):

- `Reflection::AbstractVariant::isNamedCallable()` returns `bool` — true for
  user variants and for builtins with `QCF_NAMED_ARGS`. Inherited by
  `FunctionVariant`, `Method`-variant reflection classes, etc.
- `Reflection::AbstractVariant::getNamedParameterNames()` returns
  `list<string>` — non-empty parameter names participating in the
  named-callable surface; an **empty list** when `isNamedCallable()` is
  false (pair the two calls rather than overloading "no names" onto a
  nullable return).
- `NAMED_ARGS` appears in the variant's `getCodeFlags()` list when set.

Generated API metadata (`lib/qpp.cpp` `serializeMetadataNamedArgsJson`,
~3142–3146; consumed by `qlib/QoreApiMetadata`): each function/method entry
in `*.meta.json` carries `"named_callable": <bool>` and
`"named_parameters": [ ... ]`, in addition to `NAMED_ARGS` appearing in the
existing `flags` array. The builtin-review checklist's inventory `jq`
queries rely on this schema.

## Closures And Call References

Call references and closures execute through
`execValue(const QoreListNode* args)`, so the ABI stays positional. Named
calls are accepted only when the concrete function object and the selected
variant are known at parse time (e.g. an inline-invoked closure literal,
`(sub (int x) { return x; })(x: 5)`). Otherwise `lib/CallReferenceNode.cpp`
(~162) rejects the call with `NAMED-CALL-NOT-SUPPORTED`. Generic `code`
values and external call references without parse-time parameter names are
rejected; lifting this is part of v2.

## Varargs And `argv`

Named arguments bind declared parameters only and are never appended to
`argv`. A varargs-only signature (`sub f(...)`) rejects named calls. For a
mixed signature `sub f(int a, ...)`:

- `f(a: 1)` — accepted (binds `a`, no varargs).
- `f(1, 2, 3)` — accepted (all positional, as today).
- `f(a: 1, 2, 3)` — rejected via the positional-after-named rule. Named
  binding of declared parameters cannot be combined with varargs in one call.

## Compatibility

- Existing positional calls behave exactly as before.
- C++ function/method callback signatures are unchanged.
- Named syntax is accepted only where it cannot change the parse of existing
  code.
- AOT wire format is unchanged; pre-change and post-change AOT artifacts
  interoperate. (AOT round-trip is part of the test matrix.)

The principal cost is API governance, not runtime behaviour: a public
parameter name becomes part of the calling interface once any caller binds
it by name. Renaming a public parameter can break named callers even when
positional callers are unaffected. For builtins this exposure is bounded by
the audited `QCF_NAMED_ARGS` allowlist; user-code authors who do not want
their parameter names to be public must rename defensively before release.

## Deferred work (v2)

Not implemented; intentionally out of v1 scope:

- **Runtime-resolved variants.** Named calls where the variant cannot be
  fixed at parse time (some virtual/abstract/dynamic dispatch shapes,
  multi-variant callrefs) require names to survive into the call opcode: a
  `Call`-family opcode variant carrying a parallel names array, runtime
  variant lookup taking positional types plus names, and AOT serialization
  extended along the established pattern (new `QORE_AOT_FEAT_*` bit, new
  per-call name section, and matching writer/reader hooks in the slot-map
  **and** handler-IR serializers — the recurring footgun precedent is the
  LVPath slice work, commit `d31e62cf1`).
- **Named calls through typed callable values.** `code<T(int x, *string s)>`
  currently models parameter types but not names; carrying names in the
  callable type would enable named calls through typed `code` values without
  the concrete function object at parse time.
- **Hash spread (`**opts`).** Expanding a hash-typed value into named
  arguments by key. The `**` prefix in argument position is reserved for
  this and must not be reused.
- **`@param[named]` doxygen attribute.** Explicit per-parameter marking of
  the public named-callable surface for API governance.

These are noted so the syntax space is not accidentally consumed; none
changes the v1 contract above.
