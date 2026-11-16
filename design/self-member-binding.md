# Reads of members of self without a linear search (#6272)

Status: approved (ba3079e4e3 with the folding tests added). Implementation starts after batch T is assembled.

## Problem

A read of a member of `self` takes the same path in every execution mode:

- compiled code: `LoadSelfMember` → `qore_rt_load_self_member()` / `qore_rt_load_self_member_for_call()`;
- the AST interpreter: the self member reference node.

Both resolve the member by name at run time:

```
getReferencedMemberForSelf(name)
  ├─ member gate check:  cls->members.find(name)          (only if the class has a gate)
  └─ getReferencedMemberNoMethod(name)
       ├─ findAccessorMember(name)                       (only if the class has accessor members)
       ├─ runtimeGetMemberContext(name, class_ctx)
       │    └─ runtimeGetMemberInfo(name, class_ctx)
       │         ├─ class_ctx->members.find(name)         (for a private:internal member of the lexical class)
       │         └─ this->members.find(name)              (if class_ctx != the object's class)
       └─ getReferencedMemberNoMethodResolved()           (read lock, then the hash lookup in the object's data)
```

`QoreMemberMapBase::find()` is a `std::find_if` with `strcmp` over a `std::list`. Its cost grows with the number of
members of the class, inherited members included, and with the position of the member in the list.

Measured with callgrind (IR, debug build, develop e78bfd9d49) in the WebSocketConnection send benchmark of #6077:

| Part of one read of a member of self | Ir |
|---|---|
| `runtimeGetMemberInfo()`, with 2.09 `find()` calls per read at about 550 Ir each | about 1190 |
| the object-data part: lock, hash lookup, reference | about 490 |

The class-side resolution is about 70% of a read. Its result depends only on four things:
- the class of the object;
- the lexical class context (the class of the running method);
- the member name;
- the member maps of those classes.

None of these changes after the classes are committed. The exception is the builtin accessor members, which are added
only during module initialization.

## Option A: an immutable index for the member map of a class (runtime only)

**Scope:** `QoreMemberMap`, the map of a class's normal members, which every member read and write searches.
- `QoreVarMap` (static variables) and `HashDeclMemberMap` (typed hash members) share the `QoreMemberMapBase` template
  but are not on this path. They keep the linear scan, and can follow if a measurement shows the need.
- The `std::list` stays as the map's store and order. The AOT serializer, signatures, reflection and parse-time code
  iterate over it.

**The index:**
- It is a `std::unordered_map<std::string_view, QoreMemberInfo*>`, built once from the complete list when the class's
  members are frozen (see "When the members are frozen" below).
- The map holds a `std::atomic<const MemberIndex*>`, null before the freeze. It is stored with release semantics and
  loaded with acquire semantics.
- An index is never changed after it is built. No rehash, insert or erase ever runs on an index that a reader can see.
- `find()` reads the index when there is one; before the freeze, and for maps with fewer than N members, it keeps the
  linear scan.
- N is chosen from the release measurements (condition 3). The default is to always build the index.

### When the members are frozen

- **User classes:** at the end of `qore_class_private::parseCommit()`. Before that, the class is visible only to the
  parse that defines it (`parseAddMember()` refuses a committed class: `parseCheckSystemCommitted()`).
- **Builtin classes:** in `finalizeBuiltin()`, which `QoreNamespace::addSystemClass()` calls when it publishes the
  class (`QoreNamespace.cpp:527`) under the runtime namespace write guard.
- **Copies of a class** (imported or injected into another Program, `QoreClassIntern.h:4932`): at the end of the
  copy constructor (`QoreClass.cpp:866`, `QoreClass.cpp:973`), which builds the complete list before the copy is
  handed out.

### Condition 1: no change concurrent with reads

All structural changes of a class's `QoreMemberMap`, with what can see the class at that point:

