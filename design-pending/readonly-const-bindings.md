# Readonly `const` Bindings and Typed Constants

**Status:** Design.

**Target:** Pending. This document captures the language and implementation
shape for runtime readonly bindings, typed parse-time constants, and a possible
later `const` method qualifier.

## Summary

Qore currently has parse-time constants:

```qore
const MyConstant = 2;
```

They have an inferred type, are only legal in namespace/class-level
declaration contexts, and their initializer is evaluated as a constant
expression. That model is useful for true constants, but it is not the right
model for local block declarations or parameters because local code usually
needs values computed at runtime.

This proposal adds a second, related language feature:

```qore
const int value = getValue();

sub f(const int id, const string name) {
    # id and name are initialized by the call and cannot be updated here
}
```

In local-variable and parameter contexts, `const` means a **readonly runtime
binding**. The binding is initialized normally, then cannot be used as the base
of any lvalue update operation.

Typed parse-time constants can be added independently:

```qore
const int MyConstant = 2;
```

At namespace/class scope this remains a parse-time constant declaration, with
an explicit declared type instead of an inferred-only type.

## Goals

- Allow explicit types on existing namespace and class constants.
- Allow `const` runtime bindings in local blocks and parameter declarations.
- Make readonly locals useful with ordinary runtime expressions, local
  variables, parameters, function calls, and control flow.
- Enforce readonly bindings consistently across AST evaluation, IR
  interpretation, JIT, AOT, closure variables, references, and lvalue helper
  paths.
- Keep the existing runtime call ABI positional. `const` parameter-ness is not
  an overload dimension.

## Non-goals

- Do not implement full transitive immutability for objects, hashes, or lists
  in v1.
- Do not make `const` methods participate in overload resolution in v1.
- Do not make a readonly local equivalent to a namespace/class constant.
  Runtime readonly bindings can depend on runtime state; parse-time constants
  cannot.

## User-facing Semantics

### Scope and disambiguation

`const` resolves by grammar production, not by post-parse analysis:

| Context                                       | Meaning                   |
| --------------------------------------------- | ------------------------- |
| Namespace top-level (incl. nested namespaces) | parse-time constant       |
| Class body (declaration scope)                | parse-time class constant |
| Statement block (incl. method bodies)         | runtime readonly binding  |
| Parameter list                                | runtime readonly binding  |
| `foreach` / `catch` variable                  | runtime readonly binding  |

The same `const Type Name = expr;` syntax is therefore parse-time at namespace
or class declaration scope, and runtime-readonly inside any statement block.
There is no ambiguity because the two contexts are reached through different
grammar productions.

### Top-level `const`

Top-level `const` declarations use the existing declaration grammar and remain
parse-time constants:

```qore
const int X = 1;          # parse-time constant
```

Top-level statement blocks use statement grammar, so a `const` inside a block is
a runtime readonly binding:

```qore
{
    const int x = getValue();  # runtime readonly binding
}
```

This keeps existing top-level constant semantics stable and avoids making
`const int X = expr;` depend on whether `expr` can be folded at parse time.

A consequence worth flagging: a top-level `const` outside any explicit
statement block is parsed as a parse-time constant declaration. If its
initializer is not a constant expression, this produces a parse error rather
than silently becoming a readonly local. To get runtime readonly semantics at
the top level, wrap the declaration in `{ ... }` or place it inside a function
body.

### Parse-time constants

Existing declarations stay valid:

```qore
const Name = expr;
```

The proposed typed form is:

```qore
const TypeName Name = expr;
```

For namespace/class constants:

- `expr` is still parsed as a constant expression.
- `TypeName` is the declared constant type.
- The initializer must be accepted by the declared type.
- The stored constant type remains `TypeName`; it is not replaced by the
  inferred initializer type.

### Readonly local bindings

In statement/local contexts:

```qore
const int x = expr;
const auto y = expr;
const z = expr;
```

The binding is initialized once and then readonly. Local `const` declarations
should require an initializer. Supporting this:

