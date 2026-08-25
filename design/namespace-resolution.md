# Namespace Resolution

## Status

Implemented. The `::Qore`-first rule shipped in Qore 3.0 together with the
`%broken-namespace-resolution` escape hatch and the `ambiguous-call-resolution` warning; the
depth-ordered search predates it. This record describes the shipped algorithm and why it has the
shape it does. The user-facing contract is documented in
`doxygen/lang/205_namespaces.dox.tmpl` (`@ref namespace_resolution`), and the behaviour is covered by
`examples/test/qore/parser/qore-namespace-priority.qtest`.

Everything below lives in two files: `include/qore/intern/QoreNamespaceIntern.h` (the search
functions and the index types) and `lib/QoreNamespace.cpp` (the scoped-path matchers and the
ambiguity warning).

## Depth is the ordering key

Every `qore_ns_private` carries a `depth` field: the number of namespaces between it and the root
namespace. The root namespace has depth 0, a namespace declared directly in it has depth 1, and so
on. `updateDepthRecursive()` maintains the field when a subtree is attached or moved, and the index
structures below are keyed by it.

Depth — not declaration order, not tree traversal order — decides which of several candidate
namespaces provides a symbol. **The shallowest match wins.** This is what the language documentation
means by a "breadth-first" search; there is no actual traversal of the namespace tree at lookup time
for the unqualified case, because the indexes below have already collapsed the tree into a
name-to-shallowest-match map.

## The four steps of unqualified resolution

`parseFindClassIntern()` is the canonical shape; `parseFindFunctionEntryIntern()`,
`parseFindOnlyConstantEntryIntern()`, `parseFindGlobalVarIntern()`, `parseFindHashDeclIntern()`,
`parseFindEnumIntern()` and `parseFindTypedefIntern()` all repeat it. Each step returns immediately
on a match:

| Step | Search | Implementation |
|---|---|---|
| 1 | the `::Qore` subtree, shallowest match wins | `parseFindQore*Intern()`, gated on `!useBrokenNamespaceResolutionParse()` |
| 2 | the namespace currently being parsed, own symbols only | `parse_get_ns()` + a `parseFindLocal*()` / list `find()` call |
| 3 | the whole tree, shallowest match wins | the root index (`clmap`, `fmap`, `cnmap`, …) |
| 4 | every namespace by increasing depth, consulting any class handler | `NamespaceDepthListIterator` over `nshlist` — classes only |

Two lookups happen *before* step 1 where the context provides them, and they are not part of
namespace resolution proper:

- `parseFindConstantValueIntern()` searches the constants of the class being parsed (including
  inherited ones) before consulting namespaces at all;
- a function call inside a class resolves against the class's methods first, in the caller.

There is a parallel `runtimeFind*Intern()` family with the same four steps, gated on
`useBrokenNamespaceResolutionRuntime()` instead. The split exists because the parse-time gate reads
`parse_get_parse_options()` while the runtime gate reads `getProgram()->getParseOptions()`; a symbol
resolved dynamically (`Program::callFunction()`, reflection, `runtimeFindClass()`) must honour the
same option as the code that was parsed.

### Why step 1 exists

Before Qore 3.0 the search started at what is now step 2. Because step 3 prefers the shallowest
match and the `Qore` namespace sits at depth 1, *any* user symbol declared in the root namespace
(depth 0) silently outranked its built-in counterpart:

```qore
class Socket {              # depth 0
    string who() { return "user"; }
}
Socket s();                 # pre-3.0: the user class; 3.0: Qore::Socket
```

The failure mode was bad in both directions. A user who wanted their own `Socket` got it, but so did
every module in the same `Program` that expected `Qore::Socket` — and adding a class to the root
namespace of an application could break unrelated library code that had parsed cleanly for years.
Making `::Qore` unconditionally win removes the whole class of accidental shadowing: an unqualified
name that exists in `::Qore` always means the built-in symbol, and a user symbol of that name is
reachable only through an explicit path.

`isUnderQoreNamespace()` walks the `parent` chain rather than comparing depth, so subnamespaces of
`::Qore` (`Qore::Thread`, `Qore::SQL`, `Qore::err`, and namespaces that binary modules add under
`Qore`) participate in step 1 too, with the shallowest of them winning.

`%broken-namespace-resolution` / `PO_BROKEN_NAMESPACE_RESOLUTION` skips step 1 and only step 1;
steps 2-4 are unchanged, which is exactly the pre-3.0 order.

## The root indexes

Step 3 does not walk anything. The root namespace keeps one flat map per symbol kind — `clmap` for
classes, `fmap` for functions, `cnmap` for constants, `varmap` for global variables, `thdmap` for
hashdecls, and so on. `RootMap<T>` (and its `FunctionEntryRootMap` twin, which exists only to avoid
a `std::string` key on the hot function-call path) maps a name to a single `{namespace, object}`
pair, and `update()` keeps the entry whose namespace has the lowest depth:

```cpp
} else { // if the old depth is > the new depth, then replace
    if (i->second.depth() > ns->depth) {
        i->second.assign(ns, obj);
    }
}
```