| Change | Where | Class visible to running code? |
|---|---|---|
| `addNoCheck()` from `parseAddMember()` | `QoreClassIntern.h:3683` | no: a class being parsed; a committed class is refused |
| `addInheritedNoCheck()`, `replace()` | `parseImportMembers()`, `QoreClass.cpp:4378-4437` | no: run while the class is initialized for its parse, before `parseCommit()` |
| `moveAllTo()` | `QoreClass.cpp` (`QoreMemberMap::moveAllTo()`) | no: it moves a declaration's pending map into the class through `parseAddMember()` |
| `addNoCheck()` from `addMember()` | `QoreClassIntern.h:3879` | no: a builtin class, before `addSystemClass()` |
| `addNoCheck()` from `addBuiltinMemberWithAccessors()` | `QoreClass.cpp:3833` | no, by its API contract: "call before the class is initialized and published" (`QoreClass.h`, the method's `@note`). Enforced by the assertion below |
| `addNoCheck()` in the copy constructors | `QoreClass.cpp:953`, `QoreClass.cpp:1121` | no: the new class is not handed out until the constructor returns |
| list cleared and entries freed | `retireTypeOwner()`, `QoreClass.cpp:1252` | no: called when the class's last reference is released (`QoreTypeInfo.cpp:1333`). It frees the names and the `QoreMemberInfo` objects, so a concurrent read would already be a use-after-free today; the index adds no new requirement and is freed in the same place |
| destructor | `~QoreMemberMapBase()` | no: the class is being destroyed |

**Generic and lazily instantiated declarations:**
- A parameterized class is the same class with a parameterized type (`QoreParameterizedClassTypeInfo`); no member map
  is copied or changed.
- A parameterized hashdecl (`getParameterizedHashDecl()`, `TypedHashDecl.cpp:307`) builds its map completely before it
  is published through `parameterized_hashdecl_cache` under its lock. It is a `HashDeclMemberMap` in any case, outside
  the scope.
- The instantiation inside closures of #6268 is the same `getParameterizedHashDecl()` path. Its defect (a member
  default bound in the wrong frame) is not about member maps.

**Reflection and Program teardown:** they read the list; they do not change it.
- `releaseVarValues()` clears the values of static variables (`QoreVarMap::clear()` keeps its list).
- `releaseMethodsAndData()` changes no member map.

**The guarantee, enforced:**
- Each structural change of a class's `QoreMemberMap` goes through one private member of the map. In debug builds it
  asserts that the map is not frozen.
- `retireTypeOwner()` and the destructor assert that the class has no reference left.

**If the assertion ever trips,** for example in a module that adds accessor members to a published class:
- the late add builds a new complete index and publishes it with a release store of the pointer;
- the replaced index is kept until the map is destroyed, so a reader that loaded it never sees freed memory;
- the list itself is then changed under the class's `gate.asl_lock`. Today the list is changed with no
  synchronization at all; the change makes `find()` independent of the list after the freeze.

This fallback is designed and tested with a debug hook, but the expected and asserted path is that no class is
changed after its freeze.

### Condition 2: why a read makes two lookups

`runtimeGetMemberInfo(mem, class_ctx)` looks the member up:
- first in the lexical class's map (`class_ctx->members.find()`), because a `private:internal` member of the running
  method's class shadows a member of the same name of the object's class;
- then in the object's class's map (`members.find()`), which holds the merged members with their own `QoreMemberInfo`
  copies (access, internal storage contexts).

When the lexical class is the object's class, the second lookup is already skipped (`if (class_ctx != this)`). The
benchmark's 2.09 lookups per read come from an object of a subclass of `WebSocketConnection`, which is the common case
for handler connections.

As part of A, the second lookup is folded into the first when it cannot give a different result:
- **Condition:** the member found in the lexical class is not `private:internal`, and the object's class has no
  `private:internal` member in its hierarchy. That is a per-class flag computed when the members are frozen.
- **Why the result is the same then:** with no `private:internal` member below the lexical class, the object's
  class's entry for the name is the inherited copy of the lexical class's member, with the same storage (the object's
  main data) and the same access.
- **Check:** debug builds compute both lookups on every folded read and assert that they agree.

The read path has one more redundant lookup: `getReferencedMemberNoMethod()` calls `findAccessorMember()` (for classes
with accessor members), and `getReferencedMemberNoMethodResolved()` calls it again (`QoreObject.cpp:1488` and `:1502`).
The first result is passed on instead.

### Condition 4: the keys

- The index's `std::string_view` keys point at the list's `char*` names.
- No code renames an entry in place:
  - `replace()` changes only the `QoreMemberInfo*`;
  - the one `->first =` assignment near a class list (`BCSMList::resolveCopy()`, `QoreClass.cpp:4793`) is in the base
    class list, not a member map;
  - names are freed only with their entries (`retireTypeOwner()`, the destructor, `moveAllTo()` after the move).