```qore
const int x;
x = 1;
```

would require a definite-assignment-once model across branches. That is a
larger feature and should not be part of v1.

The bare form `const name = expr;` is an inferred readonly declaration. After
initialization it behaves like an `auto`-typed binding, but it is not a written
type declaration. Under `%require-types`, authors must write the type
explicitly, normally:

```qore
const auto name = expr;
```

For explicit `const auto`, the initializer-derived narrowed type is stable for
the binding's lifetime. There is no later assignment that can broaden or replace
the binding's type.

Qore's existing comma-separated local declaration syntax repeats the type for
each binding:

```qore
int a = 1, int b = 2;
```

Readonly local declarations should follow that shape in v1:

```qore
const int a = 1, const int b = 2;
const int c = 3, int d = 4;
```

The `const` marker belongs to the binding declaration that carries it. In the
second example, `c` is readonly and `d` is mutable. The C-style omitted-type
form is not introduced by this feature and remains invalid unless ordinary
local declaration grammar grows it:

```qore
const int a = 1, b = 2;   # parse error
```

Parallel-assignment destructuring with `const` is not supported in v1:

```qore
(const int a, const int b) = pair;   # parse error
```

Adding it would require teaching the destructuring lvalue path about
per-target binding flags; the value/cost ratio is poor and it can be added
later if needed.

Readonly is a property of the binding, not a guarantee that the referenced
value is deeply immutable. This is illegal:

```qore
const list<int> values = [1, 2];
values = [3];
values += 3;
values[0] = 9;
remove values;
delete values;
```

`remove` and `delete` clear the binding (and run destructors), so they count
as writes to the binding and are rejected on the same grounds as assignment.

The same rule applies to any lvalue update where the readonly binding is the
base:

```qore
const hash<auto> h = {"a": 1};
h{"a"} = 2;       # illegal
h.b;              # unchanged: missing key access returns NOTHING

const object o = getObject();
o.member = 1;     # illegal if it resolves as an lvalue rooted at o
```

`const hash<auto>` remains an open untyped hash. The readonly marker does not
turn the initializer's observed keys into a closed shape, and it does not make
unknown-key lookup raise an exception. Missing key access continues to return
`NOTHING`; hashdecl remains the explicit mechanism for typed hash shapes where
referencing an undefined key is an error.

This proposal does not, by itself, prevent mutation through another alias:

```qore
list<int> mutable = [1, 2];
const list<int> ro = mutable;
mutable[0] = 9;   # still legal
```

Method calls that mutate receiver state are not generally rejected in v1 unless
they are recognized as lvalue updates. Full receiver constness is covered by
the later method-const section.

A readonly local captured by a closure remains readonly inside the closure
body. The closure variable's lvalue paths must honor the same rejection rules
as the source local.

### Parameters

Parameter syntax:

```qore
sub f(const int id, const *string name = NOTHING) {
}
```

Rules:

- The caller sees the same call ABI and the same overload set.
- `sub f(int)` and `sub f(const int)` are not distinct overloads.
- Readonly parameter-ness also does not affect abstract override matching. A
  concrete implementation may add or drop `const` relative to the abstract
  declaration because the marker is a callee-body constraint, not a caller-side
  contract.
- The parameter local is initialized from the selected positional argument and
  is readonly for the body.
- Default-argument evaluation and type filtering are unchanged.
- Reflection, documentation, AST output, and AOT metadata should preserve the
  readonly marker.

### Other local-introduction sites

The same binding flag should be available wherever Qore introduces a local
variable:

```qore
foreach const int i in values {
}

try {
} catch (const hash<ExceptionInfo> ex) {
}
```

The implementation scope should stay explicit:

| Site | Phase | Initialization model |
| ---- | ----- | -------------------- |
| Local declaration | v1 | exactly one initializer expression |
| Parameter | v1 | call setup initializes the local |
| `catch` variable | follow-on | initialized once on catch entry |
| `foreach` variable | follow-on | reinitialized once per iteration |
| Destructuring/list assignment targets | out of v1 | requires per-target binding metadata |
| Other statement-introduced variables | audit before enabling | depends on the statement lowering |

