# Qore Named Arguments — Design

## Summary

Qore can support call-site named arguments without introducing a second runtime
calling convention. The right implementation is a source-level **named-argument
syntax** that binds supplied arguments to the selected signature's positional
slots before the target is executed. v1 keeps that binding limited to targets
whose signature and variant are known at parse time.

```qore
connect(host: "example.com", timeout: 30s)
```

conceptually lowers, after variant selection and argument evaluation, to the
same kind of positional list used by the standard call path:

```qore
connect("example.com", NOTHING, 30s)
```

assuming the selected signature is:

```qore
sub connect(string host, int port = 443, *timeout timeout = NOTHING)
```

This approach reuses the existing runtime ABI, preserves the C++ module
interface, and reuses the existing default-argument and type-filtering
machinery. It does **not** require a new IR opcode or AOT wire-format change for
v1, but it does require the existing call-argument lowering and optimized
fast-call paths to preserve named-call semantics.

The feature is intentionally framed as **named arguments**, not "call by name".
The actual call convention remains Qore's single positional list convention.

## Status

- Branch: `feature/5164_jit`
- Reference commit: `b49506f07` (file/line citations below are anchored here and
  will drift; treat them as starting points).
- Scope: design only at this stage; no implementation has begun.
- Target release: 2.3.0 (subject to scheduling).

## What The Current Sources Show

> Line citations in this section (and the Implementation Sketch) are anchored
> to the reference commit listed in Status; they will drift as the branch
> evolves. Treat them as starting points, not exact addresses.

Qore's call ABI is list-based throughout the public C++ API:

- `q_func_t`, `q_external_func_t`, `q_method_t`, constructor callbacks, and
  related builtin entry points all receive `const QoreListNode* args`.
  - `include/qore/common.h:355-387`
- `QoreProgram::callFunction()` also takes only `const QoreListNode* args`.
  - `include/qore/QoreProgram.h:173-181`
- `ResolvedCallReferenceNode::execValue()` takes only `const QoreListNode* args`.
  - `include/qore/CallReferenceNode.h:132-137`
- Function-call parse/eval state stores `QoreParseListNode* parse_args`,
  `QoreListNode* args`, and an optional resolved variant, with no
  name-carrying call object.
  - `include/qore/intern/FunctionCallNode.h:40-44`

Parser call construction is also list-based:

- `processCall()` creates function, method, static method, and call-reference
  call nodes using `make_args(...)`.
  - `lib/parser.ypp:1765-1837`
- `myexp` is either empty or a generic expression.
  - `lib/parser.ypp:2961-2964`
- `make_args()` returns the existing parse list unchanged when possible,
  otherwise wraps a single expression in a one-element `QoreParseListNode`.
  - `lib/QoreLib.cpp:1615-1628`

Argument parsing and evaluation currently treat call arguments as an ordered
list:

- `FunctionCallBase::parseArgsVariant()` asks `QoreParseListNode::initArgs()`
  for a positional type vector and positional runtime list, then calls
  `parseFindVariant()`.
  - `lib/FunctionCallNode.cpp:165-206`
- `QoreParseListNode::initArgs()` stores argument values and parse-time
  argument types in positional order.
  - `lib/QoreParseListNode.cpp:186-203`
- `QoreParseListNode::evalImpl()` evaluates entries in list order.
  - `lib/QoreParseListNode.cpp:130-183`

Variant resolution is positional:

- Runtime overload matching iterates `pi` from `0` to `sig->numParams()` and
  compares parameter `pi` with argument `pi`.
  - `lib/Function.cpp:1037-1191`
- Parse-time overload matching does the same with `argTypeInfo[pi]`.
  - `lib/Function.cpp:1555-1720`

The key enabling fact is that function signatures already carry parameter
names:

- `AbstractFunctionSignature` stores `typeList`, `defaultArgList`, and `names`.
  - `include/qore/intern/Function.h:66-69`
- It exposes `getParamNames()` and `getName(i)`.
  - `include/qore/intern/Function.h:102-104`,
    `include/qore/intern/Function.h:162-164`
- User function parsing pushes parameter names into the signature.
  - `lib/Function.cpp:636-700`
- Builtin signatures can also carry names.
  - `include/qore/intern/BuiltinFunction.h:42-64`
- Reflection exposes parameter names.
  - `lib/QoreReflection.cpp:211-213`

Default-argument processing is already close to what named arguments need:

- Missing or `NOTHING` arguments are filled from `defaultArgList`.
  - `lib/Function.cpp:349-383`
- Non-default argument values are filtered through the selected parameter type
  by position.
  - `lib/Function.cpp:385-415`
- User-code parameter locals are instantiated from positional slots.
  - `lib/Function.cpp:2036-2083`

The main caveat is that optimized fast-call paths currently distinguish missing
arguments from explicit `NOTHING` by `nargs`, not by a per-parameter supplied
mask:

- `instantiateFastCallParams()` evaluates defaults only when `i >= nargs`.
  - `lib/JITRuntime.cpp:4431-4479`
- `QoreIRInterpreter` direct-parameter execution copies the supplied argument
  range directly into parameter slots.
  - `lib/QoreIRInterpreter.cpp:2261-2299`

Named calls that skip middle parameters therefore cannot blindly pad the fast
path's operand list with `NOTHING` and still expect default arguments to fire.

