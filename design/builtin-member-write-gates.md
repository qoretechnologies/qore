# Builtin external member gates

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Issue #5511 adds `QoreClass::setMemberWriteGate(q_member_write_gate_t)` for builtin classes whose dynamic members live
outside the Qore object. Register the callback before class initialization/publication. Registration is immutable at
runtime. The registration class ID travels with the callback, so a Qore or builtin subclass supplies the original
base class's private data rather than looking for data under the subclass's ID.

## Dispatch and inheritance

A write gate handles only names absent from the complete member declaration map. That map includes inherited private
and internal declarations: they retain ordinary storage, type checks, and access restrictions. The first inherited
write gate wins unless the subclass registers its own. Source initialization, builtin initialization, AOT class commit,
and Program class copies preserve the registration.

Builtin `memberGate()` methods are also inherited unless the subclass declares its own gate. User-defined read gates
and notifications keep their existing non-inherited behavior. Self-member reads, including optimized native getters,
use the builtin read gate for undeclared members. A subclass's own gate takes precedence during parsing and commit.

## Lvalue lifetime and execution

Plain assignment binds a temporary without reading the external member. Operations that need the previous value read
through `memberGate()`. Nested hash/list assignments read the containing member, apply ordinary copy-on-write, and
write the changed container back. These paths use `LValueHelper` in AST, IR, JIT and AOT execution.

The temporary owns the receiver and current value. It detaches from any earlier lvalue lock before invoking a gate.
Object-count bookkeeping finishes before the write callback runs. If evaluation has already failed, no write occurs.
A successful mutation invokes the callback after locks are released; an exception from that callback reaches the
original operation. Owned operator results are released when a late callback failure unwinds IR or a native throwing
wrapper.

Crossing an object boundary is different from modifying a containing hash/list. `record.child.quantity = 2` mutates the
child object and does not replace the parent's reference to it. The parent temporary stays alive as needed, but its
write-back is disabled. A gate on the child's member becomes the new write-back target.

Read/modify/write is not atomic across threads. The callback must synchronize its external storage; synchronization
inside each callback does not make the whole sequence atomic. No Qore object or lvalue lock may be held while invoking
external code. Callback re-entry into Qore is supported. Module callbacks must honor the caller's sandbox/domain
restrictions and cooperative cancellation requirements when accessing external resources or blocking.

## Ownership and removal

`QoreMemberWriteOp` distinguishes `Assign`, `Remove` and `Delete`. The callback borrows its receiver, registration-class
private data, member name and input value. The runtime retains the private data for the call. A module storing the
input value must take its own reference. The constructor must register private data in the usual way with
`QoreObject::setPrivate()`.

For `Assign`, return `NOTHING`. For `Remove` or `Delete`, remove the external member and return an owned previous value;
the input value is `NOTHING`. `remove` returns that value to Qore code; `delete` applies Qore's normal deletion semantics.
A callback returning a value while raising an exception has that value released by the runtime. Missing external
members can return `NOTHING`; removal slices omit external entries returning `NOTHING`. Declared-member slices retain
their existing presence checks. Slice processing checks cancellation every ten callbacks.

The runtime does not discover the references kept in external storage. A module that keeps Qore objects in its private
data must use `AbstractScannablePrivateData` and the existing private-data graph notifications. The regression fixture
demonstrates this and verifies collection of a cycle through external members.

## Tests and follow-up

`examples/test/module-cpp-api/member-write-gate/` uses the native `cppapiuser` test module and checks all execution modes,
including a source-stripped compiled fixture. It covers typed/object/auto receivers, builtin and Qore inheritance,
self access, arithmetic, conditional writes using existing `??`, dynamic keys, nested containers and objects, references,
private/internal declarations, re-entry, removal/deletion, late failures and cycle collection.

The proposed `??=` syntax is separate work in issue #5512. This change adds no language operator and makes no parser or
formatter grammar changes.

## Declared members with accessors

`QoreClass::addBuiltinMemberWithAccessors(name, access, type, get, set, ptr, del)` declares a typed member whose
value lives outside the Qore object, as `addBuiltinStaticVarWithAccessors()` does for static variables. The jni
module uses it for Java instance fields. The declaration (`QoreMemberInfo`) holds a shared `QoreMemberAccessors`
(getter, setter, module data, the declaring class ID for the private data, and the deleter, which runs when the last
copy of the declaration is destroyed); inherited copies of the member share it. A class that declares or inherits
such a member has `qore_class_private::has_member_accessors` set, so member access looks for accessors only in
those classes (`findAccessorMember()`).

Such a member is left out of the member initialization list, so the object never holds a value for it. Declared
member access then dispatches to the accessors at the same points that dispatch undeclared members to the gates:

- reads: `QoreObject::evalMember()`, `getReferencedMemberNoMethod()` and its resolved and prehashed variants used by
  IR, JIT and AOT, self reads, slices, `getMemberAsBigInt()` and `hasMember()`; the getter's value is checked
  against the declared type;
- writes: `getLValue()` and the resolved/prehashed lvalue variants bind the member to the same `MemberLValue`
  temporary as the write gate, typed with the member's declared type, so assignments are type-checked before the
  setter runs; compound and nested assignments read through the getter first; the setter runs after the lvalue locks
  are released; `tryAssignScalarMemberResolvedPrehashed()` declines such members so the lvalue path is used;
- removal and deletion read the value with the getter and then call the setter with `NOTHING`
  (`evalExternalMemberWrite()`), also for slices and the C++ API (`takeMember()`, `deleteMemberValue()`);
- `setValue()` and hash merges into the object call the setter, merges after releasing the object lock and the
  lvalue lock.

The member is not part of the object's data: keys, hash conversions, iteration, copies and serialization do not
include it, and `getMemberValueNoMethod()`, which returns a borrowed value under the object lock, returns no value.

`examples/test/module-cpp-api/member-accessors/` checks typed, `object` and `auto` receivers, a Qore subclass, an
object created from C++ without the Qore constructor, type errors on write and read, setter errors, private access,
`self`, reflection, the C++ API, slices, merges and references, in AST, IR, JIT and tiered execution and from a
source-stripped compiled module.