`catch` and `foreach` do not need a different binding model, but `foreach`
needs a distinct verifier/AOT exemption because each iteration rebinds the loop
variable.

### References

References are the main bypass risk. The rules cover both reference creation
and reference use.

Direct mutable reference creation is rejected:

```qore
const int x = 1;
reference<int> r = \x;    # mutable reference to readonly binding
*reference<int> r2 = \x;  # also rejected - `*reference<T>` is mutable
r = 2;
```

A readonly binding with reference type cannot be used as an update lvalue:

```qore
int x = 1;
const reference<int> r = \x;
r = 2;                    # illegal
```

The same rule applies when a readonly local is passed by reference to a
function parameter declared as `reference<T>` (or `*reference<T>`). The
parse-time check fires on the call site:

```qore
sub mutate(reference<int> r) { r = 2; }

const int x = 1;
mutate(\x);               # illegal: passing readonly local where mutable
                          # reference is required
```

Closure capture via a reference parameter inside the closure body is rejected
on the same grounds.

For v1, the conservative rules are:

- Creating any mutable reference (`reference<T>` or `*reference<T>`) to a
  readonly binding is illegal.
- Passing a readonly binding to a parameter of mutable reference type is
  illegal.
- A readonly binding with reference type cannot be used as an update lvalue.

A later `readonly reference<T>` type could represent a read-only reference
explicitly, but that is not required for useful readonly local variables.

## Current Implementation Shape

The current code already has good centralization points:

- `LocalVar` stores parse/runtime metadata for locals and parameters
  (`include/qore/intern/LocalVar.h`).
- Parameters become local variables in `UserSignature::parseInitPushLocalVars`
  (`lib/Function.cpp`).
- AST lvalue validation goes through `check_lvalue()` and `LValueOperatorNode`
  (`lib/QoreLib.cpp`, `include/qore/intern/QoreOperatorNode.h`).
- Runtime local lvalue access goes through `LocalVar::getLValue()` for normal
  locals.
- Closure variables and immediate closure values have separate lvalue paths.
- IR/JIT/AOT have direct local-store fast paths that bypass parts of
  `LValueHelper`.

This means the feature is not a parser-only change. The readonly bit must be
visible to every path that can update a local value.

## Implementation Sketch

### 1. Add readonly metadata

Add a flag to `LocalVar`:

```cpp
bool readonly = false;
```

Add accessors:

```cpp
bool isReadOnly() const;
void setReadOnly();
```

Copy constructors and any AOT/local metadata reconstruction must preserve it.

Parameters need metadata in the common signature representation, not only in
`UserSignature`. `const` parameter-ness is ignored for overload identity, but it
is visible to reflection, documentation, AST output, and AOT signature
serialization. The clean shape is either:

- a `std::vector<uint8_t> readonlyList` on `AbstractFunctionSignature`,
  aligned with `typeList` / `names` / `defaultArgList` (prefer `uint8_t` over
  `bool` so the container behaves like a normal `std::vector` for taking
  addresses, parallel iteration, and ABI-stable layout); or
- a small parameter-info struct if the signature representation is due for
  cleanup.

`UserSignature::parseInitPushLocalVars()` then passes the flag when it pushes
parameter locals. Builtin signatures may default all parameter flags to false
unless/until the C++ API grows a way to declare readonly parameters.

### 2. Parse syntax

Add grammar support for:

- Typed parse-time constants:
  - `const Type Name = expr;`
  - public/scoped/class variants mirroring existing constant rules.
- Local readonly declarations:
  - `const Type name = expr;`
  - `const name = expr;` as `auto`, if accepted.
- Parameter readonly qualifiers:
  - `const Type name`
  - `const Type name = default`
- Follow-on local-introduction sites:
  - `foreach const Type name in expr`
  - `catch (const Type name)`

The parser should treat namespace/class `const Name = expr` as the existing
parse-time constant form. In local statement contexts, `const name = expr` is
a runtime readonly binding because `const` is not currently legal there.

