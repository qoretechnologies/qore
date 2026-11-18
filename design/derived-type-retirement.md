# Freeing retired derived types and the declarations they name

Issue: [#6419](https://github.com/qoretechnologies/qore/issues/6419) (step (a): design only; nothing here is
implemented).  Related: #5790 (declaration retirement), #5765 (generic caches), #6421 (guard profile type identity).

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
