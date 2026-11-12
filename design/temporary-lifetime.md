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
call, are released there; a value borrowed from a variable or a constant has no reference of the frame to release.

Only a value defined again for each execution of the call is released by it.  Loop-invariant code motion moves a read
of a local or a parameter that a loop does not write (`qore_ir_is_hoistable_query_load()`) to the preheader of the
loop, and the moved instruction keeps the temp scope of the statement in the loop body that it came from, so neither
the static use count (one use) nor the temp scope tells a hoisted read from an argument defined in an earlier block of
the statement (a call in a `try` block is an invoke that ends its block).  `qore_ir_find_reused_call_args()` gives the
call arguments defined in another block than the call whose block can run again without the defining block running
in between (over the normal and the exception edges, searched within the strongly connected component of the call's
block; an argument defined outside that component is reused without a search); no call releases them or takes their
slot.  The IR interpreter counts such an argument one use more in its use counts (`buildValueUseCounts()`), so
`consumed_call_args`, `consumeLastUseCallArgSlots()` and the moves of a value used once leave it alone with no check at
run time; native code leaves it out of `releaseConsumedCallArgs()` and of the cleanup array that a `_consume_args`
helper or `qore_rt_clear_arg_cleanups()` releases (`buildArgCleanupArray()`).  Such a read lives until the end of the
statement that the preheader belongs to.

