# Freeing retired derived types and the declarations they name

Issue: [#6419](https://github.com/qoretechnologies/qore/issues/6419).  Steps (a) (design), (b) (type parameter types),
1a (epochs) and 1b/1c (freeing declarations, quiescent waits) are implemented; see "Stages 1b and 1c: what was built"
for what differs from the proposal below.  Related: #5790 (declaration retirement), #5765 (generic caches), #6421
(guard profile type identity).

File and line references are to develop 4706551f7c.

## Problem

A derived type is a type made on demand and cached in `lib/QoreTypeInfo.cpp`.  The kinds are:

- complex hash, list, softlist and reference types (`ch_map` ... `cslon_map`);
- complex buffer types (`cb_map`, `cbon_map`);
- unions (`union_map`, `union_on_map`);
- typed callable types (`complex_code_map`);
- parameterized class types (`parameterized_class_map`);
- type parameter types (`type_parameter_map`);
- wildcard types (`wildcard_type_map`).

When a class, hashdecl or enum is destroyed, `qore_purge_derived_types()` removes the derived types built on its
types from the caches and moves them to `retired_derived_types`, which is freed only at library cleanup.  Freeing
them earlier was never safe for two reasons:

1. **Raw pointers.** Values, code and caches hold derived types as raw pointers, without references (see the
   inventory).  A type check in any thread can be reading one at any time.
2. **Never-reused addresses are assumed.** `qore_get_type_identity_serial()` returns 0 for a derived type because
   its address is never reused, so every cache keys derived types by address alone.

A declaration that a derived type names is therefore retired, not freed: `type_owner_derived()` keeps it until
library cleanup (`qore_type_owner_released()`, `lib/QoreTypeInfo.cpp:1298`).  A retired class releases its methods,
members and static variables (`QoreTypeOwner`), but it keeps its own allocation, its types and its name.

Step (b) (501a444c86) stopped a generic declaration's own type parameter types from keeping it.  What remains per
generation of a Program whose generic values were used (`examples/test/qore/classes/deleted-program-own-generic-cycle.qtest`)
is:

- 2 retired classes;
- 2 orphaned type parameter types;
- `Box<Foo>`;
- 2 complex list types and 1 complex hash type.

That is 10.39 KiB per generation (Release RSS).  I estimate that most of it is the two retired classes and that the
six derived types are 1-2 KiB; the step 2 test measures the split.

## Two different things to free

There are two separate things to free, with very different risks.

- **(A) The declarations that derived types name.**
  - They hold most of the memory.
  - Their types already have an identity serial, and the caches that can outlive them already key by it.
  - Freeing them needs no change to any cache keyed by a derived type's address.
  - It needs every pointer from a derived type to a declaration, or to a type the declaration owns, to stop being
    followed: the derived type is **orphaned**, as (b) does for type parameter types.  It also needs a grace period
    for the threads that read such a pointer just before it was orphaned.
- **(B) The derived types themselves.**
  - They are small, but a program that keeps creating and deleting Programs with generic code accumulates them
    forever.
  - Freeing one makes its address reusable, which breaks the serial-0 assumption of every holder in the inventory
    that keys or compares by address.
  - It also needs to know that no live value has it as its type, which is the per-value counting question.

**Proposal: do (A) first (stage 1) and (B) only if the measurement after stage 1 justifies it (stage 2).** Both use
the same epoch mechanism.  Stage 1 alone removes the declarations from the leak and changes no address-keyed cache.

## Inventory: where a derived type is held outside a value

In the tables, *Kind* says whether the holder can contain derived types (D), declaration types (C), or both.  *Key*
says whether it is a map or set keyed by a type address (addr), by address plus identity serial (addr+serial), or not
keyed (-).  *X-Pgm* says whether it can name types of another Program.  The quiescence point is when no thread can
still be reading the pointer once the holder has dropped it.

### Values (for reference: the free condition of stage 2)

| Holder | File | Lifetime | Notes |
|---|---|---|---|
| `qore_list_private::complexTypeInfo` | `include/qore/intern/qore_list_private.h:53` | the list | any thread |
| `qore_hash_private::complexTypeInfo` | `include/qore/intern/QoreHashNodeIntern.h:127` | the hash | copied on copy and `+` |
| `qore_object_private::instantiated_type` | `include/qore/intern/QoreObjectIntern.h:229` | the object | `Box<Foo>`, builtin `Future`/`Channel`/`Queue` type args; the object refs its class's declaration, not its type args' |
| `lvalue_ref::typeInfo` | `include/qore/intern/lvalue_ref.h:42` | the reference | holds a dependency ref on its Program only |
| `QoreClosureTypeContext` (`inst.type_args`, `receiver_type_info`) | `include/qore/intern/QoreClosureNode.h:123-162` | the closure | generic closures |
| call references `receiver_type_info`, `explicit_inst` | `include/qore/intern/CallReferenceNode.h:231,459` | the call reference | |
| iterators (`QoreListIterator`, `QoreListHashIterator`, `QoreHashIterator`, `SingleValueIterator`, `QoreChannelIterator`) | `QoreListIterator.h:85` etc. | the iterator object | |
| reflection `Type` (`QoreType::typeInfo`) | `modules/reflection/src/QC_Type.h:50` | the object | keeps the declarations (`QoreExternalTypeDeclRefs`), not the derived type |

### Derived types naming other types (what orphaning must cut)

| Holder | File | Kind | Notes |
|---|---|---|---|
| `QoreTypeSpec::u.ti` in `accept_vec` / `return_vec` | `include/qore/intern/QoreTypeSpec.h:389-395` | C, D | element type of every complex type; union members' specs; read by every type check (17 sites in `QoreTypeSpec.h`, 57 in `QoreTypeInfo.cpp`) |
| `QoreParameterizedClassTypeInfo::base_class`, `type_args` | `include/qore/intern/QoreTypeInfo.h:1640-1641` | C, D | |
| `QoreUnionTypeInfo::members` | `QoreTypeInfo.h:4545` | C, D | |
| `QoreComplexCodeTypeInfo::returnType`, `paramTypes` | `QoreTypeInfo.h:4865-4866` | C, D | |
| `QoreWildcardTypeInfo::bound` | `QoreTypeInfo.h:1754` | C, D | |
| `QoreTypeParameterTypeInfo` owners | `QoreTypeInfo.h:1716-1718` | declaration | already atomic and orphaned by (b) |
| or-nothing twins (`QoreParameterizedClassTypeInfo`, `QoreTypeParameterTypeInfo`, `QoreComplexBufferOrNothingTypeInfo`) | `lib/QoreTypeInfo.cpp:2141-2153,2296-2311,2384+` | D | point to the non-nothing twin, retired together |
| `QoreComplexReferenceTypeInfo::ref_type` | `QoreTypeInfo.h:2344` | D | owned and deleted with its parent |

### Code: IR (freed with the `QoreIRFunction` unless noted)

| Holder | File | Kind | Key | X-Pgm | Release / quiescence |
|---|---|---|---|---|---|
| `UserVariantBase::specialized_ir_cache` (`QoreGenericSpecializationKey`: receiver, `type_args`) | `include/qore/intern/Function.h:311-324,1755`; insert `lib/Function.cpp:9473-9511` | C, D | addr+serial (serial 0 for D) | yes | registered (`SpecializedIRCache::evictDeadTypes`, `Function.cpp:9515`); a running call holds a reference on the `QoreSpecializedIR`; quiescent when the last reference is released |
| specialized `QoreIRFunction::specialization_receiver_type_info`, `specialization_type_param_instantiation` | `include/qore/intern/QoreIR.h:4632,4635` | C, D | - | yes | freed with the specialization (above) |
| per-instruction types: `element_type`, guard `type_info`, `value_parse_type`, lvalue-path step `type_info`, `MakeList`/`MakeHash` `typeInfo`, `object_type_info`, `receiver_type_info`, `explicit_type_param_inst`, `NewHashDeclFromHash::hd` | `QoreIR.h:1588,2120,2138,2351,2753,2767,2794,2851,3084,3426,3584,3585,3644,3717,4039,4040` | C, D | - | via inlining and specialization | freed with the IR; quiescent when no thread runs it (Program thread count, specialization references) |
| derived types made by lowering (`qore_get_complex_hash_type`) | `lib/QoreIRLowering.cpp:21949,22295` | D | - | no | as above |
| analysis data: `value_facts[].type_info`, `exact_assigned_boxed_local_types` (values), `return_type_info` | `QoreIR.h:1321,4512,4521,4629` | C, D | - | no | as above |
| call-site caches `cached_return_type` | `QoreIR.h:3459,3516,3556,3592,3667,3726`; filled `lib/QoreIRInterpreter.cpp:4298,4541` | C, D | - | yes (inlined imported functions) | as above; written with the cached callee and validated with it (the instruction's fixed variant, the receiver class serial, or the variant serial in the JIT cache) |
| guard type profiles `TypeProfile::extended_type_counts`, `plugin_type_counts` | `QoreIR.h:1913,1936-1937` | C, D | addr (develop); addr+serial with #6421 | yes | #6421 registers and evicts only types with a nonzero serial; **derived types are counted unregistered** and stay until the IR is freed; only compared, never dereferenced (except plugin types) |
| other IR owners: `jit_owned_ir` (every generation, until the variant dies), `StatementBlock::cached_toplevel_ir`, `cached_toplevel_aot_debug_ir`, `helper_ir`, JIT work items' `owned_ir_func` | `Function.h:1709`; `StatementBlock.h:355,375`; `QoreIR.h:3198`; `QoreJIT.h:1269,1370` | C, D | - | as above | as above; `jit_owned_ir` keeps old generations for the variant's lifetime |

### Code: JIT native code and AOT

| Holder | File | Kind | Key | X-Pgm | Release / quiescence |
|---|---|---|---|---|---|
| type pointers compiled into JIT machine code as immediates (`getTypeInfoPointerArg()` plus direct casts; also `QoreTypeParamInstantiation*`) | `lib/QoreIRToLLVM.cpp:1529-1536` (about 13 callers), 9517, 19891, 19932, 20353, 20402, 23336, 23363, 23837, 24013, 24221, 27466, 33142, 33168, 33978, 34104, 36593, 37371; instantiations 22680, 22710, 26596, 26621, 27470, 27885, 28800 | C, D (including derived types created at codegen by `specializeType()`) | - | yes (specializations, importers, `deps`) | a `CodeUnit` is freed by `QoreJIT::releaseCodeOwner()` (Program teardown, `lib/QoreProgram.cpp:3752`; last specialization reference, `Function.cpp:7662`); units with no owner (AOT IR without a Program), OOM-recorded units, and units another unit links to (`deps`) can live for the process; old code survives drop/recompile until the owner is released.  **Cannot be evicted: an immediate cannot be patched.** |
| `QoreAOTContext::type_path_cache` (receiver to resolved type), `hashdecl_path_cache` | `include/qore/intern/QoreAOT.h:199-208,443,455-459`; `lib/JITRuntime.cpp:5948-6050` | C, D | addr + base-class serial for a parameterized class receiver (`JITRuntime.cpp:5320-5364`) | yes | the variant's `cached_aot_ctx`; **relies on parameterized class types never being freed** (comment at 5326) |
| per-thread front caches for type and hashdecl paths | `JITRuntime.cpp:5444-5453,5978-5995` | C, D | addr, guarded by generation and `qore_get_method_dispatch_epoch()` | yes | invalid after an epoch bump; nothing bumps it when a derived type is freed |
| per-thread instanceof cache (path address, Program id) to type | `JITRuntime.cpp:7457-7488` | C, D | path + Program id | no | **relies on "the global registries of complex types, which are not freed"** (comment at 7425) |
| `QoreAOTMethodDispatchCacheEntry::receiver_type_info`, `arg_types[8]` | `QoreAOT.h:236-253`; `JITRuntime.cpp:21831,21914,21946` | C, D (argument value types) | addr + dispatch epoch | yes | kept until the call target is destroyed; safe only because class and hashdecl destruction bumps the dispatch epoch |
| `QoreAOTCallTarget::object_type_info`, `receiver_type_info`, `explicit_type_param_instantiation` | `QoreAOT.h:345-347`; resolved in `lib/QoreAOTRuntime.cpp` | C, D | - | yes | the variant's AOT context |
| `qore_builtin_type_path_table` | `JITRuntime.cpp:5946` | D (builtin-only, e.g. `hash<string, int>`) | path | no | never freed; its types name no declaration and are never retired |
| AOT images | `lib/QoreAOTBinary.cpp:6739-6747` | none | - | - | an image names types by path string and embeds no type pointers; images are never `dlclose`d |
| `aot_deferred_type_info_map` (placeholder types by path) | `lib/QoreTypeInfo.cpp:395-466` | placeholder (derived types can be built on it) | path | yes | never freed |

### Runtime holders other than code

| Holder | File | Kind | Key | Lifetime / quiescence |
|---|---|---|---|---|
| declarations: member, static variable and hashdecl member types, `BCNode::type_info`, `qore_class_private::type_param_bound_types`, `type_param_default_types`, `runtime_default_types` (filled lazily), `parameterized_vparents`; `typed_hash_decl_private::type_args`, `default_receiver_type_info` | `include/qore/intern/QoreClassIntern.h:1243-1245,2394,3010-3017`; `typed_hash_decl_private.h:92,973` | C, D | - | the declaration; a declaration that survives its Program references what it names directly (`QoreTypeOwner`) |
| signatures: `returnTypeInfo`, `typeList`, `type_param_*_types`, `nn_uniqueReturnType`, `UserClosureFunction::classTypeInfo` | `Function.h:261-262,534-535,3114,3535` | C, D | - | the function / Program |
| globals and typedefs: `Var::typeInfo` (also `is_ref` into another Program), `TypedefEntry::typeInfo` (copied into importers), `ConstantEntry::typeInfo` | `Variable.h:175-182`; `QoreNamespaceIntern.h:73`; `ConstantList.h:158` | C, D | - | the Program |
| locals: `LocalVar::typeInfo`, `refTypeInfo`, `narrowedTypeInfo` (parse tree); `ClosureVarValue::typeInfo`, `refTypeInfo` (refcounted) | `LocalVar.h:343-344,1417-1419` | C, D | - | the Program / the closure |
| `LocalVarValue::type_substitution_cache` | `LocalVar.h:189-205`; `lib/Variable.cpp:4884-4980` | C, D | addr (inputs and results; no serial; not registered) | a per-thread variable stack entry: quiescent when the frame is popped |
| parse-time AST nodes (`FunctionCallNode` type args and instantiations, `QoreParseListNode`/`QoreParseHashNode` types, operator nodes, `TryStatement`, `NewComplexTypeNode`, ...) | `FunctionCallNode.h:52-57,378,916-917` etc. | C, D | - | the Program's parse tree; some are read at run time by any thread running the Program |
| thread state: `ThreadData::current_receiver_type_info`, `returnTypeInfo`, `implicit_arg*_type_info`, `thread_local runtime_type_param_instantiation`, `RuntimeConfig::return_type_info`, saved copies in context helpers; `CodeEvaluationHelper` per frame; `LValueHelper` / `lvinfo` | `lib/thread.cpp:740,901-909,2239-2240`; `lib/Function.cpp:127`; `Function.h:1185-1213`; `Variable.h:1114,1750` | C, D | - | the current frame or call: quiescent when the thread leaves the call (or Qore code) |
| background thread start parameters `BGThreadParams::type_param_inst`, `receiver_type_info` | `lib/thread.cpp:2281-2283` | C, D | - | copied from the starting thread; read by the new thread at its start |
| generic caches: `specialized_ir_cache` (above), `typed_hash_decl_private::parameterized_hashdecl_cache` | `typed_hash_decl_private.h:889-945`; evicted `lib/TypedHashDecl.cpp:440` | C, D | addr+serial (0 for D) | registered and evicted; the evicted entry is released after the locks (`qore_generic_cache_release_list_t`) |
| bookkeeping: `derived_type_keys`, `type_param_owner_keys`, `QoreGenericCacheRegistry::by_type` / `by_cache` | `lib/QoreTypeInfo.cpp:611,618,813-815` | C, D | addr | cleaned by the purge; registry entries for a freed derived type must be removed before its address can be reused |
| the derived-type caches themselves | `lib/QoreTypeInfo.cpp:500-605` | D | addr of the types they are built on | the purge removes the entries; with freeing, a new type with an old address must not find a stale entry (it cannot: the entries are removed when retired) |

Checked and holding no type pointers: `cached_pre_instantiated`, `owned_locations`, the IR tier and OSR state,
`QoreCallTargetCheckCache`, `NativeClosureLeafCache` (keyed by serial), `ClosureInlineCache`, exceptions and call
stack info, `qore_program_private`, buffers (they hold an element enum), enum values, `typeinfo_map` (builtin types
only), and the plugin type descriptors (module types).

### What the inventory means for each stage

- **Stage 1 (free declarations, keep the derived types).**
  - No holder above keyed by a derived type's address is affected: no derived type's address is reused.
  - Holders of a declaration's own types are already handled, because these types are freed today when no derived
    type names them: the caches that outlive them key by serial or are evicted (#5765, #5790, #6421).
  - What is new is that a derived type that stays allocated must not lead to the freed declaration.  The second
    table lists every such pointer.
- **Stage 2 (free derived types).** Every holder keyed by a derived type's address, or embedding it, must either
  release it before the free or give it an identity.  That covers the specialization key, the parameterized hashdecl
  key, the guard profiles, the AOT type path caches, the instanceof and front caches, the dispatch cache, the
  substitution cache, and the registry and bookkeeping.
  - The simplest way is to give a mortal derived type a nonzero identity serial, taken from a never-reused counter
    when the type is created.  The existing (address, serial) keys then stop matching a reused address without
    further change.
  - The comments at `JITRuntime.cpp:5326` and 7425 describe invariants that stage 2 changes; that code must change
    with it.
  - **JIT immediates cannot be evicted.** A derived type embedded in a code unit can be freed only after the unit is
    freed.  The unit must therefore hold the derived type; see "the free condition".

## The epoch scheme

### Retire list

- When the purge retires derived types (stage 2), or the last release of a declaration finds only orphanable derived
  types naming it (stage 1), the items go onto a **pending list** under `ctl` instead of `retired_derived_types` or
  `retired_type_owners`.
- Each item records the global epoch at the time it was retired (`retire_epoch`).
- The global epoch is a 64-bit counter.  Each retirement batch increments it once, after the orphaning stores.

### Per-thread epoch publishing

- Each `ThreadData` gets one atomic 64-bit word, `type_epoch`.  0 means quiescent; otherwise it holds the global
  epoch the thread read when it entered.
- **Outermost entry and exit** (implemented in stage 1a): a thread runs Qore code exactly while it has a Program
  context (`ThreadData::current_pgm`), which only `lib/thread.cpp` sets, through `set_thread_current_pgm()`.
  - When the context is set from none, the thread publishes the current global epoch in `ThreadData::type_epoch`;
    when it is cleared, the thread stores 0.  A thread created with a Program context publishes it in its
    `ThreadData` constructor.
  - Setting the context from one Program to another (an inner entry, a call into another Program) is only a test of
    the two pointers.
  - The store is a sequentially consistent exchange: a thread entering Qore code must publish its epoch before it
    reads any type pointer (store-load ordering), so the exchange (a locked instruction on x86-64) is the price of
    an outermost entry.  It is never on the call path of code already running.
  - A consequence: the main thread runs Qore code for its whole run, and a thread that deletes a Program is in Qore
    code while it retires the Program's types, so without the quiescent waits of stage 1c nothing retired is ever
    freeable while the main thread runs.  Stage 1c is what makes the frees happen.
- **Blocked threads are quiescent.**
  - The blocking primitives store 0 before they wait, and republish the current global epoch when they wake.  These
    are `QoreCondition::wait*`, `QoreQueue` waits, `SmartMutex`/`RWLock`/`VLock` waits, sleep, `Thread` joins and
    the socket, poll and async I/O waits.
  - This is safe only at waits that no type check spans.  A thread that loaded an element type through a derived
    type's spec and then blocks in an object or lvalue lock with that pointer in a local would break it.
  - Stage 1 therefore publishes quiescence only at the waits a Qore builtin performs on behalf of the program:
    `Queue::get`, `Condition::wait`, `sleep`, socket and file I/O, `Thread` joins.  At those, the caller's frame holds
    values (referenced) and code-held types (owned by IR, AST or code units), but no pointer obtained through an
    orphanable pointer.
  - Internal lock waits in the middle of an assignment (`VLock`, `QoreRWLock` inside `LValueHelper`) do **not**
    publish quiescence.  They are short, and a deadlock there blocks far more than retirement.
  - The list of quiescent waits is explicit, and each is reviewed against the inventory.
- Foreign threads (a JVM or Python thread registered with Qore) publish only while they are inside an entry.
  Registration alone does not make a thread active, so a foreign thread blocked in its own runtime does not hold back
  a grace period.

### Grace period and when frees run

- A pending item with `retire_epoch = E` can be freed once every thread's `type_epoch` is 0 or at least E,
  i.e. every thread has been quiescent at some point after the item was retired.  The scan uses
  `thread_list.anyThreadData()` (the pattern of `qore_stop_module_uncounted_entries()`, `lib/thread.cpp`).
- The scan is not on any fast path. It runs:
  - after each retirement batch, in the thread that retired it, which is typically the one deleting a Program;
  - at the outermost exit of a thread, only when the pending list is not empty: a relaxed load of a global flag;
  - at Program deletion.
- The free itself runs outside `ctl`, as the generic cache release list does today.
- No thread waits for a grace period.  Retirement never blocks: pending items wait for a later scan.

### The free condition

An item is freed only when both of these hold:

1. **Every thread has passed its retire epoch** (the grace period above).  This covers every transient pointer held
   by a running thread: locals in C++ frames, interpreter registers and type-check temporaries.
2. **Nothing persistent leads to it any more.**
   - **Stage 1: a declaration.**
     - Every derived type that names it has been orphaned.  Its pointers to the declaration and to the types the
       declaration owns are replaced, with release stores, by pointers to an immortal tombstone type per kind (an
       object type, a hashdecl type or an enum type; each `*`-variant accepts `NOTHING`).  The tombstone keeps the
       declaration's name for error messages; the name is copied when the derived type is orphaned, outside the
       lock, so the destructor path does not allocate.
     - This is (b)'s orphaning extended to every kind in the second table.
     - Semantics do not change.  A retired declaration already matches no value except values of an equal
       declaration, of which none exist (`QoreTypeOwner` comment, `include/qore/intern/QoreTypeOwner.h:48-80`), and
       the tombstone matches the same set.
     - Readers of the orphanable fields load them with acquire ordering. On x86-64 and AArch64 that is a plain load
       (AArch64: `ldar`), so type checks cost nothing measurable.  The fields become atomics, and every read site
       changes (74 `u.ti` sites, plus the other fields).
     - A value whose own type is the declaration's type (an object, a hashdecl value) still references the
       declaration, so the declaration is not released while such a value lives.  Reflection keeps its declarations
       (`QoreExternalTypeDeclRefs`).
   - **Stage 2: a derived type.**
     - No value has it as its type, and no holder in the code and cache tables keeps it.  Values are the
       per-value counting question.  The proposal is to count only **mortal** derived types, those whose
       construction reaches a user declaration (a flag set at creation).
     - `list<int>`, `hash<string, int>` and every builtin-only derived type are immortal: never retired, never
       counted.  Typed containers of them pay one predictable branch on a flag in the type; the type check reads the
       type anyway.
     - Containers of mortal types, objects with a mortal `instantiated_type`, closures with a mortal type context,
       and iterators and references naming one pay an atomic increment and decrement.
     - JIT code units, AOT contexts and specializations hold a count on each mortal derived type they embed or key
       by, released when the unit, context or specialization is freed.  This is the only way the immediates can be
       covered.
     - The registries and caches keyed by address remove a freed type's entries first; the mortal types' identity
       serials (see above) make any missed entry harmless.

### Bounding and reporting the delay

The delay comes from a thread that stays inside Qore code without passing a quiescent point: a long native call, a
loop with no blocking call, or a foreign thread inside an entry.

- **Memory bound.** The pending list holds exactly what is kept forever today (`retired_type_owners`,
  `retired_derived_types`).  The scheme never keeps more than today, and keeps it only until the slowest thread next
  passes a quiescent point.  The worst case, a thread that never leaves Qore code, is today's behaviour, not a leak
  the scheme adds.
- **Bookkeeping** (stage 1a): each retired declaration and derived type records its epoch (parallel vectors of
  `retired_type_owners` and `retired_derived_types`, under the type lock).  `qore_type_retirement_info()` counts the
  pending items, those whose grace period has passed (freeable), the oldest and newest pending epochs and the
  lowest epoch of the threads in Qore code; it reads the items before it scans the threads, so an item counted as
  freeable is one no thread can read.  Debug builds report it with `dbg_type_retirement_info()`.
- **Diagnostic of who blocks** (stage 1a): `dbg_type_retirement_blockers()` (debug builds) returns each thread that
  runs Qore code since an epoch before the newest pending item, with its TID and epoch.  The thread's location is
  not read there, as another thread's stack is read only with the handshake of `getAllCallStacks()`: a test or an
  operator calls `get_all_thread_call_stacks()` for it.
- **Teardown report** (stage 1a): with `QORE_DEBUG_TYPE_RETIREMENT` set, library cleanup prints the bookkeeping
  (`[TYPE-RETIREMENT] ...`) before it frees everything, as it does today.
- **No forced progress.** A grace period is never forced by interrupting a thread: it would need signals or safe
  points in native code, a far larger change.  The report makes the delay visible; it is not bounded in time.

### What the scheme does not cover

- **Transient pointers held across a quiescent wait.** Any C++ code that loads a type through an orphanable pointer
  and then waits in one of the quiescent waits.  Stage 1 must review each quiescent wait.  The list is deliberately
  limited to builtin-level waits (see above).
- **Native code that reads values without a counted entry.** A module that keeps Qore values in its own threads and
  type-checks them (a list `push()` from a JVM or Python callback thread) without entering a Program has no
  `type_epoch`.  Each such C++ API entry must be found and wrapped in an epoch entry, or the free is unsafe.
  - The binary modules in this repository and the module-* repositories need an audit.
  - The alternative is to publish in the type-check functions themselves, which covers every caller at the cost of
    a TLS lookup per check.  I propose the audit and an assertion: in debug builds, a type check of a mortal type
    asserts that the thread's `type_epoch` is set.
- **Stage 2 only, not stage 1:**
  - JIT units that no owner releases (units of AOT IR without a Program, OOM-recorded units, `deps` chains): their
    derived types are kept while the unit lives.
  - `aot_deferred_type_info_map` placeholders and derived types built on them: never freed (path-keyed, process
    lifetime).
  - `jit_owned_ir` keeps every IR generation of a variant, and with it its derived types, until the variant dies.
- **Found by the inventory and handled separately** (not derived types): the type arguments that a call reference
  or a closure holds did not keep their declarations; they keep them since #6489 (`QoreTypeDeclRefs`), as values
  and objects do since #5790.

## Stages 1b and 1c: what was built

Stage 1b (freeing declarations) and stage 1c (quiescent waits) ship together: without 1c, the main thread runs Qore
code for its whole run and holds back every retirement.

### Tombstones (`lib/QoreTypeTombstone.cpp`)

**Deviation from the proposal above.** The proposal said a retired declaration matches no value except values of an
equal declaration, "of which none exist", and that a name-only tombstone matches the same set.  It does not: another
Program parsed from the same source has equal declarations, and today a value typed with a deleted Program's
declarations (`list<Foo>`, `list<hash<HD>>`, `list<E>`) accepts their values, and an empty `list<Foo>` is a
`list<Base>` of the new Program.  A name-only tombstone would have broken that reload pattern, so the tombstones
reproduce the equality of the declaration (decision by prof, recorded for David):

| Kind | Equality the type checks use | The tombstone keeps |
|---|---|---|
| class | `qore_class_private::equal()`: class ID, or name and signature hash | the name, path and signature hash; the tombstones of the base classes (with their access and virtual flags, and the parameterized parent types); the type parameters with the tombstones of their bounds and defaults; the raw-acceptance flags |
| hashdecl | `typed_hash_decl_private::equal()`: name and members, member types compared with `QoreTypeInfo::equal()` | the name, path and type parameters; the members, with the tombstones of their types and their defaults when they are simple values; the parent's tombstone; a concrete declaration of a generic hashdecl is that of the generic hashdecl's tombstone with the tombstones of its type arguments |
| enum | `qore_enum_decl_private::parseEqual()`: name, base type and members | the name, path, base type and members |

- A tombstone is a real, empty, builtin (`sys`) declaration, so it is never retired, and the existing type check code
  runs on it unchanged.
- Tombstones are **interned by a fingerprint** of the properties above (back references in cycles are written as the
  distance from the top of the fingerprint's stack, and the fingerprint of a declaration that refers back to nothing
  outside it is reused, so a hierarchy that reaches a declaration by many paths is fingerprinted once), so a Program created and deleted repeatedly from the same source adds no
  tombstone after the first deletion.
- The recursion ends at builtin declarations and builtin types, which are referenced directly; a user declaration is
  always tombstoned, live or not, so a tombstone never points to a user declaration and keeps nothing alive.
- The tombstone of each declaration is cached in its `QoreTypeOwner` (`type_owner_tombstone`).  The tombstones of a
  deleted Program's declarations that derived types name, and of the concrete declarations of its generic hashdecls
  (`HD<X>`, which the generic hashdecl's cache holds), are made in `~QoreTypeOwnerBatch()` before any declaration of
  the Program is released, since a tombstone is made from what the declaration names (the bounds of its type
  parameters, for example), which can be freed before the declaration is retired.  The bounds and defaults of a
  class's type parameters are now also among the names that a surviving class keeps (`getTypeOwnerNames()`).
  A parse that fails removes its declarations (`qore_program_private::internParseRollback()`) in a batch of the same
  kind (`qore_ns_private::getRollbackTypeOwners()`: the pending declarations of the namespaces that existed, and every
  declaration of the namespaces the parse added); they name each other's types without references as well, and the
  lists free them in hash order.
- What cannot be tombstoned (a type parameter of a function signature, an unknown kind of derived type that names a
  user declaration, nesting deeper than 64): no tombstone is made, the pointer is not orphaned, and the declaration is
  marked unfreeable (`type_owner_unfreeable`): it is kept until library cleanup, as before.
- Not reproduced: the `orig` identity shortcut of hashdecl equality (the structural comparison that follows it gives
  the same result for the copies of one declaration), a member default that is not a simple value (a plain hash
  converted to the hashdecl through a kept value gets no such default), and `injectedClass`.

### Orphaning

- The orphanable fields are read with acquire loads: `QoreTypeSpec::u` through `uTi()`, `uQc()`, `uHd()` and `uEd()`
  (the union is `mutable` and the stores are `__atomic` stores, so the `const` spec members could stay const);
  `QoreOrphanablePtr<T>` for `QoreParameterizedClassTypeInfo::base_class` and `type_args`,
  `QoreUnionTypeInfo::members`, `QoreComplexCodeTypeInfo::returnType` and `paramTypes`, and
  `QoreWildcardTypeInfo::bound`; vectors are returned as read-only views (`QoreOrphanablePtrVecView`).
- A gap found in review: the conversions of `softlist<T>` captured the element type in lambdas.  They read it from a
  shared orphanable cell now.  `QoreComplexReferenceTypeInfo`'s hard reference type, made on demand, is orphaned with
  its parent.
- **A retired derived type is orphaned from the declaration being retired.**  Its pointers to that declaration and
  to its own types are repointed to the tombstone.  Its pointers to other user declarations, which are alive, are
  left as they are, because code that compares declarations by identity must still find them.  (An earlier version
  orphaned every user declaration at once, and the substitution of `T` in `Box<*Foo>::set(T)` failed after `Foo`'s
  retirement, as the base class had become the tombstone of `Box`.)  They are recorded
  (`retired_derived_naming`: declaration to retired derived types) and orphaned when that declaration is retired in
  turn.  On the destructor path (a declaration destroyed, not retired), the destroyed declaration's own pointers are
  left, as before; they are compared by address and never read, as the declaration can be partly destroyed.
- The order is: the purge removes the derived types from the caches (under `ctl`), the orphaning stores run outside
  the lock (the tombstones are made under their own registry lock, which is taken before `ctl`), then the epoch is
  incremented and recorded for the derived types and, after `retireTypeOwner()`, for the declaration.  An epoch
  recorded is therefore always after the orphaning stores that it covers.

### Publishing the epoch

The 1a publish (`exchange(load(epoch))`) had a gap: a scan made between the thread's load of the global epoch and its
store reads 0 and frees, while the thread goes on with an epoch from before the retirement and can read the pointer
before it was orphaned.  `qore_publish_type_epoch()` stores the epoch and loads the global epoch again: in the total
order of sequentially consistent operations, a scan that read the slot before the store is ordered before the second
load, which then reads the retirement's epoch and synchronizes with its orphaning stores.

### The free (`qore_type_retirement_free()`)

A retired declaration is freed when:

1. its epoch is recorded (its derived types are orphaned) and every thread's epoch is 0 or at least it;
2. it is not unfreeable, and no declaration keeps it (`type_owner_kept_by`): a retired hashdecl that names it in its
   members (the former "forever named" marks, now counted per keeper and released when the keeper is freed), or a
   concrete declaration of a generic hashdecl whose type arguments name it (`HD<Foo>`, whose members name `Foo`'s types
   without references; `qore_type_owner_keep_type_args()`);
3. only its retirement holds a reference to it (the reflection API and #6489's references keep it).

The scan runs at the outermost exit of a thread from Qore code
(`ProgramThreadCountContextHelper::restoreContext()`, `~QoreProgramContextHelper()`), at the end of each quiescent wait
and from the debug hook `dbg_type_retirement_free()`; not after a retirement, as the retiring thread usually runs
Qore code since before it (it holds the retirement back itself) and can hold locks of the Program being deleted.  It
never waits; a free that releases a keeper rescans.  The
freed declaration is released through its normal last-reference path (`qore_type_owner_released()` returns false for
it), so its destructor runs the same purges and dispatch-epoch bumps as any other free.

From the start of library cleanup (`qore_cleanup()`, once every thread has ended: `qore_type_retirement_shutdown()`),
the declarations retired (those of the modules' Programs, which are deleted then) are kept until library cleanup frees
them, as before: no tombstone is made and nothing is orphaned or freed, as the process ends.

### Binary module declarations that Programs' declarations name (qore#7533)

Builtin (system and binary module) declarations are not decided with the declarations of Programs
(`isTypeOwnerSystem()`): they name each other directly and are freed with their namespace.  `qore_cleanup()` deletes
the modules (`QMM.cleanup()`, user modules before the modules that they require) before it clears the Programs that are
still referenced (`clearRemainingPrograms()`, such as a Program that holds itself in a global variable), so a binary
module's declarations can be freed while a declaration of a Program still names them, and the Program's
`QoreTypeOwnerBatch` read the freed types.  The declarations of the system namespace (`Qore::`) are freed with
`staticSystemNamespace`, after every Program, so only the original declarations of binary modules
(`isTypeOwnerModule()`) are referenced.

The rule is ownership, not the module context of a declaration's creation.  A builtin declaration that belongs to no
binary module is marked (`not_module_decl`, `setNotModuleDecl()`): the constructor of `StaticSystemNamespace` marks
every class, hashdecl and enum of the namespace tree that it has built, with the concrete declarations that its generic
hashdecls have made by then, such as `KeyValueInfo<auto>` (`mark_static_system_namespace_decls()`, before any other
thread exists); the pseudo-classes (`pseudo_classes_init()`) and the tombstones (`QoreTombstoneBuilder`) are marked
when they are made.  A copy of a hashdecl or enum, a concrete declaration made afterwards and an imported copy of a
class keep the mark of their source, and a class shares its private data with its copies in Programs; a concrete
declaration copies the mark rather than reading its generic's, as it does not reference its generic, which can be a
module's that it outlives.  Every other original builtin declaration is a module's, whenever it is made: module-jni and
module-python create builtin classes when they are first needed, after the module's initialization, with no module
context or with the context of a user module being parsed.  Copies are never module declarations themselves, as before
(`isTypeOwnerSystem()` is false for a hashdecl or enum copy).

The references follow compound types: a member of type `list<ModEnum>` or `hash<string, hash<ModHd>>`, a union, a
parameterized class or a code type names the module declarations of its element, member, argument, parameter and return
types (`qore_type_owner_add_module_names()`, which walks the orphanable views of the type arguments and parameter types,
as `QoreDerivedTypeOrphaner` does, and does not walk into the declarations that it finds nor into type parameters).  A
module class is the exception, one level deep: a class's type is shared with its copies, so a Program names the
module's original class, whose member, static variable, signature and type parameter types (and those of its module
base classes) are read by the code of the Program that calls it, so the module declarations that they name are
referenced with it (`getTypeOwnerModuleSignatureNames()`, added by `qore_type_owner_add_module_class_names()`, once
per class in a `QoreTypeOwnerBatch`), but not what those name in turn.  A Program names a copy of a module's hashdecl,
which references what its members name itself (below), and an enum names no declaration.  The derived-type retirement
does not cover them: a builtin declaration is never retired, so a derived type that names one is not what keeps it.
Three references close the window:

- A hashdecl copied into a Program shares the types of its source's members, so a copy of a module's hashdecl names
  the module's original enums, hashdecls and classes; a module's enum could then live on only through the constants of
  the Program, which `waitForTerminationAndClear()` releases before the batch reads the member types.  A hashdecl copy
  (and a concrete declaration of one) references the module declarations that its member types name, and those of its
  parents for as long as they are module declarations (a parent's members are read with the copy's), with what the
  module classes among them name in their own types, from when it is made (`copy_module_names`), and releases them
  last in its destructor, after its source (`copy_source`).  A copy of a copy reuses its source's list, and a module's
  hashdecl computes its list once for all its copies (`getBuiltinModuleNames()`).
- An enum copied into a Program references its original if the original is a module's (`orig_ref`), released last in
  its destructor: the module's C++ code makes values from its original's members (`QoreValue::makeEnum()`), and the
  code of a Program cleared after the modules can call it.
- Every other declaration of a Program references the module declarations that it names from the first batch that it
  is in (`QoreTypeOwner::type_owner_module_refs`: `getTypeOwnerNames()` lists them with the user declarations that it
  names directly, and `QoreTypeOwnerBatch::init()` takes them apart from the names, which only list the declarations
  of Programs) until it is destroyed (`qore_type_owner_destroyed()`), whether it survives, is retired or is freed: a
  class of a user module whose signatures name a binary module's class (a class's type is shared with its copies, so
  it is the module's original) survives the module's Program through its copy in another Program, which reads the
  signatures when it is deleted.  A hashdecl copy does not list them, as it references them itself.

So the batch of a Program cleared after the modules reads no freed module type: a declaration that it reads is either
a copy that references what it names, or one whose own Program's batch (which ran while the Program's copies or the
module kept the module declarations) took the references.  Builtin classes copied into a Program are builtin
declarations themselves and are not walked by the batch.

### Quiescent waits (1c)

`QoreTypeQuiescentWaitHelper` stores 0 in the thread's slot before the wait and publishes the epoch again after it,
unless the thread runs an unsafe frame (below); then it frees what can be freed.

| Wait | Where | Review |
|---|---|---|
| `Queue::get()`, `shift()`, `pop()` (also the result Queue of every synchronous `Socket` operation, which the async I/O controller runs) | `qore_queue_private::waitReadIntern()`, around the condition wait only | the queue lock is held; no type is read in the wait; the room and consume notifiers, which can run code, run after the lock and outside the quiescence |
| `Condition::wait()` | `condition_wait_impl()` | the smart lock is released and taken again inside; no type is read |
| `Counter::waitForZero()` | both variants | no type is read |
| `WaitGroup::wait()` | | no type is read |
| `thread_join()` | around `QoreThreadList::joinThread()` | no type is read |
| `sleep()`, `usleep()`, `Socket::poll()`'s sleep, the file lock retry | `qore_cancellable_sleep()` | no type is read |
| file, stream and backquote I/O | `qore_quiescent_cancellable_poll()` in `QoreFile.cpp`, `InputStream.cpp`, `OutputStream.cpp`, `BackquoteNode.cpp` | no type is read; not in `qore_cancellable_poll()` itself, as the AOT jobserver wait uses it in the middle of a compilation |

Not quiescent: the lock waits (`Mutex`, `RWLock`, `VLock`, `SmartMutex`, the lvalue locks), the `Queue` write waits,
and every wait of library code that is not listed.

### Frames that hold types read through derived types (the audit)

A wait is quiescent only if no frame of the thread holds a pointer that it read through a derived type and uses after
the wait.  The audit of `lib/` found:

| # | Holder | Handling |
|---|---|---|
| 1 | the call's inferred type arguments (`CodeEvaluationHelper::type_param_instantiation`, current as `runtime_type_param_instantiation`), read from the arguments' derived types (`T` = `Foo` from a `list<Foo>`) | unsafe frame |
| 2 | the substituted return type (`setReturnTypeInfo()`), read through the receiver's type arguments or #1 | unsafe frame (only in a frame with a receiver type or type arguments) |
| 3 | `LocalVarValue::type_substitution_cache` | reset when the slot is set for a new variable (`LocalVarValue::set()`), so it never outlives its frame; filled only in unsafe frames |
| 4 | the parameter types substituted for default argument processing (`Function.cpp`) | unsafe frame (the receiver is set before the arguments are processed) |
| 5 | `BGThreadParams::type_param_inst` and `receiver_type_info`, copied to a new thread, which publishes a later epoch | the new thread's frame is unsafe, and `BGThreadParams` holds `QoreExternalTypeDeclRefs` on what they name, as the starting thread's epoch is not inherited |
| 6, 7 | `QoreHashIterator::value_type` (from the iterate operator, `HashIterator`, `HashPairIterator` and their reverse forms): kept for the iterator's lifetime, so a persistent holder | `QoreTypeDeclPin`: references to the user declarations that the type names (no allocation and no lock for a type that names none) |
| 8 | the element types of `QoreListIterator`, `SingleValueIterator`, `QoreChannelIterator` (read only through the virtual `getElementType()`) | `QoreTypeDeclPin` |
| 9 | hmap and hmap-select: the common value type, which can outlive the last value of it (a duplicate key replaces it) | `QoreTypeDeclPin`, set when the type changes |
| 10 | running specialized code (IR, JIT) for `Box<Foo>`, whose IR and code embed `Foo`'s types; the running call holds the specialization, not `Foo` | unsafe frame: every entry into generic code goes through a frame that sets a receiver type or type arguments |
| 11 | closures and call references made in generic code (`QoreClosureTypeContext`) | #6489 (`QoreTypeDeclRefs`), which must land with or before this; the closure's frame is unsafe while it runs |

The frames that set a receiver type whose type arguments name user declarations (`QoreTypeInfo::isUnsafeReceiver()`,
one load of a flag set when the parameterized class type is made: a plain class receiver, the type of the object's
class, and a builtin generic such as `Queue<int>` are not counted), or infer type arguments that name user
declarations, are counted in `ThreadData::type_unsafe_frames` (`qore_type_unsafe_frame_add()`): `CodeEvaluationHelper` (when it sets a receiver type or infers type arguments),
`QoreClosureTypeContextHelper`, the JIT's `ReceiverTypeInfoGuard` and `TypeParamContextHelper`,
`BGTypeParamInstantiationHelper` and `HashDeclDefaultReceiverHelper`.  The count is changed only in generic code.  The
IR and AOT specialization guards of the lowering (`Function.cpp`, `QoreAOT.cpp`) and `IRClosureSpecializationGuard`
run no user code and are not counted.

An ordinary call runs develop's code in `CodeEvaluationHelper`: the frame is marked only in branches that generic calls
take.
- An explicit receiver is tested in `substituteCallerReceiverTypeParams()` with the value that it tested before: the
  contains-type-parameter cache of `QoreTypeInfo` (-1 not known yet, 1 or 0) also holds `QTI_CTP_UNSAFE_RECEIVER`, so
  the test of a value that is not 0 takes the receivers that can contain a type parameter or name user declarations;
  the type of a plain class is 0, and the other readers of the cache (the IR and JIT coercions of local variables)
  are unchanged.
- The receiver of a call with an object only (`setObjectReceiverTypeInfo()`, inlined as
  `qore_get_object_receiver_type_info()` was) is tested in the branch of an object instantiated with type arguments.
- Inferred type arguments are scanned after the test for a generic variant (`setTypeParamInstantiation()`).
- The mark and the restoration of the caller's type parameter instantiation share one byte (`generic_frame`), which the
  destructor tests once, as it tested the restoration before; `markTypeUnsafe()` is out of line and cold.
- `examples/test/qore/classes/type-retirement-unsafe-frames.qtest` checks the count in each kind of frame.

The acquire loads of the pointers of a `QoreTypeSpec` are not moved before a test, merged into a conditional move or
removed by the compiler, as the plain loads before them were: a getter whose callers usually find another kind loads
only after its test, and `getComplexHash()` and `getComplexList()`, which hash and list types ask, load first and select
(the union is always wholly set); they still take an instruction or two more than the plain loads on some paths.  The
rows of the AST interpreter's and the JIT's assignments to local variables depended on the inlining of
`LocalVar::tryGetLValue()` and of `getLValue()` in the JIT runtime, which the compiler stopped doing when the unity
chunks that compile them grew: `tryGetLValue()` is always inlined, and the JIT runtime uses `getLValueInline()`;
`getLValue()` itself is left to the compiler, which calls it from the IR interpreter's `execute()` (forcing it inline
there raised the register pressure of that function).

Checked and safe (no type read through a derived type held across user code): foreach; the functional value types of
select, streaming and range; map, select and mapselect (every value is kept in the result); sort with a callback; the
list and hash pseudo-methods; `LValueHelper` (index expressions are evaluated before the lvalue is navigated); the fold
and retype loops (the element type is read again in each iteration, and the hash being built pins its hashdecl); the
type checks themselves; the casts; references (`lvalue_ref` reads its target type through its derived type each time);
implicit argument types (parse time only).

### Threads that read types without a Program context

The orphaning and the tombstone creation run with the thread's epoch published (`QoreTypeEpochEntryHelper`), also
when the thread is outside Qore code.  Code in a thread that has no `ThreadData` (a foreign thread that is not
registered) cannot publish an epoch: a module that type-checks values in such a thread must register it and enter a
Program.

The audit of the threads that run without a Program context (the threads of `q_start_thread()` and the foreign
threads have `ThreadData` and no Program):

| Thread | Type work | Handling |
|---|---|---|
| the JIT's background compile thread (`QoreJIT::bgCompileThreadLoop()`; a raw thread, no `ThreadData`) | lowers queued IR, whose types the enqueuing thread read (a specialization for `Box<Foo>` names `Foo`'s types) | each work item holds a `QoreTypeEpochHold` armed with the enqueuing thread's epoch, which the scan counts as a thread, until the item is compiled or cancelled |
| async I/O threads and callback workers (`AsyncIoControllerPriv.cpp`) | builtin hashdecls with user objects as members; builtin call references | the user classes are those of live objects, which reference them; no derived type that names a user declaration is read |
| ONNX async threads (`modules/ml`) | `hash<auto>` lists of user hashes | builtin types only |
| `ThreadPool` cancel code, the signal thread's exception handling, the release of thread resources | dereferences, exception printing | no type check of a derived type |
| AOT batch workers (`qcc`) | code generation from the parsed Program | the Program outlives them, and no Program is deleted meanwhile |

Debug builds assert that a runtime type check (`QoreTypeInfo::acceptInputInternImpl()`) of a derived type that names
a user declaration directly is made in a thread that publishes an epoch (`qore_type_check_epoch_assert()`); a
declaration's own type is not checked, as it is read from an owner that keeps the declaration.  Modules outside this
repository that check values in threads of their own (registered with `q_register_foreign_thread()`) must enter a
Program for it.

### Child processes created with fork()

The locks of the scheme (the registries of tombstones, epoch holds and generic caches, the debug registries of types
and declarations, and the lock of the debug count of entries into quiescent waits) are acquired before `fork()` by fork
handlers, and the epoch hold of a background JIT compile that a child loses with its thread is released in the child
(qore#7549); see "Type retirement" in `design/fork-handlers.md`.  A free or a retirement that another thread had begun
at `fork()` is not finished in the child: the declarations that it had taken off the retire list stay allocated there.

### Bounding and reporting the delay

A long native call, or a thread blocked in a wait inside generic code with type arguments, delays the frees; it never
makes them unsafe.  `dbg_type_retirement_info()` reports the pending, freeable and freed declarations and the
tombstones; `dbg_type_retirement_blockers()` names the threads that hold the newest retirement back.

### Measured

Release RSS growth per Program generation (`measure.q` of step (b) with a quiescent point, `usleep(1)`, after each
generation, as a server thread reaches when it waits; 5000 generations after 200; GCC 15.2):

|Program|1a (`92cf90db85`)|1b+1c|
|---|---|---|
|a plain class|0.00 KiB|0.00 KiB|
|a generic class and hashdecl, unused|1.38 KiB|1.37 KiB|
|generic values of its own classes|10.47 KiB|4.34 KiB|

Debug builds count no retired declaration left per generation (`dbg_type_retention_breakdown()`); what is left is
six retired derived types per generation of the last Program (two type parameter types, two complex list types, a
complex hash type, `Box<Foo>`), which stage 2 would free.  Without a quiescent point (a thread that never waits and
never leaves Qore code), nothing is freed before the thread does, as before.

Callgrind instructions per iteration (`rows.sh` plus `bench-generic.q`, `bench-guardprof.q` and `bench-pseudo.q`, the
minimum of 5 allocation layouts, GCC 15.2, Release; the fix built with `-g`, which does not change the code),
develop `f446244fba` and the head at paths of equal length:

|Row|ast develop|ast fix|ir develop|ir fix|jit develop|jit fix|
|---|---|---|---|---|---|---|
|fc|14025|14025 (+0)|2893|2894 (+1)|315|315 (+0)|
|sc|13944|13946 (+2)|8119|8120 (+1)|3387|3381 (-6)|
|m|16362|16363 (+1)|10259|10260 (+1)|4946|4937 (-9)|
|fs|16674|16675 (+1)|9389|9389 (+0)|4553|4548 (-5)|
|mp|24334|24335 (+1)|14813|14816 (+3)|2042|2042 (+0)|
|mm|63232|63245 (+13)|44659|44661 (+2)|23520|23488 (-32)|
|tc|14875|14875 (+0)|14228|14234 (+6)|2218|2216 (-2)|
|tl|14545|14552 (+7)|7184|7186 (+2)|3044|3046 (+2)|
|tl0|14194|14201 (+7)|6534|6537 (+3)|1856|1855 (-1)|
|th|16228|16235 (+7)|12899|12914 (+15)|8445|8454 (+9)|
|go|34651|34725 (+74)|33525|33679 (+154)|24694|24822 (+128)|
|hd|19115|19094 (-21)|13112|13110 (-2)|6114|6114 (+0)|
|gp|29967|29980 (+13)|31980|31988 (+8)|17096|17103 (+7)|
|pv|9650|9655 (+5)|7845|7867 (+22)|2906|2913 (+7)|
|pl|8955|8960 (+5)|3740|3740 (+0)|258|258 (+0)|
|pt|9308|9313 (+5)|6062|6070 (+8)|3229|3236 (+7)|

`go` (an object of a generic class with a class type argument) pays its unsafe frame; the other rows pay the
acquire loads of the `QoreTypeSpec` getters (an instruction or two on some paths) and the remaining shifts of the
compiler's inlining in the unity chunks (see above).

### Known limits and risks

- A declaration whose tombstone cannot be made is kept until library cleanup, as before: one nested deeper than
  `QORE_TOMBSTONE_MAX_DEPTH` (64) declarations and types (ex: a class at the top of a chain of 40 diamonds, which is 80
  classes deep), and the kinds listed above.  A fingerprint stops as soon as it fails, as the declarations on its path
  are not memoized: a walk that went on would follow every path of a chain of diamonds.
- A frame counted as unsafe whose `CodeEvaluationHelper` constructor ends with a C++ exception (`std::bad_alloc`)
  after it was counted leaves the count raised: the thread's waits are not quiescent afterwards, which delays frees
  and never makes them unsafe.
- A declaration destroyed rather than retired (a class deleted with its class object, builtin declarations of an
  unloaded module) leaves the pointers that retired derived types have to it, as before.
- Out-of-tree modules that type-check values in threads of their own must enter a Program (debug builds assert it for
  derived types that name user declarations).
- The three stack-limit tests of `qore/threads` (`frame-check-stack`, `parse-stack-boundary`, `small-thread-stack`)
  overflow their deliberately small stacks under ThreadSanitizer's larger frames; there is no TSan report.

## Costs (estimates, to be measured)

- **Stage 1:**
  - outermost entry and exit: one load and one store (release) each;
  - type checks: acquire loads of the spec pointers, which compile to plain loads on x86-64 (`ldar` on AArch64);
  - quiescent waits: one store each.
  - Expected: tl, tl0, th, go and hd within noise (±0.1%); fc, sc, m, fs, mp and mm unchanged; a small one-off cost
    per Program deletion (a scan of the thread list).
- **Stage 2:** an atomic increment and decrement per value of a mortal derived type.  The baseline rows use builtin
  element types, so they should only show the flag branch.  A new row with containers of a user class type
  (`list<Foo>`) would show the counting cost; I estimate it at 20-40 cycles per typed container created and
  destroyed.

## Proposed order

1. **Stage 1a.** Pending list, global epoch, `type_epoch` publishing at outermost entry and exit, the counters and
   the debug hooks.  Nothing is freed yet, and the pending items are freed at cleanup as today.  The test checks the
   counters and that a scan finds a blocked thread.
2. **Stage 1b.** Orphaning of every derived-type pointer to a declaration (atomics, tombstones) and freeing
   declarations after the grace period.  The #6419 test gets its expected-zero assertion for the declarations.
   valgrind and TSan run on the retirement tests and the generic suites in every mode.
3. **Stage 1c.** Quiescent waits at the builtin-level blocking calls, each reviewed against the inventory.
4. **Measure** what is left per generation (only derived type shells), and decide whether stage 2 is worth its
   complexity.
