# Const Methods and Shallow Const Receivers

Status: Implemented. This document records the v1 language design and durable
semantics. The stable builtin annotation audit is tracked in
`design/const-methods-builtin-audit.md`.

## Summary

Qore currently supports `const` on bindings, but not on methods. A value such as
`const A a()` prevents rebinding `a`, but it does not prevent method calls that
mutate the object behind the binding:

```qore
class A {
    private int x = 0;

    int inc() {
        return ++x;
    }
}

const A a();
a.inc();  # currently allowed
```

This proposal adds a trailing `const` qualifier for methods and uses it to make
readonly object bindings useful as receivers. A `const` method promises not to
mutate `self` directly, and calls through a readonly object receiver are allowed
only when the resolved method is `const`.

The intended semantics are shallow receiver constness. This is not a full
transitive immutability system and does not make all reachable objects immutable.

## Dependency On Readonly Bindings

This design depends on the implemented readonly `const` binding baseline:

- readonly local and closure bindings must be identifiable by parse analysis
- closure capture must preserve readonly binding metadata
- lvalue validation must reject writes, removals, mutation operators, and
  mutable-reference creation rooted at readonly values
- IR and AOT verification must preserve and re-check readonly metadata across
  execution modes

Readonly const-lvalue behavior has not been released. This design therefore does
not need to preserve prerelease behavior where a readonly object binding could
call a mutating method, and it does not need a legacy AOT mode for prerelease
artifacts that encoded readonly lvalues without the final feature metadata.

The readonly-binding infrastructure needed by const methods is recorded in
`design/readonly-const-bindings.md`: readonly expression-root analysis,
reference bypass checks, IR/AOT verifier coverage, and AST/IR/JIT/AOT
cross-mode tests.

Dependency direction is one-way: const methods depend on readonly bindings, but
readonly-binding follow-ups do not depend on const methods. Receiver constness,
const method declarations, readonly receiver calls, and mutation checks rooted
at `self` are owned by this document.

## Goals

- Add syntax for const methods on normal, non-static user methods.
- Treat `self` as a readonly receiver in const method bodies.
- Reject direct mutation of `self` through statically self-rooted lvalue paths
  and receiver expressions in const method bodies.
- Reject calls to non-const methods when the receiver expression is known to be
  readonly and the method target is resolved at parse time.
- Preserve method constness in parser metadata, AST parser output,
  QoreCodeFormat output, reflection, QoreApiMetadata output, documentation, and
  AOT binaries.
- Keep method constness out of overload resolution in the first implementation.
- Keep the existing meaning of `const` bindings and constants unchanged.
- Add no per-call or per-store runtime cost for statically verified const calls
  and const method bodies across AST, IR, JIT, and AOT execution.

## Non-Goals

- No deep or transitive object immutability.
- No const-qualified object type, reference type, list type, hash type, or class
  type in the general type system.
- No C++-style const overload sets in the first implementation.
- No const-qualified `copy()` special method in the first implementation.
- No `mutable` member escape hatch in the first implementation.
- No guarantee that all dynamic calls through untyped values are rejected before
  runtime. The first implementation should enforce statically resolved calls and
  direct mutations.