The astparser/tree-sitter grammar is currently also positional:

- `call_expression` has `function` and `argument_list`.
  - `modules/astparser/grammars/tree-sitter-qore/grammar.js:881-895`
- `argument_list` is just comma-separated expressions.
  - `modules/astparser/grammars/tree-sitter-qore/grammar.js:908-912`

Any source syntax change must therefore be mirrored there.

## Does It Make Sense?

Yes, with constraints.

It makes sense for fixed-signature APIs where arguments are optional, reordered
for readability, or sparse because defaults are common. It would improve call
sites like:

```qore
DatasourcePool::constructor(driver: "pgsql", db: "app", host: "db.internal", max: 20)
```

or:

```qore
schedule(name: "nightly", start: 02:00, retry_count: 3)
```

It is less compelling for extensible options surfaces, especially where an
argument is already semantically a data object. In those cases a typed
hash/hashdecl option remains better:

```qore
hash<HttpListenerOptionInfo> opts = <HttpListenerOptionInfo>{
    "port": 8011,
    "bind": "127.0.0.1",
};
```

For example, `HTTPClient::constructor(hash<auto> opts)` is not made callable as
`HTTPClient(url: "...", timeout: 30s)` by this design, because `url` and
`timeout` are option-hash keys, not formal parameters. v1 can only support
`HTTPClient(opts: {"url": "...", "timeout": 30s})` if that constructor is
explicitly opted in. A separate "named options hash expansion" feature would be
needed to turn option keys into call-site argument names.

Named arguments would make parameter names part of the public API. For public
modules, changing a parameter name would become a compatibility break for named
callers, even if positional callers remain unaffected. This is an API
governance concern, not a runtime concern, and is the single largest
non-technical cost of the feature.

## Recommended Surface Syntax

Use `name: expression` inside call argument lists:

```qore
sub connect(string host, int port = 443, *timeout timeout = NOTHING, bool tls = True) {
    ...
}

connect(host: "example.com");
connect(host: "example.com", tls: False);
connect("example.com", timeout: 30s);
```

This is visually consistent with Qore hash syntax, but should be parsed only in
call argument context. It must not be added as a generic expression form.

Alternative syntaxes considered:

- `name = expression` — conflicts visually and syntactically with ordinary
  assignment expressions.
- `.name = expression` — less idiomatic for Qore and visually heavier than
  `name:`.
- `name => expression` — the Perl/PHP-style fat arrow; readable but adds a new
  operator token to the parser, would need to be reserved against any future
  use as a "maps-to" expression operator, and offers no clarity advantage over
  `name:`.

### Grammar Disambiguation

`IDENT ':'` is unambiguous **only at the start of an argument slot**. The
parser must lookahead one token at the start of each call-argument production
and treat `IDENT COLON` as a named-argument introducer there; in all other
contexts the existing meaning of `:` is preserved.

| Context | Existing meaning | Conflict? |
| --- | --- | --- |
| Ternary `a ? b : c` inside an expression | conditional separator | No — `:` appears mid-expression, not at slot start |
| `case x:` inside a switch | statement-level label | No — statement context, not call-argument context |
| `goto label:` / `label:` | statement-level label | No — statement context |
| Brace-form hash literal `{a: 1}` | key-value separator | No — already inside a `{...}` block |
| Paren-form hash literal `(a: 1, b: 2)` as expression | key-value separator | No — at a call site the call's outer `(...)` are call parens, not hash delimiters; named-arg `name: expr` is enabled only at call-arg slot starts. To pass a paren-form hash as a positional argument, wrap it explicitly: `f((a: 1, b: 2))` |
| Map operator key shorthand | n/a | No new ambiguity |

### Pitfall: `f(a: 1)` vs `f((a: 1))`

Because Qore allows paren-form hash literals (`(a: 1)` is a hash with one
member), the extra parens matter at a call site:

- `f(a: 1)` — **named call** binding parameter `a` to `1`.
- `f((a: 1))` — **positional call** with one argument: the hash `{a: 1}`.

Code that today passes paren-form option hashes as `f((opts...))` and is
edited to drop the inner parens will silently change meaning post-feature.
Documentation and lints should call this out.

The grammar must therefore introduce a call-argument-specific production
(e.g. `call_arg : IDENT ':' expression | expression`) used by `f(...)`, method
calls, constructors, base constructor calls, and call-reference calls, and
**must not** add `name: expression` as a generic expression form. Paren-form
list and hash literals (`(1, 2, 3)`, `(a: 1, b: 2)`) keep using `myexp` and
remain expression-context constructs; named-arg syntax is enabled only at
call-arg slot starts.

## Recommended Semantics

1. Positional arguments may appear first.

```qore
connect("example.com", timeout: 30s)
```

2. Named arguments may follow positional arguments.

3. Positional arguments after named arguments must be a parse error or
   named-call setup error.

```qore
connect(timeout: 30s, "example.com")  # error
```

4. Duplicate named arguments must be an error.

```qore
connect(host: "a", host: "b")  # error
```

5. A named argument must match a declared parameter name in the selected
   variant.

6. Named arguments must not bind to varargs / `argv`.

7. Unknown named arguments must make a variant ineligible; if no variant
   remains, the diagnostic should mention the unknown name and list tested
   variants as current overload errors do.