### 3. Type and initializer handling

For typed parse-time constants, reuse the assignment-style type flow:

- Resolve the declared type.
- Parse the initializer with `expected_type_info` set to the declared type.
- Run the normal compatibility check.
- Preserve the declared type in the `ConstantEntry` instead of overwriting it
  with the inferred initializer type during parse/runtime constant init.

For readonly locals, parse the initializer as a normal runtime expression. The
declared type participates exactly as a normal local declaration with an
initializer does today. After the initial assignment is accepted, mark the
binding as assigned and readonly.

### 4. Parse-time lvalue rejection

Extend lvalue validation to reject updates rooted at a readonly local.

`check_lvalue()` already recursively walks these shapes back to the base:

- variable reference
- list/hash indexing
- range indexing
- hash/object dereference
- casts

That is the right place to reject:

```qore
x = v
x += v
++x
x++
remove x
delete x
x[0] = v
x{"k"} = v
```

The check needs an "initial assignment is allowed" mode for declaration
initializers. Ordinary later assignments are errors.

The recommended mechanism is a distinct AST node for the declaration
initializer rather than a "currently initializing" thread-local flag on the
parser. A flag is cheap but easy to abuse — for example, the initializer
expression itself could legally reach back through a closure or argument and
mutate the binding mid-init. A dedicated initializer node localizes the
"first write" exception to the exact site that introduced the binding and
falls cleanly out of grammar dispatch.

### 5. Runtime lvalue/reference rejection

Parse-time rejection is not enough. Runtime lvalue, reference, and
reflection-style paths also need guards:

- `LocalVar::getLValue()` should reject readonly locals except for the
  declaration initializer path.
- `LocalVar::remove()` should reject readonly locals.
- `VarRefNode::getLValue()` should check readonly before dispatching to direct
  closure/immediate lvalue paths that bypass `LocalVar::getLValue()`.
- `ClosureVarValue` carries its own `readonly` flag. The flag is not
  derivable from the owning `LocalVar` because several call sites reach the
  closure value directly (the `LoadClosure` cache path is one example).
  Storing the flag on the value itself keeps every direct-access site honest
  without threading the owning `LocalVar` through every API.
- Reference creation must reject mutable references to readonly local bases.

The error should be a parse error when statically known and a runtime
exception only for paths that cannot be proven at parse time.

Compiled `StoreLocal` / `StoreClosure` fast paths are handled by the IR verifier
and AOT-load invariant in §6.1, not by adding a per-store readonly branch to the
hot path.

### 6. IR, JIT, and AOT

Enforcement is layered to keep the hot store path branch-free:

1. **Parse-time rejection** is the primary line of defense. Any program that
   reaches IR lowering with a syntactically rejectable readonly write is a
   parser bug.
2. **IR lowering** simply does not emit store/update instructions for readonly
   locals after the declaration initializer. Under correct emission the lower
   layers never see such a store.
3. **Defensive runtime checks** sit only on paths that parse-time cannot
   prove statically — primarily reference creation, reflection-driven writes,
   and any closure-aliasing escape hatch. These paths raise an exception, not
   a parse error.

Given the verifier/load-time invariant below, the IR/JIT/AOT store fast paths do
**not** need a per-store readonly check. The paths to audit for missing
emission-time gating instead:

- IR interpreter `StoreLocal` fast paths.
- IR interpreter fused local-int update instructions.
- JIT runtime helpers such as `qore_rt_assign_local()` and
  `qore_rt_assign_local_no_coerce()`.
- Direct `LocalVarValue::val.assign(...)` fast paths.
- AOT local metadata and local slot reconstruction.

### 6.1 IR validity invariant

After parse initialization and IR lowering, readonly locals must never appear as
the write target of:

- `StoreLocal`
- `StoreClosure`
- fused local update instructions such as local-int increment/add-store
- list/hash index store instructions rooted at that local
- lvalue-path store/remove/delete records rooted at that local