The answer is a property of the function, cached for every thread that runs it, so it must not depend on the thread
that computes it: the IR interpreter runs the analysis without cancellation check points, as its other analyses, and
native code fails the compilation when it is cancelled.  Before, a cancelled analysis gave every value as reused, and a
function first called by a thread whose cancellation was pending (one that caught the THREAD-CANCELLED exception) never
released an argument at the return of its call again, in any thread
(examples/test/ir/ReusedCallArgumentCancel.qtest).  Before, `foreach string m in (l) { m.find(text); }` (since the
release of variable reads at call return) and `foreach string m in (l) { toupper(text); }` (before it) released the
hoisted read of `text` at the first call, and every later iteration gave the call NOTHING in the IR interpreter and
native code (qore#5638; examples/test/ir/LoopInvariantCallArgument.qtest).

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

The operators hand the operands that they evaluate to it in the same way, as soon as each is evaluated
(`QoreOperandEvalHolder`): the arithmetic, bitwise, shift, comparison, logical, regex, `keys`, `elements`, `exists`,
`instanceof` and unary operators, `?:` its condition, `[]` its index, `{}` its key and a scalar cast its
operand.  Holding a value when it is created, as the compiled modes
register a temporary when the instruction that creates it runs, gives the same order of release: the right operand
before the left one, and the operand that an operator created for an earlier element of a functional operator when
the operand is evaluated again.  So in `f(h + {"a": o}) + (o = NOTHING) + mark()` the hash literal, an operand of
`+`, lives until the end of the statement, and the object is destroyed after `mark()`, as in the other execution
modes (David, 2026-11-10, qore#5712: "all chains, whole statement").  An operand that the operator takes into its
result is not held, and the operands of a functional operator's expression are held by the scope of each element.  When
an exception leaves the statement, these operands are released with the receivers, after the argument lists that the
exception interrupted; the compiled modes order them the same way (`qore_ir_build_exception_release_groups()` puts the
operands of the corresponding instructions in the receiver class).

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
  `instanceof`, a comparison, `keys`, `elements`, `??`, `?*`; not the sequence of a slice, which the slice releases
  when it has been made), the source of a functional operator, the
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

A read of a plain local (`qore_ir_is_plain_local_read()`: not through a reference, not of a closure-bound or a
top-level local, in a function that creates no reference; its storage can be IR-only or AST-visible, as for the
locals of a statement in a `try` block, whose calls carry their expressions) holds a reference of the frame, while
the AST interpreter borrows the value from the variable; the two differ only when the statement writes the variable
after the read, which destroys the value in the AST interpreter (qore#5712):

- a read that is a call argument is released when the call returns (`read_needs_release_at_return()` in the IR
  interpreter's `buildValueUseCounts()`);
- a read whose uses are all later in its block and only take values from it - an element of a list or hash literal
  (`MakeList`, `MakeHash`, `MakeHashConstKeys`), the receiver of a member read (`HashKeyAccess`) and an operand of
  `AddAny` - is released after its last use when the statement can write the local later, in the read's block or in a
  block that the statement continues in (a `try` block, `&&`, `||` and `?:` split a statement into blocks):
  `QoreIRFunction::releasePlainLocalReadsBeforeWrites()`, which runs after `computeIROnlyLocals()`, inserts
  `ReleaseOwnedTemp`; a read whose local is declared with, or whose value facts give, a type that holds no other
  values (an int, a float, a bool, a string, a date, a binary or a number) is skipped, as its release cannot be
  observed (`qore_ir_value_may_be_observable()`).  So `f((o,)) + (o = NOTHING) + mark()` and
  `o.n + (o = NOTHING) + mark()` destroy the object before `mark()` runs.  A slice is not such a use: as the source of
  a functional operator, the AST interpreter iterates the sequence itself (`map $1, l[0..(l = ()).size()]`).

A list or hash literal, the result of `AddAny` and the value that `remove`, `shift` or `pop` takes, passed to a call
as its argument, are released when the call returns, as every argument is, when that release can be observed: when
they hold a value that nothing else holds, such as an object that the expression created or the value of a local that
the statement writes later (`qore_ir_find_observable_temp_call_args()`, used by the IR interpreter and by
`QoreIRToLLVM::releaseConsumedCallArgs()`).  A literal of scalars, constants and reads of locals that keep their values
is released at the end of the statement, which costs the call nothing.

A method call reference to a temporary (`\make().getName()`) is created from the receiver that the lowering
evaluated (`qore_rt_create_object_method_ref()`, `ParseObjectMethodReferenceNode::makeReference()`); the interpreter
and JIT code used to evaluate the AST node again, which evaluated the receiver expression a second time.

## Exceptions and function exits

When a statement raises an exception that a handler in the same function catches, its temporaries are released before
the handler runs (the IR interpreter's `cleanupToTempScope()`, native code's `QoreIRToLLVM::getExceptionEdgeTarget()`).
When the function is left, the temporaries of the statements still running and the iterators of the `foreach`
statements left are released before the `on_exit` / `on_error` handlers that the function exit runs
(`QoreIRToLLVM::emitExitStatementCleanup()`), as the AST interpreter releases them when the statements unwind.

## Functional operators in an expression

The predicate of a streaming operator (`takewhile`, `first`, `count` ...), of `select` and the expressions of a
map-select run with the element as the implicit argument (`$1`) and its index as `$#`
(`QoreIRLowering::lowerWithImplicitContext()`).  Their temporaries stay in the statement's temp scope, one per
expression, as for the AST interpreter's receivers: the code records a temp scope that no `PushTempMark` opens
(`QoreIRBuilder::allocateUnmarkedTempScopeId()`), so the normal path has no per-element mark or drain, an exception
that the code raises releases nothing before the operator's cleanup, and the arguments that it creates for a call are
still the call's.

An exception that leaves the operator releases, in the AST interpreter's order, the element, the value that a map
expression gave last, the list built so far, the iterator and the source, then the statement's temporaries
(`FunctionalLoopExceptionCleanup`); an exception that the iterator's `next()` raises releases the iterator and the
source before the list (`routeNext()`), as the AST interpreter's functional iterator ends as if the source were
exhausted.  The cleanup exists only if a handler of the function encloses the operator: an exception target gives
the operator's calls and operations their invoke form, which the IR interpreter runs much more slowly, so an operator
with no handler keeps the form it has always had and an exception leaves the function directly.  The function's exit
then restores the implicit arguments and elements open at the raising instruction, innermost first, before it
releases the frame's values (`qore_ir_find_open_implicit_contexts()`: the IR interpreter's frame exit, native code's
`QoreIRToLLVM::getImplicitContextRestoreEdge()`, which has the instructions on the error edge only), and releases the
operator's iterator before the handlers of the blocks left (`QoreIRInstruction::restore_on_exception_exit`).

A streaming operator that stops before its iterator is exhausted (`take` at its count, `takewhile` at an element,
`first`, `any`, `all`) releases the element it stopped at and its iterator when it returns, and its source like map
and select (`lowerWithSourceRelease()`).

`examples/test/ir/streaming-element-cleanup/` compares the release order with the AST interpreter's in every mode
and with AOT.

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

The list of a `foreach` statement that is `keys`, a slice (`seq[a..b]`) or a list of indices without a range
(`seq[i, j]`) is iterated by the AST interpreter with the iterator of the operator, which keeps the hash, or a list
sequence, until the loop ends, while the other temporaries of the list expression are released before the body runs
(the keys of an object are copied first, and a one-index or non-list value is made first).  The IR lowering
(`getForeachSource()`, `lowerForeachSourceIterator()`) creates the same iterator with `IteratorCreateSource`, from
the operand and indices that it evaluates in a temp scope of its own, which ends once the iterator is made; the
runtime helper calls the operators' own functions (`QoreKeysOperatorNode::getKeysIterator()`,
`QoreSquareBracketsRangeOperatorNode::getSliceIterator()`, `QoreSquareBracketsOperatorNode::getIndexesIterator()`),
and the iterator has the `ForeachSource` entries above.  Only an operand that can hold an object is iterated so: the
list of any other operand is built first, since its release cannot be observed (qore#5770,
`examples/test/ir/foreach-source-lifetime/`).

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
executable. `examples/test/ir/temp-release-points/` runs the release points of reads of locals that the statement writes later
and of the temporary arguments above (qore#5712).
