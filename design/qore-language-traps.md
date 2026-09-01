# Qore Language Traps

## Status

Implemented behaviour, verified against `build/qore` at Qore 3.0.0. This file is a working reference
for coding agents and for anyone debugging a failure whose cause is a correct-but-surprising Qore
behaviour. The user-facing versions of these entries live in `doxygen/lang/258_common_pitfalls.dox.tmpl`
and in the reference sections it links to; this file exists because agents search by *error text*,
not by topic.

**Every entry below was run.** Do not add an entry from memory or from another document — see
[Disproved claims](#disproved-claims) for three widely-repeated "facts" about Qore that are false
and would have been documented as true if they had not been re-run.

## Index by error text

| Error or symptom | Trap |
|---|---|
| `RUNTIME-TYPE-ERROR: <lvalue> expects type 'X', but got no value instead` | [1](#1-a-declaration-does-not-initialize), [9](#9-map-over-nothing-returns-nothing) |
| `PARSE-TYPE-ERROR: ... the container's element type was inferred from an earlier assignment` | [2](#2-container-types-are-narrowed-by-their-first-value) |
| `PARSE-TYPE-ERROR: cannot append a value with type 'X' to a list with element type 'Y'` | [2](#2-container-types-are-narrowed-by-their-first-value) |
| `RUNTIME-TYPE-ERROR: ... this type is the declared value type of the hash holding the target` | [2](#2-container-types-are-narrowed-by-their-first-value) |
| `PARSE-EXCEPTION: 'C()' names class 'C', not a function` | [11](#11-constructing-an-object-in-an-expression-requires-new) |
| `PARSE-EXCEPTION: read-only local variable 'X' declared without type information` | [12](#12-a-read-only-local-needs-an-explicit-type) |
| `PARSE-EXCEPTION: hard keyword 'enum' cannot be used after '.'` | [10](#10-identifiers-that-collide-with-keywords) |
| a lookup always reports "not found"; a loop over an inherited member iterates zero times | [6](#6-privateinternal-is-invisible-to-subclasses-silently) |
| a method override is ignored, with no error | [7](#7-the-first-inherits-branch-wins) |
| a declared capability/option key reads as absent | [5](#5-presence-emptiness-and-absence-are-three-different-things) |
| a remote call exceeds an interval limit only in some time zones | [8](#8-days-and-months-are-calendar-units) |
| a character loop truncates non-ASCII text or reads past the end | [13](#13-size-counts-bytes-length-counts-characters) |
| `OBJECT-ALREADY-DELETED: cannot call a method on an object that has already been deleted`, thrown in a `background` thread | [16](#16-background-keeps-an-object-alive-but-a-method-call-reference-does-not) |
| `OBJECT-ALREADY-DELETED: attempt to access member 'X' of an already-deleted object of class 'C'`, thrown in a `background` thread | [16](#16-background-keeps-an-object-alive-but-a-method-call-reference-does-not) |

## Parser and tool feedback

The parser reports traps only when it can identify the exact construct and give advice that is valid for that
construct. Provable failures are errors; cases that can be intentional use the opt-in `language-traps` strict
warning (`%enable-warning language-traps`, `--enable-warning language-traps`, `%strict-warnings`, or
`WARN_LANGUAGE_TRAPS` in the embedding API). The warning is deliberately excluded from `WARN_ALL` and the
default `%modern` warning set.

Every targeted diagnostic has a stable semantic `id`, a plain-language `hint`, machine-readable `facts`, and
one or more `suggestions`. Exact source rewrites are marked `machine-applicable`; advice requiring a choice is
marked `review-required`. This lets an editor or coding agent act on the diagnostic without matching mutable
exception prose.

| Diagnostic ID | Severity and exact detection boundary | Primary correction |
|---|---|---|
| `UNINITIALIZED-LOCAL-READ` | strict warning for a direct read of a non-optional local that is not definitely assigned; optional locals, `exists` operands, declaration sites, assignment targets, and reads after all control-flow branches assign are excluded | initialize before the read or make absence explicit in the type |
| `NARROWED-CONTAINER-TYPE-MISMATCH` | error only when the parser can trace an `auto` container's effective element type to a narrowing assignment | use `auto!` only for an intentionally heterogeneous container |
| `NULL-NOTHING-COMPARISON` | strict warning for a parse-time-known `NULL`/`NOTHING` comparison with `==`, `!=`, `===`, or `!==` | test the two states separately according to the data contract |
| `HASH-MEMBER-TRUTHINESS` | strict warning when a statically container-valued hash member is converted directly to bool by `if`, loop, ternary, `!`, `&&`, or `\|\|`; scalar members and members passed through another expression are excluded | use `hasKey()` if presence is intended |
| `OPTIONAL-MAP-RESULT` | strict warning for a direct `map` result whose source's static type permits `NOTHING` assigned to a required list; optional and explicitly coalesced targets are excluded, and the wording does not claim that a runtime guard is absent | confirm the source is non-`NOTHING` on every path, coalesce with `?? ()`, or make the target optional |
| `HARD-KEYWORD-MEMBER-ACCESS` | syntax error only for a hard keyword after `.` | apply the exact quoted-subscript replacement |
| `MISSING-NEW-CONSTRUCTOR` | error only after callable lookup fails and the same reachable name resolves to a class | apply the exact `new ` insertion |
| `READONLY-LOCAL-REQUIRES-TYPE` | error for an untyped read-only local when types are required | write `const auto NAME` or a concrete type |
| `STRING-MULTIPLICATION-IS-NUMERIC` | strict warning when exactly one `*` operand is a parse-time-known string and the other is statically numeric | call `strmul()` |
| `EXISTS-OPTIONAL-REFERENCE-VALUE` | strict warning for `exists` applied directly to an optional local reference | return `{found, value}` or pass a separate supplied flag |

---

## 1. A declaration does not initialize

A type restriction constrains what a variable may hold; it does not give it a value.

```qore
int i;
printf("%y %y\n", exists i, i.type());   # False "nothing"
```

Most reads appear to work, because `NOTHING` converts to `0`/`""`/`False` as the context requires —
`i == 0` is `True` and `i + 1` is `1`. The failure arrives only where the declared type is actually
required, and it is a **runtime** error, so it survives review and appears only on the path that
reads the unassigned variable:

```qore
int i;
int j = i;   # RUNTIME-TYPE-ERROR: <lvalue> expects type 'int', but got no value instead
```

**Do:** initialize in the declaration (`int i = 0;`, `hash<auto> h = {};`) when the variable is read
before it is unconditionally set, or declare it `*int` to say that "no value" is expected. Test with
`exists`, never with `== 0` / `== ""` / a truth test.

The opt-in `UNINITIALIZED-LOCAL-READ` warning uses definite-assignment state merged across control-flow
branches. It does not warn for optional declarations or deliberate `exists` checks.

## 2. Container types are narrowed by their first value

`hash<auto>` and `list<auto>` accept any hash or list and then **keep** the concrete type they were
given. This applies to a literal *and* to a narrowly-typed value copied in:

```qore
hash<auto> h = {"x": 1};      # h is hash<string, int>
h.y = "string";               # PARSE-TYPE-ERROR

hash<string, int> src = {"a": 1};
hash<auto> rv = src;          # rv is hash<string, int>
rv.k = 2020-01-01Z;           # PARSE-TYPE-ERROR
```

The full message names the fix itself:

```
PARSE-TYPE-ERROR: lvalue for assignment operator '=' expects type '*int', but right-hand side is
type 'date'; the container's element type was inferred from an earlier assignment; to use mixed types,
include values of all needed types in that assignment, or use hash<auto!> or list<auto!> to
disable type narrowing for the variable
```

Three things make this bite unpredictably:

- a **heterogeneous** literal (`{"code": 1, "msg": "x"}`) infers `hash<auto>` and never narrows, so
  the same pattern works in one place and fails in another;
- typed lists enforce at parse time too — `list<int> l = (1,2); l += "x";` is
  `PARSE-TYPE-ERROR: cannot append a value with type 'string' to a list with element type 'int'`;
- when the value crosses a function boundary the parser cannot see it, and the same defect surfaces
  at run time instead. Runtime code can identify the holding container but cannot always recover its
  source declaration, so its message deliberately does not guess that `auto!` is appropriate:
  `RUNTIME-TYPE-ERROR: <lvalue> expects type 'string', but got type 'hash<string, int>' instead; this
  type is the declared value type of the hash holding the target; review the container declaration
  and the value being assigned`.

The structured diagnostic ID is `NARROWED-CONTAINER-TYPE-MISMATCH` only when the parser can prove
that the target variable has an inferred container type and can identify the assignment that established
it. Explicitly typed targets use `INCOMPATIBLE-ASSIGNMENT-TYPE` or
`INCOMPATIBLE-LIST-ELEMENT-TYPE` and never receive `auto!` guidance.

**Do:** declare `hash<auto!>` / `list<auto!>` when the container genuinely holds mixed types, and say
in a comment why it is heterogeneous. Hash addition (`h + {...}`) does widen and is tempting as a
fix, but it works around the type system rather than declaring intent; `auto!` is the language's
actual construct for "this container is heterogeneous" and nothing becomes untyped as a result.

## 3. NULL is not NOTHING

`NULL` is the SQL null **value**; `NOTHING` is the absence of a value.

```qore
printf("%y %y %y\n", NULL === NOTHING, NULL == NOTHING, exists NULL);   # False False True
```

`exists NULL` being `True` is the sharp edge: `exists` does not distinguish them. A database column
or a JSON `null` round-trips as `NULL`, and code that treats "no value" and "null value" as the same
thing will silently write one where the other is meant.

Comparisons where both values are known at parse time receive the opt-in `NULL-NOTHING-COMPARISON` warning.
Comparisons involving variables are not diagnosed because the parser cannot infer the intended data contract.

## 4. Hash addition widens; assignment does not

Following from trap 2, a helper that takes a narrowly-typed hash and must add a differently-typed key
cannot do it by assignment, but can by addition:

```qore
hash<string, int> h = {"a": 1};
hash<auto> bad = h;  bad.k = 2020-01-01Z;      # PARSE-TYPE-ERROR
hash<auto> ok  = h + {"k": 2020-01-01Z};       # works; ok.k is a date
```

Prefer `auto!` on the declaration when the container is genuinely heterogeneous; reach for `+` only
where a single expression must tolerate a narrowly-typed argument.

## 5. Presence, emptiness, and absence are three different things

Reading a missing key returns `NOTHING`, and an empty hash or list is `False`, so a truth test cannot
tell a missing key from a present-but-empty one:

| key state | `h.k` truth test | `exists h.k` | `h.hasKey("k")` |
|---|---|---|---|
| present, non-empty value | True | True | True |
| present, empty hash or list | **False** | True | True |
| present, value is `NOTHING` | False | **False** | True |
| absent | False | False | False |

This is the common failure in protocol code, where a capability is declared by the *presence* of a
key whose value is an often-empty settings hash:

```qore
if (caps.elicitation)          { ... }   # wrong: an empty settings hash reads as absent
if (caps.hasKey("elicitation")) { ... }  # right
```

The opt-in `HASH-MEMBER-TRUTHINESS` warning covers direct condition tests when the member is statically a hash
or list, including direct `!`, `&&`, and `||` operands. It remains quiet for scalar members, explicit
`hasKey()` or non-empty checks, and values passed to another expression because those cases do not establish
that member truthiness itself carries the condition.

## 6. `private:internal` is invisible to subclasses, silently

In Qore, `private` restricts access to the class *hierarchy* — it is what C++ calls `protected`, and
a `private` method can be overridden by a subclass. `private:internal` is the genuinely private form.

A subclass that references a `private:internal` parent member gets **no parse error**: the name does
not resolve to that member, so the code runs and does nothing.

```qore
class B { private:internal { hash<auto> m = {"a": 1}; } }
class D inherits B {
    int cnt() { int c = 0; foreach hash<auto> i in (m.pairIterator()) { ++c; } return c; }
}
# (new D()).cnt() returns 0, while the parent's m has 1 entry
```

Symptom: a lookup that always reports "not found", or a loop that never executes. Neither
`--enable-debug` nor any warning surfaces it.

## 7. The first `inherits` branch wins

A base class appearing more than once in a hierarchy is instantiated once (C++ virtual-base
semantics). When two branches each supply the same method, the branch listed **first** wins:

```qore
class Base  { private string hook() { return "a"; } string call() { return hook(); } }
class Leaf  inherits Base { string doIt() { return call(); } }
class SpBase inherits Base { private string hook() { return "b"; } }

class A inherits Leaf, SpBase {}   # (new A()).doIt() == "a"
class B inherits SpBase, Leaf {}   # (new B()).doIt() == "b"
```

This is what lets a module reuse another module's leaf classes wholesale while overriding a shared
hook once in a policy base: **list the policy base first.** Getting the un-overridden behaviour means
the `inherits` clause is in the wrong order — it is not a sign that the override is unsupported.

Related and non-obvious in the other direction: assigning a derived hashdecl into a base-typed slot
**preserves** the derived members rather than slicing them, and `cast<hash<Derived>>()` recovers
them, so a `list<hash<BaseNode>>` is a sound way to model a polymorphic collection.

## 8. Days and months are calendar units

Year, month and **day** components of a relative date are calendar arithmetic: they preserve the
wall-clock time, so the elapsed time they represent varies. Hour, minute and second components are
absolute.

```qore
TimeZone tz("Europe/Prague");
date v = tz.date("20260725090739");
v + 92D     # 2026-10-25 09:07:39 +01:00   <- same wall clock, 2209 hours later
v + 2208h   # 2026-10-25 08:07:39 +01:00   <- exactly 2208 hours
```

**Do:** express any timeout, retry interval, or window checked against a service's maximum-interval
limit in hours or seconds, never in `D` or `M`. A limit written as `3M` intermittently exceeds a
fixed 92-day server-side cap — but only in DST-observing zones, which reads like a flaky external
dependency and is fully deterministic.

A date literal with a fixed offset (`2026-07-25T09:07:39+02:00`) carries no zone rules and shows no
DST effect, so a test built from literals will not reproduce what `now()` does. Build the date from a
`TimeZone` object so it reproduces in any CI time zone.

## 9. `map` over NOTHING returns NOTHING

A `NOTHING` source returns `NOTHING` and the map expression is never evaluated; an **empty list**
source returns an empty list. The two differ:

```qore
*list<int> src;                 # no value
map $1, src;                    # NOTHING
map $1, ();                     # () - an empty list
```

Because `NOTHING` does not satisfy `list<T>`, this fails exactly on the input that produces no
elements — typically an optional argument that was not supplied:

```qore
list<string> rv = map string($1), src;
# RUNTIME-TYPE-ERROR: <lvalue> expects type 'list<string>', but got no value instead
```

**Do:** coerce with `?? ()` when the source may be `NOTHING`:
`list<string> rv = (map string($1), src) ?? ();`

When the optional source type and required assignment target are both visible, the opt-in strict warning reports
`OPTIONAL-MAP-RESULT` before execution. The parser does not claim that an enclosing runtime guard is absent, so
the diagnostic asks the user to confirm control flow as one valid resolution. An optional target or explicit
coalescing is accepted without this diagnostic.

## 10. Identifiers that collide with keywords

Soft keywords (`sub`, `elements`, `keys`, `context`, `final`, …) are usable as hash member names after
the `.` operator. `enum` is a **hard** keyword and is not usable in any identifier position:

```qore
h.enum       # PARSE-EXCEPTION
h{"enum"}    # fine
```

This bites when mirroring an external field name into Qore — a JSON Schema `enum` field, OpenID
Connect's `sub`. The parser reports `HARD-KEYWORD-MEMBER-ACCESS` on the exact `.enum` span and
provides a machine-applicable replacement with `{"enum"}`; other syntax errors retain the ordinary
parser diagnostic.

**Do:** write any externally-derived key that could collide as a quoted subscript.

Note that `private`, `public`, `static` and `protected` are **not** reserved as member names; they
are contextual keywords only.

## 11. Constructing an object in an expression requires `new`

`C()` in expression position parses as a call to a function named `C`:

```qore
class C { string f() { return "v"; } }
C().f();          # PARSE-EXCEPTION: 'C()' names class 'C', not a function
(new C()).f();    # correct
```

The structured diagnostic ID is `MISSING-NEW-CONSTRUCTOR`; it includes the resolved class name and
a machine-applicable insertion of `new `. This check also applies to qualified names such as
`Ns::C()` and does not fire if a reachable function with the same name exists.

Separately: never take a call reference on a temporary object
(`\obj.getChild("x").doRequest()`) — the temporary is released before the call runs and you get
`OBJECT-ALREADY-DELETED`. Bind the receiver to a local first. This matters most in `assertThrows()`,
where the whole call is written as a reference.
The same weak reference is what breaks a `background` thread reached through a call reference; see
[trap 16](#16-background-keeps-an-object-alive-but-a-method-call-reference-does-not).

## 12. A read-only local needs an explicit type

```qore
const X = "v";          # PARSE-EXCEPTION: read-only local variable 'X' declared without type
                        # information; use 'const auto X' or another explicit type
const string X = "v";   # correct
```

The error text already names the fix; it is listed here only because the same declaration is legal at
namespace scope, so the failure looks arbitrary.

The structured diagnostic ID is `READONLY-LOCAL-REQUIRES-TYPE`; its suggested `const auto NAME` form is
review-required because a concrete type can be the better correction.

## 13. `size()` counts bytes; `length()` counts characters

Bounding a character loop with `size()` truncates multi-byte text and reads past the end, yielding
empty strings. Use `length()` with `substr()`.

Two more standard-library signatures worth checking rather than assuming:

- `split("", str)` does **not** split a string into characters; it returns a one-element list holding
  the whole string.
- `split()`'s third positional parameter is `with_separator`, not `keep_trailing_empty` — the
  signature is `split(sep, str, with_separator = False, keep_trailing_empty = False)`, so
  `split("/", "a/b/c", True)` returns `("a/", "b/", "c")` with the separators still attached.

## 14. Arithmetic surprises

- `int / int` is integer division truncating **toward zero**: `7 / 2` is `3`, `-7 / 2` is `-3`.
  Flooring a negative quotient needs an explicit correction, and there is no `div` operator.
- `"0" * 64` is numeric multiplication and evaluates to the integer `0`; it does not repeat the
  string. Use `strmul()`. A string value known at parse time multiplied by a statically numeric expression
  receives the opt-in `STRING-MULTIPLICATION-IS-NUMERIC` warning; dynamic operands are not guessed at.
- `float()` **saturates** at the largest finite double rather than overflowing to infinity:
  `float("2e+308")` is `1.7976931348623157e308`. Any shortest-round-trip search over float formatting
  must guard against this explicitly.

## 15. `exists` on a `*reference` parameter tests the referenced value

```qore
sub t(*reference<int> r) { printf("%y\n", exists r); }
int a = 5;  t(\a);   # True
int b;      t(\b);   # False  <- a reference WAS supplied
            t();     # False
```

`exists r` cannot distinguish "no reference supplied" from "reference to an unassigned variable", so
it cannot be used to detect whether the caller passed one. Return a `{found, value}` hash, or use a
separate flag parameter, when the distinction matters.

Direct checks of an optional local reference receive the opt-in `EXISTS-OPTIONAL-REFERENCE-VALUE` warning.

---

## 16. `background` keeps an object alive, but a method call reference does not

```qore
class T {
    public { string name; }
    constructor(string n) { name = n; }
    destructor() { printf("destroyed %y\n", name); }
    work() { usleep(250ms); printf("still alive: %y\n", name); }

    startGood() { background work(); }
    startBad()  { code c = \work(); background c(); }
}

(new T("good")).startGood();   # still alive: "good"   <- then destroyed
(new T("bad")).startBad();     # destroyed "bad"       <- then, in the new thread:
# OBJECT-ALREADY-DELETED: cannot call a method on an object that has already been deleted
```

`background` extends the object's lifetime for every form that names the object directly, including
from a constructor — an object created only to start a thread (`(new T()).startGood();`, or a `background`
call in `T::constructor()`) survives until that thread returns. It does **not** extend the lifetime
when the thread reaches the object through a **call reference**: `\method()` and `\obj.method()` hold
only a weak reference to their object by design, so the object is destroyed on schedule and the thread
fails on its first member or method access.

This is the same weak reference that breaks a call reference taken on a temporary receiver — see
[trap 11](#11-constructing-an-object-in-an-expression-requires-new) — reached here on a different path.

| Background expression | Object kept alive | Why |
|---|---|---|
| `background method()`, `background self.method()` | yes | a non-static self call takes a real reference on the object for the thread's duration |
| `background sub () { ... }()` capturing object scope | yes | captured object scope is checked and referenced before the thread starts |
| `background obj.method()`, `background (new T()).method()` | yes | the object expression is evaluated in the *starting* thread; the result is a strong reference held by the copied expression |
| `background f(self)`, `background T::sm(self)` | yes | `self` is an ordinary strong reference in the evaluated argument list |
| `background c()` where `c` is `\method()` or `\obj.method()` | **no** | a method call reference holds a weak reference only |
| `background call_function_args(\method(), args)` | **no** | same — the call reference is the only link to the object |
| any of the above while another thread runs `delete obj` | **no** | `delete` destroys the object unconditionally |

`background T::staticMethod()` correctly takes no reference: a static call has no object.

The failure is timing-dependent — it appears only when the object's last ordinary reference goes away
before the thread finishes, so a test that keeps the object in scope passes and production fails. When a
thread must reach an object through a call reference, keep an ordinary reference to the object alive for at
least as long as the thread, or pass the object itself as an argument.

---

## Disproved claims

These were carried in working notes as facts and are **false** at Qore 3.0.0. They are recorded so
they are not re-introduced.

| Claim | Reality |
|---|---|
| "`map` over an empty source yields `NOTHING`" | Only a `NOTHING` or empty-**hash** source does. An empty **list** yields an empty list — see trap 9 for the accurate rule. |
| "`protected` is a reserved word" | It is not. `h.protected` parses fine, as do `private`, `public` and `static` as member names. |
| "`Foo::get().bar()` is a parse error; assign to a temporary first" | Calling a method on a static-method result works. The real trap is the *bare constructor* form — see trap 11. |

## Verifying an entry

Run the snippet before trusting it:

```
LD_LIBRARY_PATH=build build/qore /path/to/snippet.q
```

Prefer `%new-style` and `%require-types` (both implied by `%modern`) in the snippet, since that is
how the affected code is written. Several of these traps are parse-time under `%require-types` and
runtime without it, so the directive changes which error you see.