The IR verifier should reject such instructions. AOT loading should apply the
same check after local metadata is reconstructed, so stale or hand-built AOT
blobs cannot bypass parser checks. The only permitted write is the declaration
initializer or parameter/catch/foreach initialization path that creates the
binding.

`foreach` is a special case: each iteration rebinds the loop variable. The
per-iteration rebind counts as an initialization write rooted at the loop's
binding site, not as an update, and is therefore exempt from the verifier
rule. Lowering should emit these writes through the foreach init opcode rather
than a generic `StoreLocal` so the verifier can distinguish them.

### 6.2 AOT wire format

Readonly metadata must round-trip through AOT serialization:

- New feature flag bit: `QORE_AOT_FEAT_READONLY_LOCALS` using the currently
  next free AOT feature bit. At the time this design was written that was
  `1ULL << 18`, with bits 0-17 used per `QoreAOTBinary.h`.
- A `readonly` bit per slot in the local slot table. Three writer/reader pairs
  must be updated in lockstep — the IR-function slot map
  (`QoreAOTInstRegistry.cpp`), the AOT slot-map writer/reader
  (`QoreAOTBinary.cpp`), and the handler-IR reader (`QoreAOTRuntime.cpp`).
  Missing one is the recurring footgun on this branch.
- Parameter readonly flags are encoded feature-gated in the variant signature,
  inside the parameter loop. Both `writeVariantSignature` (writer) and
  `readVariantSignature` (reader) must be updated in lockstep, with the same
  feature-flag gating, to avoid the same out-of-sync footgun as the local-slot
  table. Recommended layout: after each parameter's type path, write a
  `u8 param_flags` where bit 0 means readonly. Older blobs omit this byte;
  readers only consume it when `QORE_AOT_FEAT_READONLY_LOCALS` is present.
  This keeps the flag physically next to the parameter metadata and avoids
  overloading the variant-level `flags u16`.
- For typed parse-time constants, `ConstantEntry` already has runtime type
  storage. The change is to preserve the explicit declared type separately from
  the initializer's inferred type, or add an explicit-declared-type marker so
  parse/runtime init does not overwrite the declared type with the initializer
  type. The AOT reader must honor the declared type during retype. This
  interacts with the recent `aotRetypeForMember` and class-constant fallback
  transplant work and must be reviewed against those code paths.

### 7. AST parser and tooling

Mirror grammar changes in `modules/astparser/src/`.

AST declarations and printer/searcher output need a readonly/const marker for:

- constant declarations with optional explicit type
- local declarations
- function/method parameters
- catch/foreach variables
- method qualifiers if method constness is added

Docs generators and reflection should expose readonly parameters without
changing call compatibility.

## `const` Methods

C++-style method constness is useful but should be separate from readonly
locals/parameters:

```qore
string getName() const {
    return name;
}
```

The v1 design is intentionally narrow: a parse-time-only, non-overload,
non-runtime-checked qualifier. Concretely:

- Add a method flag such as `OFM_CONST`.
- Allow it only on non-static normal methods.
- Do not allow it on constructors or destructors.
- Do not make it an overload dimension.
- During method parse, treat `self` as readonly.
- Reject direct member writes rooted at `self`.
- Reject calls to non-const methods on `self` when the target method can be
  resolved statically.

This is not full C++ transitive const. Full const receiver semantics would
require const-qualified object/reference types, const-aware method dispatch,
const method overload resolution, reflection/AOT representation, and runtime
aliasing rules. That should be a later design if needed.

## Compatibility

This is mostly additive because `const` is already a reserved keyword and is
currently illegal in local statement and parameter contexts.

Potential compatibility decisions:

- `%no-constant-defs` continues to gate parse-time constant definitions only.
  Runtime readonly locals are unaffected. The option's documentation should
  call this out so users are not surprised that `const int x = expr;` inside
  a function body still parses under the directive.
