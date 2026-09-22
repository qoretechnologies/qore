# Closure-bound local variables

A local variable that is passed by reference (`\var`), captured by a closure, or used in a `background` expression is
*closure-bound*: its value does not live in the frame but in a heap-allocated `ClosureVarValue`
(`include/qore/intern/LocalVar.h`), so that the reference, the closure or the other thread can use it after the
expression that took it, and from other threads. The parser decides this for the variable as a whole
(`ParseReferenceNode::parseInit()` and the closure parser call `VarRefNode::setThreadSafe()`), so the variable is
closure-bound in every execution of the function, whether or not the reference or closure is ever shared.

A `ClosureVarValue` is an `RObject`: it has a read-write lock (`rml`), and a write that changes the objects its value
reaches starts a recursive-reference scan rooted at the variable (`LValueHelper::~LValueHelper()`, see `dgc.md`).
That scan walks the whole graph reachable from the value, taking the r-section of every object in it, so a variable
that holds a shared object - a table, a listener, a connection - contends with every other thread using that graph.

## Who can reach a closure-bound variable

- The frame that created it, through its entry on the thread's closure-variable stack
  (`ThreadClosureVariableStack::instantiate()`). The entry holds a reference until the variable goes out of scope.
- A reference to it: `ClosureVarValue::getReference()` returns a `VarRefImmediateNode` that holds a reference.
- A closure that captured it: the closure's environment holds a reference.
- The closure-variable stack of a thread that runs such a closure (`CVecInstantiator`): each entry holds a
  reference.
- The collector, through an edge (the reference or closure above, when it is stored in the heap), or through the
  recursive set the variable was a member of (`qore_dgc_node_dereferenced()` takes a temporary reference to a set
  member it finds through a watch, and a dereference with a set can rescan it).

Everything but the frame and the collector holds a reference of its own. New references are made only by the frame's
own thread: `\var` and closure capture are evaluated in the frame, and every other holder copies one it already has.

## A variable that only its frame can use takes no lock and makes no scan

`ClosureVarValue::frameExclusive()` is true when:

- the reference count is one,
- that reference is the frame's (`frame_owned`, set when the entry that created the variable is pushed and cleared
  before that entry releases its reference - `ThreadClosureVariableStack::releaseEntry()`), and
- the variable is in no recursive set and has no deferred scan.

Then no other thread can reach the variable and nothing on the heap refers to it. It is read and written like a
plain local variable:

- `eval()`, `getLValue()`, `remove()`, `getReference()`, `getLValueId()` and `clearValue()` take `rml` only when
  the variable is not frame-exclusive (`QoreSafeVarRWReadLocker` and `QoreSafeVarRWWriteLocker` take an
  optional-lock argument for this).
- `getLValue()` does not make the variable the scan root (`LValueHelper::setClosure()`). A cycle through the
  variable needs an edge into it, and every such edge holds a reference, so while the count is one no change to the
  value can close one. The write that later creates the edge - storing `\var` or a closure in an object or container -
  scans from the object or container it writes, and that scan finds the cycle; `ClosureBoundLocalChecks` checks this
  for each way of making the edge.

Why the frame flag is needed: after the frame has returned, a closure or a stored reference can hold the variable's
only reference while several threads use it through that closure or reference. The count alone would call that
exclusive.

Why the check cannot go stale while the frame uses the variable:

- The count cannot rise from one while the frame is in the middle of an operation on the variable: only the frame's
  thread makes new references, and an lvalue operation evaluates no code of the frame while it holds the variable.
- The count is read with acquire ordering, and every reference is released with release ordering (the lock-free
  release in `RObject::tryFastDeref()` and the locked one in `RObject::deref()` alike). When the frame reads one, every
  access that the holders of the released references made - under the lock - happened before, and so did the frame's
  own flag being cleared, which is stored before its release.
- The recursive-set exclusion covers the collector: with no set and no deferred scan, a temporary reference taken
  through a watch is released by the lock-free path, which does not read the value.

Accesses that other code makes without the variable's methods still lock (`finalize()`, which program teardown runs
after all threads have left, and the interpreter's closure-environment fast paths in `lib/QoreIRInterpreter.cpp`).
Taking the lock is always correct; only skipping it needs the proof above.

## What is not done: avoiding the heap variable

A referenced local could be kept in the frame when the parser can prove that no reference to it ever leaves the
thread or outlives the frame. That proof is not available where it would matter: a reference passed as an argument
goes to a callee chosen at run time (virtual methods, closures, call references), and a builtin that takes `auto` can
store it (`Queue::push(\var)`), so the reference can reach another thread or the heap in ways the parser cannot see.
The variable therefore stays closure-bound, and the check above makes it cost what a plain local costs while nothing
else holds it.

## Regression coverage

- `examples/test/qore/vars/closure-bound-locals/closure-bound-locals.qtest`: scan counts (debug builds) for a
  variable used only by its frame, written through a reference, captured by a closure and released again; cycles
  made through such a variable after skipped scans are collected; closures whose frame has returned called by
  several threads; in this program, each execution mode and a compiled module.
- ThreadSanitizer, with a module-free driver doing the same across threads, reports no race.