8. Omitted parameters are defaultable missing slots. In the existing
   `CodeEvaluationHelper` path this can be represented as `NOTHING` in the
   positional list, because default processing already treats `NOTHING` as
   defaultable. Optimized IR / JIT fast-call paths must either receive equivalent
   missing-slot metadata or be kept on the standard path for named calls with
   holes; they must not treat binder-inserted `NOTHING` as an explicit supplied
   argument.

9. Parameter names are case-sensitive, matching normal Qore identifiers.

10. Named calls to a callable with no known signature are rejected at parse
    time. Rejection is simpler and safer than runtime reflection support.

11. Named binding to `reference<T>` parameters preserves reference semantics.
    The supplied expression (typically a `\lvalue` form) is bound exactly as
    if positional; the binder must not strip, evaluate, or wrap the reference
    during source-order evaluation. The receiving slot remains a
    `ReferenceNode`.

### Default-Arg Ordering

Default expressions may reference earlier parameters:

```qore
sub f(int a = 1, int b = a * 2, int c = a + b) { ... }
```

With named arguments, supplied slots can fill out of parameter order
(`f(c: 100)` supplies the third slot only). The invariant is:

1. The binder writes supplied values into the bound positional vector in
   source-evaluation order (so observable side effects fire in source order).
2. **Default-argument processing then runs in parameter order** over the
   bound vector — slots `0..n` are visited in declaration order, and a
   default expression for slot `i` may legally read previously filled or
   defaulted slots `0..i-1`.

This matches the existing default-arg loop at `lib/Function.cpp:349-383`,
which already iterates in parameter order. The invariant is automatic so
long as the binder does not interleave default evaluation with supplied
evaluation. Make this an explicit test (see Test Matrix): given the
signature above, `f(c: 100)` must yield `a=1, b=2, c=100`.

### ABI / Opcode Invariant

For v1, a named call to a parse-time-resolved variant must not introduce a
second runtime calling convention: name binding runs at parse time, the call
opcode operand vector remains positional, and no parameter names are carried in
the runtime call ABI or AOT wire format.

Do **not** state the v1 contract as "bit-identical IR / JIT / AOT artifacts" or
"zero runtime overhead" for every named call. That is too strong:

- reordered named arguments must still evaluate in source order, so producer IR
  may differ from a hand-written positional call in parameter order;
- named calls with defaultable holes may intentionally route through the
  standard `CodeEvaluationHelper` path in v1 instead of an optimized fast path;
- only the simple subset with no reordering and no omitted middle slots should
  be expected to lower identically to equivalent positional syntax.

Tests should guard the real invariant: no call opcode carries names, no AOT
metadata grows a name payload for call sites, ordered/no-hole named calls lower
like positional calls, and reordered or sparse named calls preserve semantics
even when their lowering is not bit-identical.

## Evaluation Order

Evaluation order is the most important semantic trap.

A naive implementation that reorders parse nodes into parameter order before
evaluation would change behavior for calls like:

```qore
f(b: side_effect_1(), a: side_effect_2())
```

The visible evaluation order must be **source order**, not parameter order.
That means named-call lowering must:

1. Evaluate each supplied argument in source order.
2. Store the evaluated value into the selected positional parameter slot.
3. Fill omitted/defaulted slots as `NOTHING`.
4. Let existing default processing replace `NOTHING` with defaults.

This differs from the current `QoreParseListNode` behavior because the list is
both the source-order evaluation container and the final positional container
today. Named calls need a small intermediate representation to separate those
two concepts. The current IR helper `lowerCallArgs()` also evaluates arguments
by iterating the call's positional argument container. v1 must change that
lowering path for named calls: emit SSA values for supplied expressions in source
order, then assemble the call opcode's operand vector in positional parameter
order according to the binder's mapping.

## Overload Resolution

Named arguments interact with overloads in a non-trivial way because parameter
names are variant-specific.

For each candidate variant:

1. Start with the positional arguments and bind them to parameters `0..n`.
2. For each named argument, look up the name in that candidate's
   `sig->getParamNames()`.
3. Reject the candidate if the name is absent, duplicated, or would overwrite
   an already-bound positional slot.
4. Produce a per-candidate positional type vector / value vector with unbound
   slots as `NOTHING`.
5. Run the existing parse-time or runtime scoring logic over that positional
   vector.

This preserves the current "best type match wins" model while adding a binding
phase ahead of it.

Ambiguity can occur if two overloads accept the same named call equally well:

```qore
sub f(int count, string mode = "x") { ... }
sub f(int size, string unit = "x") { ... }

f(count: 1)  # first only
f(size: 1)   # second only
f(1)         # existing positional overload rules
```

If a named call maps to more than one variant with equal score, the existing
ambiguity / variant-match behavior is extended rather than replaced.

### Tie-Breaking Rule

Named arguments add a new dimension to overload resolution: two variants can
both accept the same named call with the same scored type match while
differing in *how many slots are defaulted-omitted*.

```qore
sub g(int a, *int b) {}         # variant 1
sub g(int a, *int b, *int c) {} # variant 2

g(a: 1)  # both accept; both score equally on the supplied a
```

**Tie-breaker order:**

1. Existing positional type-match scoring decides first. If one variant
   scores strictly better on the supplied (post-binding) positional vector,
   it wins outright — defaulted-slot count is not consulted.
2. If type-match scores are equal, **prefer the variant with fewer
   defaulted-omitted slots** (the more specific signature match). This
   matches positional intuition where extra optional parameters do not
   dilute a match.