- No automatic audit of *all* builtin methods for const correctness in the first
  release. A full builtin audit remains out of scope. However, because readonly
  receivers are enforced for resolved builtin method variants (see "Builtin Const
  Annotation"), the common read-only builtin methods must be annotated const
  before the feature is usable with builtin-typed readonly bindings; that subset
  is required for v1.

## Syntax

The method qualifier is placed after the parameter list and before the return
clause or body:

```qore
class Point {
    private {
        int x = 0;
        int y = 0;
    }

    int getX() const {
        return x;
    }

    getY() const returns int {
        return y;
    }

    setX(int v) {
        x = v;
    }
}
```

Abstract methods use the same position:

```qore
class Shape {
    abstract int area() const;
}
```

Out-of-line method definitions should use the same spelling:

```qore
class Point {
    int length() const;
}

int Point::length() const {
    return sqrt(x * x + y * y);
}
```

Special methods that may carry the qualifier:

```qore
class B {
    methodGate(string m) const { }   # valid
    memberGate(string m) const { }   # valid
}
```

Invalid uses in the first implementation:

```qore
class A {
    constructor() const { }     # invalid
    destructor() const { }      # invalid
    static int f() const { }    # invalid
    copy() const { }            # invalid
    memberNotification(string name) const { }  # invalid
}
```

The qualifier is intentionally trailing. `const int f()` remains ambiguous with
existing uses of `const` for constants and readonly declarations and should not
be added as method syntax.

`methodGate() const` and `memberGate() const` are valid in v1. These gates may
serve readonly receivers when the gate itself is statically resolved and marked
const. `memberNotification() const` is rejected because it is a write-notification
hook reached after member mutation, not a readonly receiver operation.

## Receiver Semantics

A readonly object receiver may call const methods:

```qore
const Point p(1, 2);
int x = p.getX();       # allowed
p.setX(3);              # parse error when resolved statically
```

A mutable receiver may call both const and non-const methods:

```qore
Point p(1, 2);
p.getX();               # allowed
p.setX(3);              # allowed
```

When the receiver type is unknown or the method call is otherwise dynamic, the
first implementation should not add a new dynamic dispatch dimension. The parser
and parse-analysis phases should reject calls only when the receiver is known to
be readonly and the target method variant is resolved. This keeps dynamic Qore
code compatible while improving statically typed code.

## Const Method Body Semantics

Inside a const method, `self` is readonly. The compiler should reject direct
lvalue operations rooted at `self`, including implicit member access:

```qore
class Counter {
    private {
        int value = 0;
    }

    int get() const {
        return value;       # allowed
    }

    int bad1() const {
        ++value;            # invalid
    }

    int bad2() const {
        self.value = 1;     # invalid
    }

    int bad3() const {
        remove value;       # invalid
    }
}
```

Method calls on a readonly receiver expression must also target const methods
when the method is resolved statically:

```qore
class Counter {
    int get() const { return 0; }
    int inc() { return 1; }

    int ok() const {
        return self.get();
    }

    int bad() const {
        return self.inc();  # invalid
    }
}
```

The receiver readonly property should propagate through member access for
statically resolved method calls:

```qore
class Holder {
    private {
        Counter c();
    }

    int ok() const {
        return c.get();
    }

    int bad() const {
        return c.inc();     # invalid when resolved statically
    }
}
```

This is still shallow constness. A const method can mutate values that are not
reached through readonly `self`, for example newly created local objects or
objects received through mutable parameters. Aliasing can still expose mutable
state through another non-readonly path.

Constness constrains only the current method's `self`. Each nested method or
constructor call has its own receiver with its own mutability, so constructing a
new object inside a const method is always permitted, and the new object is
mutable even though its constructor assigns its members. Likewise, a const
instance method may read or write non-`self` state such as class-static
variables; const restricts the receiver, not all writes.

Two further rules close obvious bypasses:

- A closure created inside a const method captures `self` as a readonly receiver.
  Self-member mutation and non-const self-method calls are rejected inside the
  closure body exactly as in the enclosing const method.
- `delete self` and deletion of a self-member are rejected in a const method,
  like any other self-rooted lvalue operation.

Because Qore objects are reference types, passing `self` to a function or another
object's method that performs a mutation is an accepted shallow-constness escape;
the const contract only governs lvalue operations and method calls whose receiver
is statically known to be readonly. If a const method needs to expose a mutable
working copy, the class should provide an ordinary const method such as
`clone() const` that constructs and returns a fresh, non-readonly object.

The special `copy()` method is not const-qualifiable in v1. Qore's copy model
creates a new object first and then executes copy methods with `self` bound to
the new object while the original object is supplied as the old-object argument.
Applying the ordinary const-method rule would make the new destination object
readonly inside the copy body, which defeats the purpose of custom copy methods.
Applying `const` to the old object instead would be a separate source-const copy
hook with different body semantics, not an ordinary const method.

## Opt-In Clone Pattern

Readonly-receiver cloning should be expressed as an ordinary `clone() const`
method when a class wants to support it. This is a naming convention and library
pattern, not a special method: it has normal method dispatch, normal override
rules, normal return typing, and ordinary const-method body checks. The name
`clone` is arbitrary and carries no special dispatch; a class may use any name,
and an inherited non-const `clone()` from a base or builtin class does not
satisfy this pattern.

There is no implicit class-hierarchy traversal for `clone() const`. If a class
hierarchy wants concrete-type cloning, each concrete subclass must explicitly
override `clone() const` and construct the correct subclass object. The subclass
implementation is responsible for invoking whatever base-class const APIs,
state-export helpers, or constructor arguments are needed to preserve inherited
state. An inherited base `clone() const` may still be callable, but it only has
the behavior and return type declared by that base method; it does not
automatically copy subclass state.

Example:

```qore
class Document {
    private {
        int id;
    }

    constructor(int id) {
        self.id = id;
    }

    int getId() const {
        return id;
    }

    object clone() const {
        return new Document(id);
    }
}

class NamedDocument inherits Document {
    private {
        string name;
    }

    constructor(int id, string name) : Document(id) {
        self.name = name;
    }

    object clone() const {
        return new NamedDocument(Document::getId(), name);
    }
}
```

Const is a receiver-mutability contract, not a purity or thread-safety claim. It
is separate from `QCF_CONSTANT`: a const method may still throw, perform I/O,
read or write non-self state, write through non-self reference arguments, and a
`synchronized` const method may acquire its object's lock. Acquiring the lock is
allowed because it is not a self-member lvalue store; `const` does not imply that
a method is side-effect-free or constant-foldable.

## Inheritance and Overrides

Constness is part of the method contract, but not part of overload selection in
the first implementation.

Recommended rules:

- Constness is not part of the variant signature or mangling, so within a single
  class two variants that differ only by `const` are impossible — they collide.
  "No overload by `const` alone" is therefore a same-class rule, not a
  cross-class one.
- Across an override boundary a same-signature derived method may tighten
  non-const to const, but must not loosen const to non-const.
- An override of a const base method must also be const.
- A const method may override a non-const base method because mutable receivers
  can call const methods.
- A non-const method must not satisfy a const abstract method, and a concrete
  override of a const abstract method must repeat `const`.
- An out-of-line method definition must repeat the same constness as its in-class
  declaration; a mismatch is a `CONST-METHOD-ERROR`.
- When a signature is inherited from multiple bases with differing constness (for
  example one `abstract int f() const` and one `abstract int f()`), the effective
  requirement is const if any base requires const. A single const override
  satisfies all of them, since a const method may override a non-const one.
- Virtual dispatch through a const base method remains safe because every
  override in the dispatch chain is also const.

Note one intentional conservatism: because resolution selects the static type's
variant, a readonly receiver typed as a base whose method is non-const is rejected
even if the runtime override is const. To allow the call, narrow the static type
to the class that declares the method const, or mark the base method const.

Example:

```qore
class Base {
    abstract int value() const;
}

class Good : Base {
    int value() const {
        return 1;
    }
}

class Bad : Base {
    int value() {           # invalid: does not satisfy const abstract method
        return 1;
    }
}
```

## Implementation Touchpoints

The implementation spans the parser, method metadata, execution modes, tooling,
reflection, API metadata, and documentation. Relevant touchpoints are:

- `lib/parser.ypp`: method modifier bits, inline and out-of-line method
  grammar, abstract method declarations, special-method validation, and
  `UserMethodVariant` construction, including const acceptance for `methodGate`
  / `memberGate` and const rejection for `copy` / `memberNotification`.
- `include/qore/intern/QoreClassIntern.h`: `MethodVariantBase`,
  `MethodVariant`, and `UserMethodVariant` metadata.
- `lib/Function.cpp` and method call expression nodes: method selection,
  receiver type handling, and parse-time call validation.
- `include/qore/intern/QoreIR.h` and IR generation code: self-member loads,
  lvalue paths, and verifier checks for invalid stores from const methods.
- `lib/JITRuntime.cpp`: runtime helpers for lvalue paths should not need new
  hot-path checks after parse and AOT verification, but they are relevant for
  validating the reachable mutation paths.
- `lib/QoreAOTBinary.cpp` and related AOT headers: method flag serialization
  and load-time verification.
- `modules/astparser/`: tree-sitter grammar/generated parser integration,
  CST search, semantic tokens, and const-method metadata exposure.
- `qlib/QoreCodeFormat/CSTTranslator.qc`: formatting and round-trip output for
  the trailing method qualifier.
- `include/qore/QoreReflection.h`, `lib/QoreReflection.cpp`, and
  `modules/reflection/src/`: public C++ reflection accessors and
  `Qore::Reflection` module exposure for method-variant constness.
- `qlib/QoreApiMetadata/` and `lib/qpp.cpp`: source, reflection, and qpp
  `.meta.json` extraction must expose const-method metadata for QLS and generated
  API documentation.
- `doxygen/lang/`: language documentation for const method declarations,
  readonly receiver calls, and shallow receiver constness.

## Parser and Metadata Changes

The core bison parser currently has method modifier bits for public/private,
static, synchronized, deprecated, final, and abstract methods. It should add a
new parser-internal method flag such as `OFM_CONST`.

The method grammar should add an optional method qualifier node after the
parameter list for inline and out-of-line method definitions. The same qualifier
position should be supported for abstract method declarations.

Runtime metadata should carry a dedicated const-method bit, for example:

- `MethodVariantBase::isConstMethod()`
- constructor parameters for `MethodVariantBase`, `MethodVariant`,
  `UserMethodVariant`, and relevant builtin method variants
- reflection and documentation accessors for the new flag

Do not reuse `QCF_CONSTANT` for this purpose, and do not add a `QCF_*`
code-flag spelling for const methods in v1. `QCF_CONSTANT` describes
constant-expression behavior and is not a receiver mutability contract.
Constness is dedicated method-variant metadata. Public C++ builtin declaration
support should expose it through explicit method APIs or options, for example
`addConstMethod()`, `addConstAbstractMethod()`, `addConstPseudoMethod()`, or an
`addMethod()` options structure with a `const_method` field. qpp should expose
the same metadata with a separate method declaration attribute such as `[const]`
or `[method_flags=CONST]`, not through `[flags=...]`.

The flag split is intentional:

| Metadata | Applies to | Contract | Relation to const methods |
| --- | --- | --- | --- |
| `QCF_CONSTANT` | functions, static methods, instance methods | Trusted effect flag for code that should have no side effects and no exceptions, and can participate in constant-expression/effect handling. | For eligible instance methods, this should normally be paired with explicit const-method metadata after audit. It is not the metadata bit used for readonly receivers. |
| `QCF_RET_VALUE_ONLY` | functions, static methods, instance methods | No side effects other than possible exceptions; return value depends only on arguments. | Does not imply const-method metadata; audit case by case. |
| const-method metadata (`MethodVariantBase::isConstMethod()`, qpp `[const]`, or equivalent builder option) | const-qualifiable non-static instance method variants only | Receiver contract: the method does not mutate `self` through statically self-rooted writable paths. | May throw, perform I/O, mutate non-self state, acquire locks, or write through non-self reference arguments. |

Because Qore core, binary module, and Qorus declared methods are under project
control, an eligible instance method that remains `QCF_CONSTANT` but is not
marked const should be exceptional and documented. Valid
`QCF_CONSTANT`-without-const-method cases are non-instance functions, static
methods, special methods that cannot be const-qualified, or a method that has no
immediate effect under the existing flag model but exposes a writable self-rooted
reference, view, or alias. In the last case, review whether `QCF_CONSTANT` is
still the right existing effect annotation.

Special method validation follows the same split: `methodGate` and `memberGate`
may carry the const-method bit, while `copy` and `memberNotification` may not.

## Call-Site Enforcement

The existing method-resolution path already has receiver type information and
method variant information for statically resolved calls. The const check should
be added after method selection:

1. Determine whether the receiver expression is readonly.
2. Resolve the target method variant normally.
3. If the receiver is readonly and the selected method variant is not const,
   raise a parse exception.

Readonly receiver sources include:

- a local or closure binding declared `const`
- `self` inside a const method
- member-access expressions rooted at readonly `self`
- other expressions already marked readonly by parse-analysis
- the implicit `self` receiver of a qualified base-class call `Base::m()` inside a
  const method; such a call is subject to the same check as `self.m()`, so
  `Base::nonConst()` from a const method is rejected while `Base::someConst()` is
  allowed. The clone pattern relies on this: `Document::getId()` is permitted
  because it is const.

Global (`our`) object bindings are not readonly receivers in v1. `our const` is
not a readonly-binding form. Receiver constness in v1 derives only from
local/closure `const` bindings, `self` in a const method, and expressions rooted
at those.

The readonly property must propagate through expressions that merely alias a
readonly value, so it cannot be dropped by a no-op rewrap:

- `cast<T>(expr)` of a readonly receiver stays readonly. A cast must not be a
  const-escape hatch.
- parenthesized expressions and the selected branch of a conditional/ternary
  whose operands are readonly stay readonly.
- a `*T`-typed (possibly-`NOTHING`) receiver that is otherwise readonly stays
  readonly; nullability does not affect the const check.

The property does not propagate through constructor calls or through
function/method return values typed as objects. Qore has no const-qualified
object type in this design, so a returned object value is treated as an ordinary
mutable value even if the callee actually returned an alias to existing state.
This is an intentional shallow-constness boundary, not a freshness guarantee.

This produces a deliberate asymmetry: a non-const call on a readonly self-member
is rejected, but the same mutation laundered through an accessor return value is
allowed, because the returned object is not a readonly receiver:

```qore
int bad()   const { return c.inc(); }       # rejected: member-rooted readonly
int sneak() const { return getC().inc(); }  # allowed: getC() return is mutable
```

Builtin method variants resolve at parse time, so they are enforced exactly like
user-method variants: a readonly receiver may call a builtin method only if that
method is annotated const. An unannotated builtin method is treated as non-const
and is therefore rejected on a readonly receiver. This is why the common
read-only builtin methods must be annotated as part of v1 (see "Builtin Const
Annotation"); otherwise readonly bindings of builtin object types could not call
any method.

Gate fallback is handled by applying the same rule to the resolved gate method.
For a statically known readonly receiver, an otherwise-unresolved method call may
dispatch through `methodGate` only if the resolved `methodGate` variant is const.
A missing-member read may dispatch through `memberGate` only if the resolved
`memberGate` variant is const. If the receiver is readonly and the relevant gate
is absent or non-const, raise `READONLY-RECEIVER-ERROR`. Fully dynamic calls where
neither the receiver class nor the gate target can be resolved remain compatible
in v1.

Dynamic calls where the target method cannot be resolved should remain
compatible in the first implementation. A later version can add dynamic runtime
checks if the language grows a first-class const object type.

## Dynamic Dispatch and Const Receivers

Const enforcement is static and binding-based, so its interaction with dynamic
dispatch has three distinct cases.

Virtual dispatch through a typed readonly receiver is checked at parse time using
the static type's resolved variant. If that variant is const, the call is allowed,
and the override rules guarantee that every runtime override in the dispatch chain
is also const, so the runtime target cannot mutate `self`. No runtime const check
is needed:

```qore
abstract class Shape {
    abstract float area() const;
}
const Shape s = getShape();   # readonly binding, static type Shape
float a = s.area();           # parse-checked against Shape::area() const; safe at runtime
```

Constness is a property of the binding, not of the object value. Once the object
escapes the readonly binding into a non-readonly binding, an untyped value, or a
plain parameter, the readonly property is gone and ordinary mutating calls are
allowed. This is the same shallow, aliasing-based boundary as readonly bindings:

```qore
const Counter c();
auto a = c;        # a is an ordinary binding holding the same object
a.inc();           # allowed: a is not readonly
```

Fully dynamic calls — an untyped receiver, a method resolved by string name,
reflection invocation, or a gate target that cannot be resolved — are not
const-checked in v1. The parser rejects a call only when the receiver is
statically known to be readonly and the target variant is resolved. This keeps
existing dynamic Qore code working.

True runtime enforcement of const dispatch through values that have lost their
binding context would require a first-class const object value type that travels
with the value. That is out of scope here and is noted in Open Questions; this
design enforces const only where it can prove the receiver readonly statically.

## Builtin Const Annotation

Because resolved builtin variants are enforced, this design adopts "Option B":
builtin receiver constness is enforced from the first release rather than being
deferred. The implementation therefore ships const annotations for the common
read-only builtin methods together with the user-method feature.

Scope and approach:

- Add dedicated public C++ const-method declaration support, not a `QCF_*` code
  flag. Prefer explicit builders/options such as `addConstMethod()`,
  `addConstAbstractMethod()`, `addConstPseudoMethod()`, or a method-options
  structure. Do not reuse `QCF_CONSTANT`; that flag asserts constant-expression
  behavior, not receiver mutability.
- Existing `QCF_CONSTANT` instance methods are audit candidates, not proof. For
  eligible normal and pseudo instance methods, audit them and set explicit
  const-method metadata unless they mutate or expose writable self-rooted state.
  Methods with writable reference parameters can still be const if the written
  value is not receiver-rooted, but their `QCF_CONSTANT` tag should be reviewed
  under the existing effect semantics. Do not infer receiver constness from
  `QCF_RET_VALUE_ONLY`.
- Because the relevant declarations in Qore core, binary modules, and Qorus are
  under project control, the normal policy is that an eligible instance method
  tagged `QCF_CONSTANT` should also be marked const after audit. Exceptions must
  be documented at the declaration site or in the annotation audit notes.
- A full audit of every builtin method is not required for v1. The required
  subset is the read-only accessors and inspectors on widely used builtin classes
  whose instances are natural readonly receivers.
- Unannotated builtin methods remain non-const and are rejected on readonly
  receivers. Marking a method const only ever admits more programs and never
  rejects previously accepted code, so the annotation pass can continue
  incrementally after v1 without compatibility risk.
- Reflection and documentation must expose builtin method constness through the
  same accessors as user methods.

Pseudo-class methods are the highest-priority part of this subset. Object members
are usually value types, so inside a const method the common operation is a
read-only pseudo-method on a readonly self-member, such as `items.size()`,
`data.keys()`, or `name.typeCode()`. These resolve at parse time like any builtin
method, so the read-only pseudo-methods on `<value>`, `<int>`, `<float>`,
`<string>`, `<list>`, `<hash>`, `<date>`, `<binary>`, and `<object>` must be
annotated const in v1; otherwise reading a value-typed member inside a const
method would be a parse error. Mutating pseudo-methods (for example the `<list>`
splice/insertion forms) inherit the ordinary non-const rejection path on a
readonly receiver, which is the intended behavior.

Priority for the initial annotation pass is pseudo-class read accessors first,
then the common value/inspection types (for example date/time, string-like,
collection wrappers, and immutable view objects). Mutating service objects such
as locks, sockets, files, and datasources generally have few or no const methods
and can be annotated last.

The v1 builtin subset is captured in a checked-in audit artifact. The audit
lists candidate methods, const decisions, and the reason for any
`QCF_CONSTANT` instance method in the v1 subset that is not marked const. The
current audit is tracked in `design/const-methods-builtin-audit.md`.
The required v1 inventory starts with read-only pseudo-methods on `<value>`,
`<int>`, `<float>`, `<string>`, `<list>`, `<hash>`, `<date>`, `<binary>`, and
`<object>`, then covers common read-only inspectors/accessors on builtin
date/time, string-like, collection wrapper, immutable view, type, and reflection
classes. A broader audit of every service-style builtin class remains an
incremental compatibility improvement, not an unresolved v1 language-design
question.

## Lvalue and IR Enforcement

Const method bodies need enforcement at the lvalue layer because member mutation
can appear in several syntactic forms:

- `member = value`
- `self.member = value`
- `++member`
- `member += value`
- `remove member`
- `delete member` and `delete self.member`
- nested lvalue paths such as `member[0] = value`
- mutable reference creation rooted at `self`, such as `\value` or
  `\self.member`, and passing a self-member where a writable `reference<T>` /
  `*reference<T>` parameter can write back
- in-place value-mutating operators applied to a self-member (`push`, `pop`,
  `shift`, `unshift`, `splice`, `extract`, `chomp`, `trim`, shift-equals, and
  regex substitution/transliteration)

The parser and parse-analysis code should mark the current method body as const.
During lvalue analysis, any assignment/removal/update whose root is readonly
`self` or an implicit self-member root should be rejected.

The IR layer already represents self/member access and lvalue paths explicitly.
IR generation should reject readonly self-member stores before code generation.
The IR verifier should also reject invalid self-member store paths in const
methods so that AOT-loaded code cannot bypass the source-parser mutation checks.

The readonly-receiver call restriction must be re-verified the same way, not just
self-member stores. A non-const call whose receiver IR is statically readonly —
`self` in a const method, a self-member-rooted receiver, or a readonly local slot
— is visible in the IR, so the verifier should reject it on AOT load. This is a
v1 requirement, not a compatibility tradeoff, because readonly const-lvalue
semantics have not been released.

A `synchronized` const method's lock enter/exit operates on the object's internal
mutex, not a declared self-member. The const verifier must treat synchronized lock
acquisition and release on `self` as exempt and must not flag them as self-member
stores; otherwise a valid `synchronized` const method would be rejected at IR or
AOT verification.

IR transformations must preserve the provenance the verifier depends on. Inlining
a const method body, copy propagation, common-subexpression elimination, and
value numbering must keep readonly-receiver and readonly-`self` markers attached
to the affected values, or the const-receiver call check must run and be recorded
before those passes. Losing provenance during optimization would either hide a
real violation (unsound) or reject valid code at AOT verification.

The optimized AST, IR, JIT, and AOT execution paths should not need a hot-path
runtime check for statically verified calls. The runtime should rely on parse
and AOT verification for normal compiled code.

## Error Classes

Mirror the error-class split already used by readonly `const` bindings:

- `CONST-METHOD-ERROR` — parse-time declaration errors: a `const` qualifier on a
  constructor, destructor, static method, copy method, or `memberNotification`
  method; a const/non-const mismatch between an out-of-line definition and its
  in-class declaration; or a mismatch between an override and a const
  abstract/base method.
- `READONLY-RECEIVER-ERROR` — parse-time rejection of a non-const method call
  through a statically readonly receiver (including `self` in a const method).
- Self-member mutation inside a const method reuses
  `READONLY-VARIABLE-ASSIGNMENT-ERROR`, since `self` is a readonly receiver and
  the violated operation is an lvalue write. Runtime escape paths that bypass
  parse-time proof raise `RUNTIME-READONLY-VIOLATION`, consistent with readonly
  bindings.

## AOT Format

AOT method serialization already stores method flags including final, abstract,
and synchronized bits. Add a new serialized method flag bit for const methods.

Recommended compatibility behavior:

- Old released AOT blobs without the const-method feature bit load with all
  methods treated as non-const and without readonly-receiver call metadata.
- New AOT blobs that use const methods or readonly const-lvalue receiver metadata
  should require feature bits or a format version that older runtimes reject
  cleanly.
- AOT loading should verify that const method metadata, readonly receiver
  metadata, and generated IR agree.
- Detectable prerelease readonly-aware artifacts missing the final feature
  metadata should be rejected as malformed; no compatibility path is required for
  unreleased const-lvalue behavior.
- Method constness is part of the published API contract. Flipping a method
  between const and non-const is a breaking change for separately compiled or
  AOT-built consumers that resolved a call against the old constness, exactly like
  a signature change, and must be caught by the existing class/method versioning
  rather than accepted silently at load.

## AST Parser, Tree-Sitter, and QoreCodeFormat

The astparser module uses the tree-sitter parser for the active public API. The
old Bison-based astparser implementation has been removed from the repository,
so const-method tooling support belongs in the tree-sitter grammar, generated
artifacts, CST search layer, and downstream consumers. This work is required,
not optional, because QoreCodeFormat and QoreApiMetadata consume the
tree-sitter CST.

Required tooling updates:

- core parser grammar in `lib/parser.ypp`
- core scanner only if an additional token is needed; the `const` keyword
  already exists
- astparser tree-sitter grammar in
  `modules/astparser/grammars/tree-sitter-qore/grammar.js` and generated parser
  artifacts under `modules/astparser/grammars/tree-sitter-qore/src/`
- CST node construction for method declarations and out-of-line definitions,
  with a trailing qualifier field after the parameter list and before `returns`
  / body
- `modules/astparser/src/CSTSearcher.*`,
  `modules/astparser/src/queries/GetNodesInfoQuery.*`, and related symbol
  search paths that expose method modifiers and method-detail metadata
- QoreCodeFormat translation and round-trip formatting for const methods in
  `qlib/QoreCodeFormat/CSTTranslator.qc` and
  `qlib/QoreCodeFormat/CodeFormatter.qc`
- syntax highlighting, semantic token, and documentation generation paths that
  consume CST method metadata

Do not model the trailing method qualifier as an ordinary leading modifier in
the CST or formatter data shape. Add a dedicated field such as
`isConstMethod` or `methodQualifiers`; appending `"const"` to the existing
leading `modifiers` string risks formatting the declaration as
`const int get()` instead of `int get() const`.

QoreCodeFormat must preserve the qualifier position:

```qore
int get() const {
    return x;
}

get() const returns int {
    return x;
}
```

## Parse Options

Const-method syntax follows ordinary method-declaration rules under the relevant
parse options:

- `%require-types` is orthogonal to method constness. The trailing `const`
  qualifier neither relaxes nor tightens the existing return-type or
  parameter-type requirements.
- `%allow-bare-refs` and old-style `$` syntax do not change the qualifier. The
  trailing `const` is spelled identically whether members are referenced bare or
  as `$.member`, and `self` is spelled `self` or `$self` per the active mode. The
  readonly-`self` body checks apply the same way in both modes.
- The qualifier is positional and uses the existing `const` keyword, so no new
  scanner token or parse option gates the feature.

## Reflection API and Module

Const-method metadata must be visible through both the public C++ reflection API
and the `Qore::Reflection` module. Reflection must expose receiver constness
separately from code flags so callers can distinguish `QCF_CONSTANT`,
`QCF_RET_VALUE_ONLY`, and const-method metadata.

Required API updates:

- Add a public C++ method-variant accessor, for example
  `QoreExternalMethodVariant::isConstMethod()`, backed by
  `MethodVariantBase::isConstMethod()`.
- Add `AbstractMethodVariant::isConstMethod()` in the reflection module.
  `NormalMethodVariant` and `PseudoMethodVariant` inherit the meaningful
  instance-method result; static, constructor, destructor, copy, and other
  non-qualifiable variants return `False`.
- Add a reflection modifier bit such as `MC_CONST` and include `"const"` in
  `AbstractVariant::getModifierList()` for const method variants. This matches
  the existing variant-level reflection style for `abstract`, `final`, `static`,
  `synchronized`, and access modifiers.
- Keep `AbstractVariant::getCodeFlags()`,
  `AbstractVariant::getCodeFlagList()`,
  `AbstractReflectionFunction::getCodeFlags()`, and
  `AbstractReflectionFunction::getCodeFlagList()` focused on existing code
  flags such as `CF_CONSTANT` and `CF_RET_VALUE_ONLY`. There is no
  `QCF_CONST_METHOD` code flag in v1; expose constness only through the
  modifier/boolean metadata APIs.
- Treat variant-level reflection as authoritative. `AbstractMethod` objects can
  contain multiple variants with different signatures, and those variants may
  differ in constness, so a method-level boolean would need explicit
  all-variants or any-variant semantics. Do not add one in v1 unless that
  aggregate meaning is documented.

Module touchpoints include `modules/reflection/src/QC_AbstractMethodVariant.qpp`
for `isConstMethod()`, `modules/reflection/src/QC_AbstractVariant.qpp` for the
modifier bit/list, `modules/reflection/src/QC_AbstractClass.qpp` and
`modules/reflection/src/AbstractReflectionObject.h` for the exported modifier
constant, and the normal/pseudo/static/special method variant classes for
documentation and inherited behavior.

Reflection exposes constness for inspection only; it does not add an enforcement
point. Reflection-based and dynamic invocation paths — `AbstractMethodVariant::call()`,
`object::callMethod()` and the call-by-name forms, and `call_object_method()` — are
not const-checked in v1, consistent with the dynamic-call compatibility stance in
"Call-Site Enforcement" and "Dynamic Dispatch and Const Receivers". A caller that
knows a method is non-const through reflection must not assume the reflection call
path will reject it on a readonly receiver.

## API Metadata

This feature also impacts the API metadata pipeline described in
`design/api-metadata-for-modules.md`. Constness is method-variant metadata and
must be available to QLS, generated documentation, and downstream metadata
consumers.

Required metadata updates:

- Add `is_const_method: bool` to `QoreApiMetadata::MethodInfo`. Absence in older
  metadata means `False`.
- Preserve the trailing `const` spelling in human-readable `signature` fields,
  but treat `is_const_method` as the authoritative machine-readable field.
- `qlib/QoreApiMetadata/AstMetadataExtractor.qc` must read constness from
  astparser method-detail metadata for `.qm` / `.qc` source extraction.
- `qlib/QoreApiMetadata/ReflectionMetadataExtractor.qc` must read constness from
  `Reflection::AbstractMethodVariant::isConstMethod()` or the `"const"` /
  `MC_CONST` variant modifier.
- `lib/qpp.cpp` must emit `is_const_method` in qpp `.meta.json` method records
  for builtin and binary module methods declared with qpp `[const]` or the
  equivalent builder/API metadata.
- `qlib/QoreApiMetadata/SymbolMerger.qc` and
  `qlib/QoreApiMetadata/QppDocIndex.qc` must preserve the field when merging
  reflection structure with AST or qpp documentation. Reflection remains
  authoritative for loaded symbols; qpp/source metadata must still carry the
  field for offline indexes and source-only metadata.
- Metadata tests must cover all three sources: astparser source extraction,
  reflection extraction, and qpp `.meta.json`.

## Documentation

Language documentation should be updated in the class/method documentation under
`doxygen/lang/` with examples covering:

- declaring const methods
- opt-in `clone() const` methods for readonly receivers, including explicit
  subclass overrides when concrete-type cloning is desired
- calling const methods through readonly object bindings
- attempted calls to non-const methods through readonly receivers
- invalid mutation of members in const methods
- inheritance and abstract method override rules
- the shallow nature of const receiver semantics
- the difference between `QCF_CONSTANT`, `QCF_RET_VALUE_ONLY`, and const-method
  metadata
- reflection API/module access through variant-level `isConstMethod()` and the
  `"const"` modifier
- API metadata exposure through `MethodInfo.is_const_method` and signatures that
  preserve trailing `const`

Release notes should describe const methods and readonly receiver enforcement as
a new feature layered on the readonly-binding work. The existing meaning of
parse-time constants is preserved. Readonly const-lvalue behavior has not been
released, so no compatibility mode is required for prerelease behavior where a
readonly object receiver could call a mutating method. The parser-level syntax
note is that a trailing `const` token in method declarations is now meaningful
and is no longer absorbed by error recovery.

## Tests

Core language tests should cover:

- accepted inline, out-of-line, and abstract const method declarations
- ordinary `clone() const` methods that construct and return fresh mutable
  objects, including explicit subclass overrides and base const-method calls
- accepted `methodGate() const` and `memberGate() const`; rejected constructor,
  destructor, static method, copy method, and `memberNotification() const`
  declarations
- duplicate method definitions that differ only by `const`
- readonly receiver calls to const and non-const methods
- mutable receiver calls to both const and non-const methods
- statically readonly receiver fallback through `methodGate` and `memberGate`,
  including rejection when the resolved gate is non-const
- read-only pseudo-method calls on value-typed self-members (`items.size()`,
  `data.keys()`) accepted; mutating pseudo-methods on readonly self-members
  rejected
- direct member mutation from const methods
- implicit and explicit `self` member mutation
- `remove`, `delete`, increment, compound assignment, in-place mutating
  operators, mutable reference creation, and nested lvalue paths rooted at `self`
- shallow-boundary cases: a self-member receiver such as `c.inc()` is rejected,
  an accessor-return receiver such as `getC().inc()` is allowed, and `new` /
  constructor member initialization remains allowed inside a const method
- closures created in a const method preserving readonly `self`
- calls from const methods to const and non-const methods on `self`
- qualified base-class calls from a const method: `Base::someConst()` accepted,
  `Base::nonConst()` rejected
- a `synchronized` const method accepted (lock enter/exit not flagged as a
  self-member store) across IR/JIT/AOT verification
- dynamic dispatch: a virtual call through a typed readonly receiver resolves to a
  const override and runs; aliasing the object into a non-readonly binding allows
  mutating calls; fully dynamic, by-name, and reflection invocations are not
  rejected in v1
- inheritance and abstract override compatibility
- AST, IR, JIT, and AOT execution modes
- AOT load rejection for invalid const method metadata
- a performance regression guard showing const methods and readonly-receiver
  calls add no measurable per-call or per-store overhead on a const-heavy
  workload, in line with the zero-runtime-cost goal

Tooling tests should cover:

- astparser output for const method declarations
- tree-sitter parse output for the same syntax
- `CSTSearcher` / `GetNodesInfoQuery` method-detail output including constness
- QoreCodeFormat round trips that keep `const` after the parameter list and
  before an optional `returns` clause
- documentation extraction of method constness

API metadata tests should cover:

- `.qm` / `.qc` source metadata from `AstMetadataExtractor` with
  `is_const_method: True` for const methods and `False` / absent for non-const
  methods
- reflection metadata from `ReflectionMetadataExtractor` carrying the same field
- qpp `.meta.json` output from `lib/qpp.cpp` for qpp `[const]` builtin methods
- `SymbolMerger` preserving `is_const_method` while applying AST/qpp
  documentation to reflection-provided structure
- older metadata without `is_const_method` being treated as non-const by
  consumers

Reflection tests should cover:

- `Qore::Reflection::AbstractMethodVariant::isConstMethod()` for user normal
  methods, builtin normal methods, pseudo-methods, static methods, constructors,
  destructors, and copy methods
- `AbstractVariant::getModifierList()` and `getModifiers()` exposing `"const"`
  / `MC_CONST` only for const method variants
- `AbstractVariant::getCodeFlagList()` and `getCodeFlags()` continuing to expose
  `CF_CONSTANT` and `CF_RET_VALUE_ONLY` without conflating them with const-method
  metadata
- builtin annotation audit cases where a `QCF_CONSTANT` instance method is also
  reflected as const, and a `QCF_RET_VALUE_ONLY` method is not automatically
  reflected as const without explicit annotation
- mixed method overloads with different signatures where one variant is const
  and another is non-const, proving variant-level reflection is authoritative

## Implementation Phasing

The work splits into phases with hard dependencies; later phases assume the
earlier ones. Each phase has a usable end state.

- **Phase 1 — usable core (user classes).** Syntax and the `OFM_CONST` parser
  flag, `MethodVariantBase::isConstMethod()` metadata, const method body mutation
  checks, and call-site readonly-receiver enforcement for statically resolved user
  methods. Special-method validation (`methodGate`/`memberGate` accept,
  `copy`/`memberNotification`/constructor/destructor/static reject) lands here.
  End state: const methods work end-to-end on user code in the AST interpreter.
- **Phase 2 — builtins usable.** Pseudo-class and common read-only builtin
  annotation via const-method builder/qpp metadata, prioritizing pseudo-methods.
  End state: readonly bindings of value-typed members and common builtin types
  are usable. This phase is the gate for the feature being practical, not just
  expressible, and it includes the checked-in builtin audit artifact described in
  "Builtin Const Annotation".
- **Phase 3 — compiled paths.** IR generation/verifier rejection of readonly
  self-member stores and readonly-receiver calls, provenance preservation across
  optimization passes, the synchronized-lock exemption, and the AOT method flag
  plus load-time verification. Depends on Phase 1. End state: JIT and AOT enforce
  the same invariants as the parser.
- **Phase 4 — tooling and ecosystem.** astparser tree-sitter grammar,
  CST/QoreCodeFormat round-trip, reflection API/module exposure, QoreApiMetadata
  extraction/merge/qpp, and documentation. Largely parallelizable once Phase 1
  fixes the metadata shape; the tree-sitter work is a hard dependency for
  QoreCodeFormat and QoreApiMetadata.

A minimal shippable increment is Phase 1 plus Phase 2: const methods usable on
both user and common builtin/value-typed receivers, enforced in the AST
interpreter. Compiled-path verification (Phase 3) is required before the feature
is enabled in AOT builds. A public release must either include Phase 3 for every
compiled execution path exposed by that release or gate const-method syntax /
compiled execution so unsupported paths reject const-method programs cleanly.
There must be no released mode where const-method source is accepted and then
executes through an unverified compiled path.

## Implementation Work Plan

The implementation should land in focused commits so each layer can be reviewed,
tested, and reverted independently if needed. Each commit must include tests for
the behavior it enables, and the audit in "Builtin Const Annotation" must be
checked in before builtin const annotations are considered complete.

Recommended commit sequence:

1. **Design and planning update.** Finalize this work plan and the related
   `design/api-metadata-for-modules.md` metadata notes. Verification: markdown
   review and `git diff --check`.
2. **Core method metadata.** Add parser-internal `OFM_CONST`, a dedicated
   `MethodVariantBase::isConstMethod()` bit, constructor plumbing for user and
   builtin method variants, and no public `QCF_CONST_METHOD` code flag.
   Verification: compile-only tests plus reflection-internal assertions where
   available.
3. **Core syntax.** Parse trailing `const` for inline, out-of-line, and abstract
   method declarations; reject it for constructors, destructors, static methods,
   `copy()`, and `memberNotification()` while accepting it for `methodGate()` and
   `memberGate()`. Verification: parser tests for accepted and rejected syntax.
4. **Override and declaration validation.** Enforce const/non-const compatibility
   across out-of-line definitions, abstract methods, and overrides, including
   multiple-base effective-const requirements. Verification: class hierarchy
   tests with accepted const tightening and rejected const loosening.
5. **Readonly receiver call checks.** Use existing readonly-binding analysis to
   reject statically resolved non-const calls through readonly local/closure
   bindings, readonly `self`, readonly self-member roots, and qualified base
   calls from const methods. Verification: AST interpreter tests for readonly and
   mutable receivers, dynamic-call compatibility, and gate fallback behavior.
6. **Const method body checks.** Treat `self` as readonly while parsing const
   method bodies, including closures created inside them. Reject self-rooted
   lvalue writes, removals, deletion, mutation operators, nested lvalue paths,
   and writable reference creation. Verification: negative tests for every
   lvalue form listed in "Lvalue and IR Enforcement" and positive tests for
   local-object mutation and non-self writes.
7. **Builtin declaration API and qpp support.** Add explicit const-method builder
   support such as `addConstMethod()` / options or qpp `[const]`, independent of
   `[flags=...]`. Verification: generated qpp output and compile tests proving
   `QCF_CONSTANT` and const-method metadata remain separate.
8. **Builtin annotation audit and v1 annotations.** Check in the audit artifact,
   annotate the required pseudo-method and common read-only builtin subset, and
   document any `QCF_CONSTANT` instance-method exceptions. Verification:
   readonly self-member pseudo-method tests such as `items.size()` and
   `data.keys()`, plus reflection tests for annotated builtins.
9. **IR/JIT/AOT enforcement.** Serialize const-method metadata, preserve readonly
   receiver provenance in IR, reject invalid self-rooted stores and non-const
   readonly-receiver calls in the IR/AOT verifier, and exempt synchronized lock
   enter/exit. Verification: AST, IR, JIT, and AOT tests over the same const
   method corpus, including malformed AOT rejection.
10. **astparser tree-sitter tooling.** Update the tree-sitter grammar/generated
    artifacts and expose the trailing qualifier as method-detail metadata rather
    than a leading modifier.
    Verification: astparser CST and symbol metadata tests.
11. **QoreCodeFormat.** Preserve trailing `const` during CST translation and
    round-trip formatting before optional `returns` clauses and bodies.
    Verification: formatter round-trip tests for inline, abstract, and
    out-of-line declarations.
12. **Reflection and API metadata.** Add C++ and module reflection accessors,
    variant modifier metadata, `QoreApiMetadata::MethodInfo.is_const_method`,
    extractor/merger/qpp `.meta.json` support, and tests for source,
    reflection, and qpp metadata. Verification: reflection module tests and API
    metadata tests from all three sources.
13. **Language docs and release notes.** Document syntax, shallow semantics,
    clone pattern, override rules, builtin annotation behavior, reflection/API
    metadata exposure, and release gating. Verification: docs build if
    available.
14. **Final regression and performance pass.** Run the focused const-method
    suites, the relevant existing readonly-binding suites, AOT/JIT coverage, a
    full build, valgrind over the new runtime tests, and a const-heavy benchmark
    or regression guard to confirm there is no hot-path runtime check.

Dependency gates:

- Commit 2 must land before all code that needs variant metadata.
- Commits 3 through 6 form the minimum user-code AST interpreter feature.
- Commit 8 depends on commit 7 and is required before readonly builtin/value
  receivers are useful.
- Commit 9 is required before const-method syntax can be exposed in released
  compiled execution paths.
- Commits 10 and 11 are ordered because QoreCodeFormat consumes tree-sitter CST
  data.
- Commit 12 depends on the final metadata shape from commits 2, 7, and 10.

Per-commit verification should include `git diff --check`, a targeted build, and
the focused tests for the changed layer. Before every commit, run the repository
audit checklist against the uncommitted diff and fix any failures.

## Estimated Effort

The shallow const receiver design is a medium-to-large language feature.

Estimated implementation effort:

- Core syntax and metadata: 2 to 4 days
- Method body mutation checks: 4 to 7 days
- Readonly receiver call checks: 3 to 6 days
- Inheritance and abstract override rules: 2 to 4 days
- AOT and IR verification support: 3 to 6 days
- Shared readonly-binding infrastructure hardening, if not already complete:
  1 to 3 days
- Builtin const annotation (required v1 subset): 2 to 4 days
- Reflection C++ API, reflection module exposure, and reflection tests: 1 to 2
  days
- QoreApiMetadata schema/extractor/merger/qpp metadata support: 1 to 2 days
- astparser, tree-sitter, QoreCodeFormat, and docs: 4 to 8 days
- Tests, module audit, and regression work: 4 to 8 days

Total estimate: the itemized work sums to roughly 6 to 10 focused engineering
weeks for a single engineer (about 27 to 54 days), including the required v1
builtin annotation subset and assuming no major parser or IR surprises. The lower
end of the calendar range is achievable only if independent workstreams
(reflection, API metadata, tooling/tree-sitter) are parallelized across more than
one engineer.

A full const type system with const object/reference types, deep constness,
dynamic runtime enforcement, const overloads, and complete builtin annotation is
a separate larger project. That should be estimated in months, not weeks.

## Open Questions

- Should dynamic method calls on readonly bindings remain allowed permanently,
  or should they become runtime errors in a later version?
- (Settled for v1) Member access rooted at readonly `self` makes the receiver
  readonly for statically resolved method calls, applied uniformly to any
  self-member-rooted receiver (see the `Holder` example and Call-Site
  Enforcement). The only residual question is whether later versions bound how
  deep this propagates through chained member access.
- Should a later source-const copy hook be added for readonly receivers? Such a
  design would need special `copy()` semantics where `self` remains the mutable
  destination object and the old-object argument is readonly. It would also need
  explicit rules for mixed class hierarchies where some classes define only
  ordinary `copy()` and others define a source-const copy hook.