Note what this does *not* say: there is no tie-break at equal depth. The first entry inserted for a
name is kept, and insertion order is not source declaration order — with three same-depth namespaces
`A`, `B` and `C` each defining `f()`, the call resolves to `C::f()` when they are declared in that
order and to `A::f()` when the declarations are reversed. So the outcome is stable for a given
program text but is an artifact of when each symbol reached the index, and it changes when an
unrelated module contributes a namespace at the same depth.

This is a deliberate trade: the flat index is what makes unqualified lookup a single map probe
instead of a tree walk, and paying for a deterministic tie-break would mean keeping every candidate.
The `ambiguous-call-resolution` warning exists to make the cases where the tie matters visible
rather than to resolve them.

Functions carry a second map, `pend_fmap`, for functions parsed but not yet committed.
`parseFindFunctionEntryIntern()` consults both and compares depths across them, so a pending
shallower declaration outranks a committed deeper one during the same parse.

## Qualified resolution is a different algorithm

A path such as `A::B::sym` does **not** go through the root indexes, and step 1 does not apply to
it. `parseFindScopedClassIntern()` and its siblings iterate `nsmap`, which maps a namespace *name*
to a depth-ordered multimap of every namespace with that name:

```cpp
NamespaceMapIterator nmi(nsmap, nscope[0]);
while (nmi.next()) {
    if ((oc = nmi.get()->parseMatchScopedClass(nscope, matched))) {
        return oc;
    }
}
```

Two properties follow, and both surprise people:

- **The leading component is a name, not a path from the root.** `Deep::Inner::MyConst` resolves
  even when `Deep` is nested three levels down, because `nsmap` is keyed by the namespace's own
  name. A namespace path is anchored only if it starts with `::`.
- **A partial match does not stop the search.** If `A::B` exists but has no `sym`, the iterator
  moves on to the next namespace named `A` and tries again. Only after every candidate fails is an
  error raised.

`matched` threads through the matchers as the count of path components that were matched by the
best attempt, and it exists solely to make the error message useful: `matched != size - 1` means a
namespace component failed to resolve, anything else means the namespaces matched but the symbol
was not found there. The two produce distinguishable parse errors, which is the difference between
"you misspelled the namespace" and "the symbol is not in that namespace".

The `::Qore` preference is deliberately absent here. A path is an explicit statement of intent, so
matching it by name and depth alone is what lets a user reach their own `A::Socket` at all.

## The ambiguity warning

`qore_root_ns_private::parseWarnAmbiguousFunctionCall()` in `lib/QoreNamespace.cpp` re-walks every
namespace after a successful unqualified function resolution, collects the paths that also define
that name, and raises `AMBIGUOUS-CALL-RESOLUTION` when there is more than one. It returns early —
no warning — in four cases:

| Condition | Why |
|---|---|
| `PO_IN_MODULE` | a module cannot control what its importers declare, so the warning is not actionable there |
| resolved under `::Qore` | step 1 makes the outcome deterministic; nothing is ambiguous |
| resolved to the namespace being parsed | step 2 makes the outcome deterministic |
| one match only | nothing to be ambiguous about |

All three call sites (`parseResolveFunctionEntryIntern()`, `parseResolveCallReferenceIntern()`, and
`FunctionCallNode::parseInitCall()`) gate the call on `PO_REQUIRE_TYPES`. The warning is in
`QP_WARN_STRICT`, which is excluded from `QP_WARN_ALL`, so `%enable-all-warnings` does not turn it
on; `%strict-warnings` or naming it explicitly does.

The re-walk is O(namespaces) per call site, which is why it is off by default and behind
`%require-types` rather than being folded into the resolution path itself.

## The depth list

Step 4 iterates `nshlist`, a `multimap<depth, ns>` whose name and comment ("root namespace with
handler map") suggest it holds only namespaces with a class handler. It does not:
`rebuildIndexes()` adds **every** namespace unconditionally, and `rebuildAllIndexes()` — which
`commitModule()` calls on every binary-module load — clears and repopulates it with the whole tree.
`setClassHandler()` adds an entry too, but that entry does not survive the next rebuild; what
survives is the `class_handler` member on the namespace itself, which is what `findLoadClass()`
actually consults.

So step 4 is a depth-ordered walk of every namespace that calls `findLoadClass()` on each, and it
only adds anything beyond step 3 for a namespace with a handler installed. The shallowest entry is
the root namespace at depth 0.

That last detail is why `NamespaceDepthListIterator::next()` incrementing before its first
dereference was a real defect rather than a cosmetic one: it dropped exactly the depth-0 entry, so a
class handler registered on the root namespace was never consulted and the classes it provides could
not be resolved at all. The iterator now stays positioned before the first element until the first
`next()` call, matching `NamespaceMapIterator::next()` in the same header.
`examples/test/module-cpp-api/module-cpp-api.qtest` covers it: the `cppapiuser` test module
registers a handler on the root namespace, and the test fails with
`cannot resolve call 'CppApiHandledClass::marker()' to any reachable and callable object` if the
off-by-one returns.