3. If still tied, fall back to the existing positional-overload ambiguity
   behavior (error with the candidate list).

The defaulted-slot tie-break therefore composes *after* the existing scorer
and never overrides a strictly-better type match.

## Runtime ABI Impact

The existing C++ ABI stays unchanged.

Do not add alternate builtin signatures like:

```c++
QoreValue (*q_named_func_t)(const QoreHashNode* args, ...);
```

That would duplicate dispatch, module APIs, reflection, call references,
closure execution, and typed callable compatibility. It would also force every
builtin and module author to decide which convention they support.

Instead, perform name binding before execution and call all targets with the
same `QoreListNode*` they receive today.

## IR / JIT / AOT Impact (feature/5164_jit context)

This branch's overarching goal is to eliminate AST fallback and lower
everything to IR. The named-argument design above is intentionally aligned with
that goal: by binding names to positional slots at parse time, the existing
call opcodes do not need to carry parameter names. The IR lowering layer does
still need to know the named-call evaluation plan so it can preserve source
order while presenting positional operands to the existing call opcodes.

Two scopes are possible:

### v1 — parse-time-resolved variant only (recommended)

- Named arguments are accepted only when the variant resolves at parse time.
- The binder produces a per-variant plan: supplied entries stay in source order
  for evaluation, and each entry records its final positional parameter slot.
- The existing call IR opcodes remain positional. IR emission changes only at
  argument lowering: it emits supplied expressions in source order, then builds
  the call opcode operand vector in parameter index order.
- Defaultable omitted slots are handled consistently across the standard
  `CodeEvaluationHelper` path and optimized fast-call paths. For v1 this can be
  done by adding missing-slot metadata to the lowered plan, disabling fast paths
  for named calls with holes, or teaching the fast paths to treat
  binder-inserted `NOTHING` as defaultable rather than explicitly supplied.
- AOT wire format is unchanged. No new feature flag bit. No new section. No
  new opcode.

This means **no new IR opcode and no AOT wire-format change for v1**. It does
not mean that the IR / JIT implementation is untouched. The feature lives in:

- `lib/parser.ypp` / `lib/scanner.lpp`
- `QoreParseListNode` (or a new sibling)
- `FunctionCallBase::parseArgsVariant`
- `Function.cpp` overload scoring
- `CodeEvaluationHelper` argument preparation
- `QoreIRLowering::lowerCallArgs()` or an equivalent named-call lowering path
- optimized fast-call handling in `JITRuntime.cpp` and `QoreIRInterpreter.cpp`
  when omitted named slots are present
- astparser + tree-sitter grammar mirror

### v2 — runtime-resolved variant (deferred)

When parse-time resolution is impossible (some virtual / abstract / dynamic
dispatch shapes, callrefs that genuinely have multiple variants), the names
must survive into the call opcode. This requires:

- A new IR opcode variant (or per-arg name-index sidecar) for `Call`-family
  opcodes that carry a parallel `names` array.
- Runtime variant lookup that takes both positional types and a `names` array.
- AOT serialization extended along the established pattern: a new feature flag
  bit (the next free bit in `QORE_AOT_FEAT_*` at the time of v2
  implementation), a new section for per-call name vectors, and matching
  writer / reader hooks in `QoreAOTInstRegistry.cpp`, `QoreAOTBinary.cpp`
  (slot map), and `QoreAOTRuntime.cpp` (slot map + handler IR).
- Slot-map serializer **and** handler-IR serializer must both be updated; this
  has been a recurring footgun on this branch (e.g. LVPath slice work,
  commit `d31e62cf1`).

v2 is a meaningful, but well-trodden, addition. It is intentionally deferred
out of v1 to keep the scope honest.

## Implementation Sketch (v1)

1. Add a call-argument parse node or lightweight structure
   (e.g. `QoreCallArgsNode`) that stores source-order entries as:

```c++
struct CallArg {
    std::string name; // empty for positional
    QoreValue value;
    const QoreProgramLocation* loc;
};
```

2. Decide whether this node is a `QoreParseListNode` subtype, a sibling owned by
   `FunctionCallBase`, or a short-lived parser/binder structure. If it is not a
   subtype, update every place that currently walks `getParseArgs()` as a
   positional parse list, including IR lowering, IR verifier/local
   classification, AOT expression handlers, constructor lowering, and
   `VarRefNewObjectNode` paths.

3. Change call grammar to use a call-specific argument rule rather than
   generic `myexp` for `f(...)`, method calls, constructors, base constructor
   calls, and call reference calls.

4. Preserve `QoreParseListNode` for ordinary list expressions.

5. Add a binder that, given a candidate `AbstractFunctionSignature`, creates a
   positional parse/eval plan with explicit `source_order_index ->
   positional_slot_index` mapping and a record of which positional slots were
   actually supplied.

   Recommended location: a new `NamedArgBinder` helper (header in
   `include/qore/intern/`, implementation in `lib/`) invoked from
   `FunctionCallBase::parseArgsVariant()`. Putting the binder in its own
   class keeps the per-candidate logic out of `FunctionCallBase` and
   makes it directly usable from the closure / inline-literal code path
   without inheriting from `FunctionCallBase`.

   The resulting `Plan` is cached on the call node alongside the resolved
   variant after parse-commit, so IR lowering does not rebuild it. If the
   variant is re-resolved (e.g. across a parse rollback / retry), the
   plan is invalidated together with the variant pointer.

