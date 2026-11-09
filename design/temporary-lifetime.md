# The lifetime of temporary values

Copyright (C) 2026 Qore Technologies, s.r.o.

A temporary - a value that an expression creates and that nothing else refers to, such as the object that a call
returns in `getDocument().components.schemas.Pet.getDataType()` - lives until the end of the statement that created
it, in every execution mode (David, 2026-10-25: "all chains, whole statement").  Every part of a chain of member
accesses, element accesses, casts and method calls can therefore use it, including code that reaches it through a weak
reference (an OpenApi3 `SchemaObject` refers to its document weakly), a closure created by one of its methods, or a
call reference to one of its methods.

When a statement ends, the temporaries that it still holds are released in the reverse order of their creation.

A temporary passed to a call as an argument is released when the call returns, as the AST interpreter releases the
argument list of a call (`CodeEvaluationHelper`): the IR interpreter releases the arguments that a call consumes
(`releaseConsumedCallArgs()`, `consumeLastUseCallArgSlots()`), native code clears their cleanup slots
(`QoreIRToLLVM::releaseConsumedCallArgs()`, the `_consume_args` helpers).  An argument created by a call (a function,
method or closure call or an object construction) and a value read from a variable that the read gave a reference of
its own to (see [Values read from variables](#values-read-from-variables)), in the block or the temp scope of the
call, are released there; a value borrowed from a variable or a constant has no reference of the frame to release, and
a loop invariant is defined in a temp scope of its own.

## Statement boundaries

These parts of a statement are statements of their own: their temporaries are released when their value has been
computed, before the code that follows runs.

| Part | AST interpreter | IR lowering |
|---|---|---|
| every statement | `AbstractStatement::exec()` | `lowerStatementBlock()`: `PushTempMark` / `DiscardTemps` |
| condition of `if`, `while`, `do`, `for` (each evaluation) | `qore_eval_condition()` | `PushTempMark` / `DiscardTemps` around `lowerConditionValue()` |
| initializer and iterator of `for` | `ForStatement` | `PushTempMark` / `DiscardTemps` |
| list of `foreach` | `ForEachStatement` | `lowerExpressionInTempScope()` |
| value of `switch` (the value is kept for the cases) | `SwitchStatement` | `lowerExpressionInTempScope()` |
| return expression (before `on_exit` handlers and local destruction) | the return statement | `DiscardTempsKeep` before `emitBlockCleanups()`, when the expression creates a value with a call and a handler or a local that can hold an object runs before the `Return` (`returnCleanupObservesError()`); otherwise the `Return` releases them, which no code can tell apart, and callee summaries keep the plain body form they recognize |
| `on_exit`, `on_success`, `on_error` handler | `StatementBlock::execIntern()` | the handler's statements |
| operand of a change to a container: assignment to a member or element, compound assignment (`+=` ...) except to a variable of a type without references, `push`, `unshift` | `QoreMutationOperandHolder` | `lowerMutationOperand()` |
| key or index of an lvalue | `LValueHelper::doHashObjLValue()`, `doListLValue()` | `lowerExpressionInTempScope()` |
| expression of a `map` or the map expression of a map-select whose value is not used (each element) | `QoreScopedEvalHolder` | per-iteration `PushTempMark` / `DiscardTemps` |
| expression of a `background` operator, in the new thread | `op_background_thread()` | the same (the thread evaluates the expression) |
| default value of a parameter, initializer of a class member, of a static class variable and of a hashdecl member, which the callee or the code that creates the object or hash evaluates while the caller's statement runs | `QoreScopedEvalHolder` (`CodeEvaluationHelper::prepareDefaultArgs()`, `qore_class_private::initMember()`, the static variable initializer, `typed_hash_decl_private`) | evaluated without the caller's temp scope |

An expression evaluated again within the same statement - the expression of a `map` whose value is used, of a
`select`, of `foldl`, also when a `foreach` statement gets the elements of a `map` one by one - keeps one temporary
per expression: the one from the previous element is released when the next one is created (the IR interpreter
overwrites the value slot of the instruction; native code overwrites the cleanup slot), so a loop never accumulates
temporaries; the last one is released at the end of the statement.

## AST interpreter

`QoreStatementTempScope` (`include/qore/intern/QoreStatementTemps.h`, `lib/AbstractStatement.cpp`) holds the
temporaries of the innermost scope of the thread.  The chain nodes hand their receiver to it when it is a temporary
(`QoreStatementTempScope::holdReceiver()`):

- `QoreHashObjectDereferenceOperatorNode` (`.member`, `{key}`, slices)
- `QoreSquareBracketsOperatorNode` (`[index]`)
- `QoreDotEvalOperatorNode` (method calls)
- the container casts (`QoreHashDeclCastOperatorNode`, `QoreComplexHashCastOperatorNode`,
  `QoreComplexListCastOperatorNode`; a class cast gives its operand itself)
- `ParseObjectMethodReferenceNode` (`\obj.method()`)

A value borrowed from a variable is not held by these nodes, and a value that a read of a variable gives a
reference of its own to is held by the variable reference itself (see [Values read from
variables](#values-read-from-variables)).  Nor are strings, numbers, binaries and dates, which hold no other values,
so their lifetime cannot be observed.

## Values read from variables

A read of a variable that gives the code a reference of its own to the value gives a temporary of the statement,
which lives until the statement ends like any other temporary (David, 2026-11-09, qore#5638: "we cannot lose
expression results needed to execute a statement while the statement is being evaluated").  These reads are:

- a read through a reference: a local or a parameter that holds a reference (`auto v = \o;`, a `reference<T>`
  parameter, a reference to a member of `self` or to a global), also when the variable that holds the reference is
  closure-bound or holds a reference to another reference;
- a read of a closure-bound local, a global or a thread-local variable, which is read under the variable's lock; a
  local that a reference is taken of (`\o`) anywhere in its function is read this way in the whole function, also by
  a statement that does not use the reference;
- a read of a static class variable (`S::s`, `StaticClassVarRefNode`, `LoadStaticVar`) or of a member of `self`
  (`m`, `SelfVarrefNode`, `LoadSelfMember`), which is read under the lock of the class or the object.

The first change for qore#5638 (033fd1f8cd) said that static class variables and members of `self` already gave
the same results in every mode: that was so only for a receiver.  As an operand the AST interpreter released such a
read when the operator returned while the other modes held it, and as an argument native code (JIT, tiered, a qcc
executable) held it to the end of the statement; both now follow the rule below.

The statement holds the value as long as it uses it, by what it is used for (David, 2026-11-09):

- the receiver of a chain (a method call, a member or element access, a cast, a call reference and the callee of a
  call through a call reference), the operand of an operator (`!v`, `v && ...`, `v ? ... : ...`, `exists`,
  `instanceof`, a comparison, `keys`, `elements`, `??`, `?*`, a slice), the source of a functional operator, the
  value of an assignment, an element of a literal and the value of a `return` statement live until the end of the
  statement (a `return` statement releases its temporaries before the `on_exit` handlers run).  So in
  `auto v = \o; string s = v.name() + (o = NOTHING) + mark();` the object that `v` gave is destroyed after `mark()`
  has run, also when a call later in the statement throws;
- an argument of a call - of a function, a static method, a method, a method of `self`, a closure, a call reference,
  a constructor, a builtin function, also of a builtin conversion that compiled code runs inline (`boolean()`,
  `int()`, `float()`, `string()`) - is released when the call returns, as every argument is: the call cannot use the
  value after it returns, and a callee that keeps it holds a reference of its own.  So in
  `string s = f(v) + (o = NOTHING) + mark();` the object is destroyed by the assignment, before `mark()` runs.

An expression evaluated for each element of a `map` keeps the value that it read last only, as for any other
temporary.

The IR interpreter and native code keep the result of such a load in the temp scope of the statement until
`DiscardTemps` (a load through a reference is owned: `qore_local_reference_eval()`, `LoadLocal`).  The AST
interpreter holds it in the innermost `QoreStatementTempScope` (`VarRefNode::evalImpl()`,
`SelfVarrefNode::evalImpl()`, `StaticClassVarRefNode::evalImpl()`, `QoreStatementTempScope::holdRead()`), which takes
a reference of its own: the code that evaluated the variable keeps its reference, so a value that it keeps after the
scope ends (the scope of a condition, or of an operand that is a statement of its own) stays valid.  Handing the
read's reference over to the scope instead would make the value borrowed from the scope, which ends before the code
that evaluated it is done in the scopes that keep their value (a switch value, an lvalue key, an operand of its own,
the class cast that gave its operand itself, a26c622cb7), so it is not done.  A scope holds its first four values in
storage of its own (`InlineCapacity`), so a statement allocates no memory for them.

A value that a callee, or the code that creates an object or a hash, evaluates while the caller's statement runs -
a default argument, a member initializer, a hashdecl member default, a static variable initializer - is evaluated as
a statement of its own (see [Statement boundaries](#statement-boundaries)): a read that it makes is not a temporary of
the caller's statement, which it would otherwise be held by (`dflt() + (gd = NOTHING)` with `*E e = gd` destroyed the
object after the caller's statement in the AST interpreter only).

An argument is released at the return of the call:

- AST interpreter: the argument list of a call reads a variable given as an argument without handing the value to
  the statement (`evalCallArg()` of `VarRefNode`, `SelfVarrefNode` and `StaticClassVarRefNode`, used by
  `qore_list_private::eval()`; a declaration that creates an object, `VarRefNewObjectNode`, overrides it with a normal
  evaluation), so the argument list holds the only reference that the read took and releases it when the call returns
  (`CodeEvaluationHelper`);
- IR interpreter: a value read by `LoadLocal`, `LoadArg`, `LoadClosure`, `LoadGlobal`, `LoadThreadLocal`,
  `LoadSelfMember` or `LoadStaticVar` and used
  once, as an argument of a call in its block or temp scope, is consumed by the call as a temporary created by a call
  is (`buildValueUseCounts()`: `consumed_call_args`); only a read that owns its value has a cleanup entry to release;
- native code: such a read is released by `QoreIRToLLVM::releaseConsumedCallArgs()` when the call returns, or passed
  to a `_consume_args` helper with its cleanup slot.  A read through a reference keeps its reference in the slot of
  its read site (`emitReferenceLocalRead()`), which holds the cached value's predecessor when the local holds no
  reference, so the slot is released only if it holds the value read (`qore_rt_release_reference_read()`).  A read
  that a later write to the local can release before its last use takes a second reference of its own, in a cleanup
  slot of the value (`overwritten_local_reads`: `f(v, \x)`, `f(v, (v = x))`), which is released as well;
- a builtin conversion that the lowering runs inline (`lowerBuiltinTypeConversion()`: `ToBool`, `ToInt`, `ToFloat`,
  `ToString`) releases its argument after it with `ReleaseOwnedTemp`, as the call it replaces would.

These reads are not held:

- a value borrowed from the variable: a local that holds the value itself, and a variable that holds a weak
  reference, which reads as its target; no mode keeps such a value after the variable is assigned another value in
  the same statement (`o.name() + (o = NOTHING)` and `(o && (o = NOTHING))` destroy the object before the rest of the
  statement runs).  The IR interpreter reads a local's value from its slot cache with a reference of its own, which
  kept the operand of an operator to the end of the statement; a load of an IR-only local that is used once, as the
  operand of an operator whose value keeps no reference to it (`ToBool`, `Not`, a comparison, `InstanceOfBool`,
  `Exists*`, `Elements*`, `IsNullOrNothing`), and that no write to the local can release before it is used, is
  borrowed as the base of a method call is (`interpreter_borrowed_bases`, `qore_ir_operator_keeps_no_operand()`);
- the reads made while a reference is resolved: the lvalue expression of a reference is evaluated in the context of
  the code that created the reference, with the temp scope of the statement suspended
  (`QoreStatementTempScopeSuspension`, the first member of `RuntimeReferenceHelperBase`), so the variable that it
  names and the temporaries of its keys and indexes are released when the reference is resolved, and the read through
  the reference is held as a whole by the variable reference that read it.  The suspension also keeps a reference
  that compiled code resolves (`qore_local_reference_eval()`, `ClosureVarValue::eval()`) from holding a value in the
  scope of the AST statement that called the compiled code.

## IR interpreter and native code

All owned instruction results are kept in the cleanup vector (interpreter) or cleanup slots (native code) of the
statement's temp scope until `DiscardTemps`.  Method call receivers used to be released when the call returned
(`releaseDeadMethodCallBase()` in the interpreter, `is_dot_eval_base_release` in `QoreIRToLLVM`, from 36643fca8a);
that release was removed, so a method chain keeps its receivers to the end of the statement as well.  Only the strong
reference that a load of a weak local variable takes is still released at its last use (`weak_load_result_ids`,
`weak_load_temp_slots`): in the AST interpreter, a variable read is not held either.

A method call reference to a temporary (`\make().getName()`) is created from the receiver that the lowering
evaluated (`qore_rt_create_object_method_ref()`, `ParseObjectMethodReferenceNode::makeReference()`); the interpreter
and JIT code used to evaluate the AST node again, which evaluated the receiver expression a second time.

## Exceptions and function exits

When a statement raises an exception that a handler in the same function catches, its temporaries are released before
the handler runs (the IR interpreter's `cleanupToTempScope()`, native code's `QoreIRToLLVM::getExceptionEdgeTarget()`).
When the function is left, the temporaries of the statements still running and the iterators of the `foreach`
statements left are released before the `on_exit` / `on_error` handlers that the function exit runs
(`QoreIRToLLVM::emitExitStatementCleanup()`), as the AST interpreter releases them when the statements unwind.

## Leaving a `foreach` statement

The AST interpreter's `foreach` statement (`ForEachStatement::execImpl()`) destroys its iterator, which holds the
list, and then its loop variables when it ends, however it ends: so a `return`, a `break` or an exception that leaves
the loop releases the list and the variables before the code after the statement runs - the `on_exit` and `on_error`
handlers of the enclosing blocks, the statement after the loop, or the handler that catches the exception.  The IR
lowering does the same for a value `foreach` whose list or variables can hold an object (a typed loop over a list of
scalars needs nothing):

- `return`: a `ForeachSource` cleanup entry, below the `ForeachElement` entry and above an `Lvars` entry for the loop
  variables, makes `emitBlockCleanups()` release the reference that the loop holds to the current element, the
  iterator (`IteratorRelease`) and the list (`ReleaseOwnedTemp`), then the variables.  `break` and `continue` do not
  visit these entries.
- `break`: the loop exit releases the iterator, which an exhausted loop has released already, and the list before the
  variables.
- an exception that leaves the body for a handler of the function goes to `foreach.exception.cleanup`
  (`emitForeachExceptionCleanup()`), which releases them and goes on to the handler.  A block with handlers anchors an
  exception target when it encloses a scope with locals (`mayHaveNestedScopeLocals()`), which a `foreach` with loop
  variables now is.
- the lazily evaluated `foreach` over `map`, `select` and the other functional operators
  (`lowerForeachLazyPipelineFused()`) does the same for the source and its iterator.

`IteratorRelease` and `ReleaseOwnedTemp` release on the path that they are on only, and clear the slot, so the
release at the end of the frame finds nothing; they do not change the native code generator's compile-time slot maps,
which the other paths of the loop still use, and do not check for an exception, since they also run on the path of
one.  A return statement that reads a variable that can hold a weak reference (a loop variable, or one assigned with
`:=`) has a temp scope of its own (`expressionReadsWeakCapableLocal()`): the load takes a strong reference, which is
released with the statement's temporaries, before the variable.

## Tests

`examples/test/qore/vars/temporary-lifetime/` runs every case in `temporary-lifetime-checks.qi` in this program, in a
worker in each execution mode, as an AOT program and from a compiled module, and compares the traces with the AST
interpreter's (`temporary-lifetime-results.qi`).  `examples/test/ir/ReferenceReadLifetime.qtest` runs the reads of
[Values read from variables](#values-read-from-variables) in every execution mode, on the IR tier and as a qcc
executable.