- `%require-types` interacts only insofar as `const` locals must follow the
  same typing rule as ordinary locals under the directive (the bare
  `const name = expr;` form is an inferred declaration and is rejected when the
  directive forbids untyped declarations; `const auto name = expr;` is explicit
  typed syntax and remains valid).
- `const` parameter markers should be preserved in reflection/docs but ignored
  for overload identity.
- Open hash lookup behavior is unchanged. In particular,
  `const hash<auto> h = {"a": 1}; h.b` still returns `NOTHING`; the
  unknown-key exception behavior remains limited to hashdecl values.
- Existing namespace/class `const Name = expr` syntax keeps its existing
  parse-time semantics.

## Error Reporting

Two new error classes:

- `READONLY-VARIABLE-ASSIGNMENT-ERROR` — raised at parse time for any update
  rooted at a readonly binding that is statically detectable (assignment,
  compound assignment, increment/decrement, `remove`, `delete`, indexed
  write, member write, mutable reference creation, mutable-reference
  parameter pass).
- `RUNTIME-READONLY-VIOLATION` — raised at runtime for paths that escaped
  parse-time analysis (reflection, indirect reference, closure aliasing).

Existing `CONSTANT-*` error codes for parse-time constants are unchanged.

## Test Plan

Add parser/runtime tests covering:

- Typed namespace constants and class constants.
- Type mismatch in typed constants.
- Local `const` with runtime initializer using params/locals/function calls.
- Reassignment, compound assignment, increment/decrement, remove/delete.
- Indexed/hash/object lvalue updates rooted at readonly locals.
- Missing-key lookup on `const hash<auto>` still returns `NOTHING`; hashdecl
  missing-key lookup still raises the existing error.
- Readonly params and defaulted readonly params.
- Abstract/concrete method matching with readonly parameters added/dropped on
  the concrete implementation.
- `%require-types` behavior: `const name = expr;` rejected and
  `const auto name = expr;` accepted.
- Comma-separated local declarations using Qore's repeated-type form:
  `const int a = 1, const int b = 2;` accepted, mixed
  `const int a = 1, int b = 2;` accepted with only `a` readonly, and
  `const int a = 1, b = 2;` rejected unless ordinary local declaration grammar
  grows omitted-type declarators.
- Closure capture of readonly locals and attempted mutation inside closures.
- Reference creation from readonly locals.
- IR, JIT, and AOT execution of readonly locals, including paths that would
  otherwise use direct store or fused increment instructions.
- Top-level disambiguation: `const int X = 1;` at file scope produces a
  parse-time constant (and a parse error if the initializer is not const-
  foldable); `{ const int x = getValue(); }` at the top level produces a
  readonly local. Easy to regress with a future grammar tweak.
- IR-verifier and AOT-load rejection: hand-built IR with `StoreLocal` /
  `StoreClosure` / fused-update / lvalue-path-store / list-or-hash index-store
  targeting a readonly slot must be rejected by the verifier and by the AOT
  loader. Foreach per-iteration rebinds must continue to be accepted.
- AST parser/printer round trips for all new syntax.

## Recommended Phasing

1. Typed namespace/class constants. Touches `ConstantEntry` storage and the
   AOT class-constant transplant/retype path, both of which have been recent
   bug surfaces — review existing fixes before extending.
2. Readonly local declarations and parameters, including parameter metadata in
   reflection/docs/AOT. Params are not complete without signature metadata
   because callers, docs, and serialized variants need to preserve the marker
   even though overload resolution ignores it. This phase also implements the
   §6.1 IR-verifier rule and the matching AOT-load check; without them the
   invariant is aspirational and stale or hand-built blobs can bypass parser
   checks.
3. Readonly foreach/catch/local-introduction sites.
4. AST parser/printer/searcher support for all accepted syntax and remaining
   local-introduction metadata. Phases 1-3 emit the minimum AST/reflection
   metadata needed for their own surface (e.g. typed-const declared type for
   docs, parameter readonly bit for reflection); Phase 4 completes coverage
   uniformly across all accepted forms and consumers.
5. Shallow `const` methods.
6. Optional deeper const/reference/type-system work.