6. Extend parse-time variant lookup to accept named-call information.
   Internally it can still call a refactored positional scorer.

7. After the variant is selected, evaluate supplied expressions in source
   order and write results to positional slots. The IR emitter follows the same
   plan: current positional-only `lowerCallArgs()` behavior is not sufficient for
   named calls because it conflates evaluation order with operand order.

   Pseudo-IR for `f(b: side1(), a: side2())` against `sub f(int a, int b, *int c)`:

   ```
   ; supplied expressions evaluated in source order:
   %0 = call @side1                   ; supplied for slot 1 (b)
   %1 = call @side2                   ; supplied for slot 0 (a)
   ; defaultable hole filled with literal NOTHING:
   %2 = const_nothing                 ; slot 2 (c) — defaultable
   ; call opcode operand vector built in positional parameter order:
   %3 = call_user @f, [%1, %0, %2]    ; [a, b, c]
   ```

   The binder produces a `Plan { source_order_ssa_ids[], slot_index_for_source[i],
   supplied_mask }`, and the IR emitter materializes the operand vector by
   reading `source_order_ssa_ids[plan.inverse_slot_map[slot]]` for each
   parameter slot, substituting `const_nothing` for unsupplied slots.

8. Reuse `CodeEvaluationHelper::prepareDefaultArgs()` and
   `processDefaultArgs()` for the standard path.

9. Handle optimized fast-call paths explicitly. Three options, ranked by
   v1 risk:
   - **(a) recommended for v1**: mark named calls with defaultable holes
     ineligible for fast paths and route them through the standard
     `CodeEvaluationHelper` prepared-list path. Lowest risk, smallest diff,
     and named calls without holes (i.e. all required slots supplied by name)
     remain fast-path eligible.
   - (b) pass a supplied-slot bitmap / missing-slot plan into the fast path
     (`instantiateFastCallParams()` at `lib/JITRuntime.cpp:4431-4479` and the
     direct-parameter path in `QoreIRInterpreter.cpp:2261-2299`). The right
     v2 answer once measurement shows named-with-holes is a hot path.
   - (c) normalize fast-call default handling so binder-inserted `NOTHING`
     triggers defaults identically to the standard path. Tempting but
     conflates explicit `NOTHING` (currently a supplied argument) with
     binder-inserted `NOTHING` (a defaultable hole) and is a behavior change
     for positional callers; not recommended.

10. Add diagnostics with concrete error codes for IDE / qls consumption:
   - `NAMED-ARG-DUPLICATE` — duplicate named argument in the same call
   - `NAMED-ARG-UNKNOWN` — name does not match any parameter on the
     selected (or any candidate) variant
   - `NAMED-ARG-POSITIONAL-AFTER-NAMED` — positional argument appears after
     a named argument
   - `NAMED-ARG-OVERWRITES-POSITIONAL` — named argument refers to a slot
     already filled positionally
   - `NAMED-CALL-NOT-SUPPORTED` — target rejects named calls (varargs-only
     signature, callref / closure without parse-time signature, virtual /
     abstract dispatch with runtime-only variant resolution, builtin without
     opt-in)

11. Mirror syntax support in:
   - `modules/astparser/src/ast_parser.ypp`
   - `modules/astparser/grammars/tree-sitter-qore/grammar.js`
   - generated tree-sitter files, if the project expects generated sources to
     be committed
   - AST model / classes if callers need to distinguish named arguments

12. Builtin opt-in mechanism. Builtin parameter names currently live in
    `AbstractFunctionSignature::names` / `BuiltinSignature::names` for
    documentation and reflection. They have never been a public API contract.
    v1 must not silently promote every builtin parameter name into a stable
    surface. Concrete proposal:

   - **User-defined functions, methods, closures**: named-callable by
     default whenever names are present in the signature (which is always
     for user code). Names in user code are inherently part of the public
     API.
   - **Builtins**: named-callable **only** when the variant is registered
     with a new code flag, e.g. `QCF_NAMED_ARGS`, in the `QCF_*` flag space
     used by `addVariant()` / builtin registration sites. Default off.
     Variants without the flag reject named calls with
     `NAMED-CALL-NOT-SUPPORTED`.
   - v1 includes a builtin-argument-name audit and sanitize pass before
     release. The flag is then applied only to the reviewed subset. Builtins
     outside that subset remain rejected until they are audited in a later
     release.

13. Builtin argument-name audit state and pass — see the dedicated section
    below ("Builtin Argument-Name Audit") for current state, generated-metadata
    snapshot, and audit / sanitize procedure.

14. Reflection API surface additions:

   - `Reflection::AbstractVariant::isNamedCallable() returns bool` —
     whether this variant accepts named-argument calls (true for user
     variants, true for builtins with `QCF_NAMED_ARGS`).
   - `Reflection::AbstractVariant::getNamedParameterNames() returns list<string>` —
     the subset of parameter names participating in the named-callable
     surface (currently equal to `getParameterNames()` minus any varargs
     slot; reserved for future per-parameter opt-out). Returns an empty
     list when `isNamedCallable()` is false; pair the two calls instead
     of overloading "no names available" onto a nullable return.

## Builtin Argument-Name Audit