- Once the list is private, its entries are reached through an iterator whose key is `const`, so an in-place rename does
  not compile.
- `replace()` on a frozen map is covered by the assertion of condition 1. Before the freeze there is no index to update.

### Effect and cost

- **Each `find()`:** drops from about 550 Ir (debug) to the cost of hashing the name plus one bucket compare.
- **Folding:** removes the second lookup on the subclass path.
- **Memory:** one immutable hash table per class with at least N members.
- **Generated code:** no codegen change and no AOT format change.
- **Reach:** every caller benefits — reads and writes in all modes, accessors, the member gate check and reflection.

## Option B: a per-call-site binding in compiled code

Each `LoadSelfMember` site (IR instruction, JIT code, AOT code), and the corresponding store and lvalue sites, gets a
cache. It holds the object's class, the class context and the dispatch epoch, and caches the resolved
`QoreMemberInfo*`, the member class context, and whether the member has accessors or the class has a gate.

- **Hit:** two pointer compares and an epoch compare skip the whole class-side resolution, about 20 Ir. The
  object-data part (about 490 Ir) remains.
- **Invalidation:**
  - the global method dispatch epoch already covers class addresses that are reused: `releaseMethodsAndData()`,
    Program teardown and the end of module lifetime all bump it;
  - member maps don't change after commit, and objects exist only of committed classes;
  - accessor members are added only during module initialization;
  - an object of a subclass misses on the class compare and is resolved the slow way. The cache is monomorphic and is
    replaced on a miss, or it could keep 2-4 entries like the AOT method dispatch cache.
- **Cost:**
  - the IR instructions need a mutable cache field, and JIT code a per-site global;
  - AOT code needs a per-site slot in `QoreAOTContext`. That changes what qcc emits and what the runtime reads: an
    AOT format version, which takes the batch's one format slot;
  - it covers compiled code only. The AST interpreter, writes through other paths and reflection still use `find()`;
  - concurrent first fills from several threads need atomics, as `method_dispatch_cache` has.

## Recommendation

**Do A first, as #6272.**
- It removes the linear search everywhere, needs no format change and no invalidation rules, and is small.
- It is the floor that B would fall back to on every miss in any case.

**Measure B as a follow-up, only if it pays.** After A, measure the class-side share of a member read. If the
remaining resolution (hashing plus the two-map logic) is still at least about 25% of a read in the member-heavy row,
file B separately; it would take a batch's AOT format slot.

**Not in scope:** the object-data part (the read lock and the hash lookup of the value). Making that cheaper would need
slot-indexed object storage, which is a much larger change to `qore_object_private`.

## Measurements before and after (condition 3 and 5)

- **Decisions on release builds.** The decisions (N, and B go/no-go) are made from release builds. Debug numbers are
  given as a supplement.
- **Classes measured.** Classes of 3, 10 and 40 members, the last with 10 more inherited, each read:
  - by a method of the class itself (one lookup);
  - by a method of a parent class on a subclass object (two lookups before folding);
  - for a `private:internal` member of the lexical class (no folding).
- **Modes.** ast, ir, jit (synchronous compilation) and AOT (qcc).
- **Choosing N.** N is the smallest member count at which the index is at or below the linear scan in every row.
- **The +0 rule.** It holds on release builds for the IR loop rows `call`, `map`, `trycatch`, `onexit` and
  `onexit_ret` (#6074) in ir, jit and AOT.
- **Further rows.** The member-heavy rows above, and the WebSocketConnection send benchmark of #6077.
- **B go/no-go.** B is decided on the release member-heavy row: the class-side share of a read after A must be at
  least about 25%.

Tests:
- the member, inheritance, `private:internal`, accessor and gate tests (`qore/classes`, reflection,
  builtin-member-write-gates);
- a unit test in `ql_debug.cpp` that checks the index against the list for every class of a loaded Program, including
  the folding condition;
- tests of the folding, each run on a debug build, where the assertion that the folded and the two-lookup results agree
  runs on every read:
  - private inheritance: `class B inherits private A`, a method of A reading a member of self on a B object;
  - a subclass that redeclares a member of the same name;
  - a member gate on the subclass only;
  - accessor members on the subclass only;
- a debug-hook test of the late-add fallback;
- the tests of a module that adds accessor members (jni), run on a debug build, to show that the freeze assertion holds
  for it.