This is process work, not implementation, but the audit is on the v1 critical
path: until builtin parameter names are reviewed, no builtin variant can be
opted into the named-callable surface (see Implementation Sketch step 12).

### Current source state

- `.qpp` fixed parameters already have source-level names, and `qpp.cpp`
  emits those names into generated `addBuiltinVariant()`, `addMethod()`,
  and `addConstructor()` calls.
  - `lib/qpp.cpp:1300-1391`, `lib/qpp.cpp:2952-2973`
- Hand-written builtin registration also has name plumbing through
  `qore_process_params()` and the vector-based registration APIs, but
  callers can still provide empty, generic, or documentation-only names.
  - `lib/QoreLib.cpp:3079-3087`,
    `include/qore/intern/QoreNamespaceIntern.h:582-610`,
    `lib/QoreClass.cpp:4300-4364`
- Reflection already exposes `getParamNames()` from `AbstractVariant`, so
  users can see these names today; they are observable but not yet a
  compatibility promise for call syntax.
  - `modules/reflection/src/QC_AbstractVariant.qpp:735-747`
- Many existing names are good formal names (`pattern`, `timeout_ms`,
  `encoding`, `headers`, `lock`). Others are intentionally generic (`arg`,
  `v`, `l`, `h`, `opts`, `options`) and should not automatically become
  public call names.
- Hash-option APIs remain hash-option APIs. A formal parameter named
  `opts` or `options` may be named-bound as `opts: ...` only if opted in;
  its contained keys are not named arguments.
- Variadic builtins and variants with `QCF_USES_EXTRA_ARGS` require extra
  caution. Named arguments may bind only declared fixed slots; they must not
  bind the ellipsis / extra-args tail. Most variadic helpers (`sprintf`,
  `print`, dynamic call helpers, SQL bind helpers) should remain not opted
  in for v1.

### Generated-metadata snapshot

From this branch's build tree as of the reference commit:

- 271 `.qpp`-sourced `.meta.json` files describe 4,199 QPP-authored
  builtin variants.
- Those variants expose 3,585 fixed parameters and 91 varargs markers.
- Generated metadata currently has no missing parameter names and no
  duplicate names within a variant, which means the first audit problem is
  semantic quality, not mechanical absence.
- A conservative generic-name scan finds 695 fixed parameters with names
  such as `data`, `str`, `options`, `arg`, `vargs`, `opts`, or `h`.
- 68 fixed parameters are `hash<auto>` / `*hash<auto>` parameters named
  `opts`, `options`, or `h`; these are prime candidates for "do not confuse
  option keys with formal parameter names" review.

### Audit / sanitize pass

- Generate an inventory from the built tree or reflection: function / class
  / method / variant, source location, flags, fixed parameter count,
  parameter names, defaults, and varargs / `QCF_USES_EXTRA_ARGS` status.
- Reject variants with missing names, duplicate names, invalid identifiers,
  documentation-only names, or internal implementation names.
- Reject variants whose useful "names" are actually hash keys unless a
  separate named-options-hash feature is explicitly designed.
- Rename `.qpp` formal parameters before opt-in where the current names are
  too generic but the intended public names are clear. Since builtin names
  have not yet been call syntax, this is the last low-risk point to sanitize
  them.
- Add `NAMED_ARGS` to the `qpp` flag set and have it emit `QCF_NAMED_ARGS`;
  hand-written registrations use `QCF_NAMED_ARGS` directly.
- Commit the first opted-in set as an allowlist with tests. Good first-wave
  candidates are fixed-signature variants with descriptive parameters and no
  ellipsis, such as selected constructors, regex helpers, iterator
  constructors, and datasource constructors. Non-opted-in builtins must have
  negative tests.

## v1 Surface — Accepted vs Rejected

To keep v1 honest about what it does and doesn't cover, the accepted call
shapes are below. For user-defined variants, "has a parse-time-resolved
signature" is enough. For builtin variants, the same shape is accepted only if
the variant is in the audited `QCF_NAMED_ARGS` allowlist.

- function call: `f(name: expr, ...)`
- parse-time-resolved method call: `m(name: expr, ...)` inside object code, or
  an object method call where the target method and variant are resolved
  before runtime
- pseudo-method call on an intrinsic type (`<list>`, `<hash>`, `<string>`,
  etc.): always parse-time-resolved per receiver type via `QoreClass`, so
  named calls are accepted when the variant has named parameters (and, for
  builtin pseudo-methods, has opted in via `QCF_NAMED_ARGS`)
- static method call: `Cls::sm(name: expr, ...)`
- constructor call: `new Cls(name: expr, ...)`
- explicit base constructor call: `Cls::Cls(name: expr, ...)` /
  `: BaseCls(name: expr, ...)`
- inline-invoked closure literal: `(sub (int x) { return x; })(x: 5)` —
  parse-time signature is the literal's own
- direct closure / call-reference invocation **only** when the parser holds
  the concrete `UserClosureFunction*` (or equivalent function object) and
  the selected variant is fixed before runtime; in practice this is rare
  outside inline-invoked literals and is otherwise rejected

Rejected (parse-time error) in v1:

- dynamic method call where the target method or selected variant is virtual,
  abstract, or otherwise not parse-time resolvable
- call reference / closure invocation when the concrete parse-time signature and
  selected variant are not available
- varargs-only signature: `sub f(...)`
- builtin variant without `QCF_NAMED_ARGS`, even if reflection exposes parameter
  names
- attempts to bind hash option keys as parameter names, e.g.
  `HTTPClient(url: "...")` when the formal parameter is `hash<auto> opts`
- any call where overload resolution would have to be deferred to runtime

The parse-time-resolution restrictions are not permanent; v2 can lift them once
the IR / AOT named-arg payload exists. Builtin opt-in and hash-option-key
expansion remain separate API governance decisions.

## Call References And Closures

Call references and closures currently execute through
`execValue(const QoreListNode* args)`, so the ABI remains positional.

Named calls to closures are only safe when the closure has a known signature and
the selected variant can be fixed before runtime. Qore already builds typed
callable information from closure signatures, but typed callable types currently
model return and parameter types, not parameter names. Therefore:

- For direct closure values created in Qore source, v1 may support named calls
  only if the closure function object, parameter names, and selected variant are
  known at parse time.
- The v1 binder obtains the signature from the parse-time `UserClosureFunction*`
  associated with the inline-invoked literal (or any other shape where the
  parser holds the concrete function object). With the signature in hand, the
  binder produces the positional list as it would for a named function call;
  the closure is then invoked through the existing
  `execValue(const QoreListNode* args)` ABI.
- For generic `code` values or external call references without available
  parameter names, named calls are rejected.
- Supporting named calls through arbitrary call references would require
  exposing signature names consistently through
  `ResolvedCallReferenceNode::getFunction()` or a new virtual signature
  accessor. This is part of v2.

## Interaction With Varargs And `argv`

Named arguments must not be appended to `argv`. `argv` currently represents
arguments in excess of declared parameters, and user-call setup constructs it
from positional slots after `num_params`.

Named arguments are meant to bind declared parameters. Allowing them to flow
into varargs would create a second data shape inside `argv` and would undercut
the "lower to positional list" design.

For functions declared only as:

```qore
sub f(...)
```

named calls are rejected unless Qore explicitly adds a separate "keyword rest"
feature later.

For mixed signatures with declared fixed parameters and a trailing varargs
ellipsis, e.g. `sub f(int a, ...)`:

- `f(a: 1)` is accepted — binds `a`, supplies no varargs.
- `f(1, 2, 3)` is accepted as today — all positional.
- `f(a: 1, 2, 3)` is rejected via the "positional after named" rule (rule 3
  in Recommended Semantics). Practically, this means named binding of
  declared parameters cannot be combined with varargs in the same call.

## Compatibility

This can be added compatibly because:

- Existing positional calls behave exactly as they do today.
- Existing function and method C++ callback signatures do not change.
- Named syntax is only accepted where it cannot change the parse of existing
  code.
- AOT binaries produced before the change continue to load (v1 does not
  change wire format). This must be verified explicitly: build an AOT module
  on the pre-change tip, install it under `QORE_MODULE_DIR`, and load it on
  the post-change build with `QORE_BUILD_AOT_MODULES=ON`. The module must
  load without warnings and execute identically. Add this round-trip to the
  test matrix.
- The reverse direction also holds: AOT binaries produced by a v1 build can
  load on pre-v1 runtimes, because the wire format is unchanged. Source-level
  named-argument syntax cannot reach a pre-v1 parser, so this is only
  meaningful for AOT modules whose sources happen to use named calls
  internally — the lowering produces the same positional call opcodes either
  way.

The biggest compatibility concern is not runtime behavior; it is API
governance. Public parameter names become stable once any caller uses them.
Module documentation should call this out for each function whose names are
considered stable. The proposed builtin opt-in via `QCF_NAMED_ARGS` (see
Implementation Sketch step 12) limits exposure to the audited allowlist;
user-code authors who don't want their parameter names to become public API
must rename them defensively before releasing or rely on documentation
conventions.

## Test Matrix

The 2–3 day "Tests" line in the effort estimate covers the following
categories. None should be skipped; together they exercise every behavior
specified above.

| Category | Test cases |
| --- | --- |
| Basic dispatch | named-only call; mixed positional+named; zero-arg call; all-args call |
| Defaults | hole at end; hole in middle; default referencing earlier param (`f(c: 100)` over `sub f(int a=1, int b=a*2, int c=a+b)`); explicit `NOTHING` vs. omitted distinction |
| Source-order eval | named args with side effects in non-parameter order — observed order matches source order |
| Overload resolution | unique-by-name (name only matches one variant); ambiguous-by-type but resolvable-by-name; tie-break by fewer-defaulted-holes specificity rule; ambiguity error when still tied |
| Variant kinds | function, object method, pseudo-method, static method, constructor, base constructor, inline-invoked closure literal |
| References | `reference<T>` parameter named-bound (`f(out: \var)`) preserves reference semantics |
| Errors | one test per error code: `NAMED-ARG-DUPLICATE`, `NAMED-ARG-UNKNOWN`, `NAMED-ARG-POSITIONAL-AFTER-NAMED`, `NAMED-ARG-OVERWRITES-POSITIONAL`, `NAMED-CALL-NOT-SUPPORTED` (varargs-only, callref without sig, builtin without opt-in) |
| ABI / opcode invariant | no call opcode or AOT call-site metadata carries names; ordered/no-hole named call lowers identically to positional; reordered/sparse named calls are allowed to lower differently but must preserve source-order and default semantics |
| Execution modes | successful execution tests run across parser, IR-interp, JIT, and AOT modes where applicable; parser diagnostics are parser tests; reflection tests run in the normal interpreter path plus targeted AOT coverage if metadata changes |
| AOT pre-change compatibility | AOT module built on the pre-change tip loads and executes correctly under the post-change build with `QORE_BUILD_AOT_MODULES=ON` |
| Builtin allowlist | audited builtins accept named calls; non-allowlisted builtins reject with `NAMED-CALL-NOT-SUPPORTED`; hash option keys such as `url:` for `HTTPClient(hash<auto> opts)` reject unless a separate feature is implemented |
| Reflection | `AbstractVariant::isNamedCallable()` and `AbstractVariant::getNamedParameterNames()` on user variants and on builtin variants with/without `QCF_NAMED_ARGS` |

## Effort Estimate

Single engineer familiar with the parser and this branch.

| Phase | Scope | Days |
| --- | --- | --- |
| Grammar + parse node (`name: expr` in arg lists) | parser.ypp / scanner.lpp + `QoreCallArgsNode`-equivalent, plus affected parse-arg walkers | 3–4 |
| Variant binder + diagnostics (parse-time) | `parseArgsVariant`, scoring per candidate, error messages | 3–4 |
| Source-order eval lowering, default fill, type-filter reuse | wire into `CodeEvaluationHelper` and IR call-operand lowering | 2–3 |
| Fast-call default semantics | keep named calls with holes on the standard path, or teach JIT / IR fast paths supplied-slot semantics | 1–2 |
| Builtin argument-name audit + opt-in allowlist | inventory current names, sanitize `.qpp` names, add `NAMED_ARGS` qpp flag support, mark first reviewed builtin set | 2–4 |
| Reject paths (varargs-only, callref without sig, runtime-only resolution) | guards + diagnostics | 1 |
| astparser + tree-sitter mirror + AST class | grammar + generated files | 2–3 |
| Tests | parse, runtime, overload, errors, eval order, builtin allowlist, AOT round-trip via existing tests | 2–3 |
| Docs + release notes | doxygen + 2.3 relnotes | 1 |

**v1 total: ~3.5–5 weeks (17–25 days)** with no new IR opcode and no AOT
wire-format change. The estimate includes required changes in IR call-argument
lowering, optimized fast-call/default handling, and a first builtin opt-in
allowlist.

**v2 increment (if and when needed): ~2 weeks** — new IR opcode variant, AOT
feature flag bit + section, runtime variant lookup that takes names, slot-map
serializer updates (3 paths, per the LVPath slice precedent at
`d31e62cf1`).

## Future Extensions

These are explicitly **out of scope for v1 and v2** but are noted here so the
syntax space is not accidentally consumed by something unrelated and so
implementers know where to expect future pressure.

### Hash spread (`**opts`)

A natural complement for the "extensible options surface" case the doc
currently sends to `hashdecl`:

```qore
hash<HttpListenerOptionInfo> opts = <HttpListenerOptionInfo>{...};
new HttpListener(**opts);
```

Semantics would be: at the binder, expand a hash-typed value into named
arguments by key, with the key/parameter type-check applied per entry.
Allows mixing with named and positional. Out of scope for v1/v2 but the
syntax (`**` prefix in argument position) should not be reused for anything
else.

### `@param[named]` doxygen tag

API governance for module authors: let documentation explicitly mark which
parameters are part of the named-callable public surface, vs. which are
internal implementation details. Concrete syntax would extend `@param` with
an attribute (the standard doxygen direction is `@param[in]` / `@param[out]`,
which is already supported), so the named-callable marker would naturally
read as `@param[named]`:

```
//! Connects to the host
/** @param[named] host the host to connect to
    @param port the port to connect to (positional only) */
```

If the attribute approach proves awkward inside the existing doxygen filter
(`doxygen/qdx`, `qlib/Qdx.qm`), a Qdx-specific tag such as `@named_param`
is a fallback. The user-visible behavior is the same: the doc generator
records which parameters are part of the public named-callable surface.

Defers the "renaming a parameter breaks named callers" risk to opt-in
surfaces and documents intent for callers and tooling. Out of scope for v1.

### Named calls through typed callable values

Currently `code<T(int x, *string s)>` types model parameter types but not
names. v2 could extend the callable type to carry names (e.g.
`code<T(int x, *string s)>` keeping syntax but reading the names as part
of the type), enabling named calls through typed `code` values without
needing the concrete `UserClosureFunction*` at parse time. This would
generalize the v2 work (which focuses on runtime variant lookup with a
names array) to also cover the typed-callable case. Out of scope for v1
and only partially addressed by v2 as currently scoped.

## Recommendation

Implement v1 on `feature/5164_jit`. Defer v2 until concrete call sites demand
named arguments under runtime-resolved variants.

Two points worth keeping in mind that don't fit neatly into Summary or v1
Surface:

- "No new call ABI, no new IR opcode, no AOT wire-format change" is *not*
  the same as "no IR / JIT work". Source-order argument lowering and
  fast-call default handling are part of the v1 implementation effort.
- `HTTPClient(url: ...)` remains outside v1 even after the audited builtin
  allowlist exists, because `url` is an option-hash key, not a formal
  parameter. Bridging that gap is the separate `**opts` hash-spread feature
  (Future Extensions), not v1 or v2.
