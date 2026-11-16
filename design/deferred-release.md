# Deferred release of nested code

Releasing the last reference to a node releases what the node holds, and releasing that releases what it holds in
turn. Freeing a structure nested *n* levels deep therefore recurses *n* times, and a thread whose stack is smaller than
the nesting overflows it. The parser accepts up to 1000 nested syntax elements whatever the thread's stack size, and
runtime data can nest without limit, so every kind of nested structure needs a bound on the recursion that frees it.

## Who bounds what

| Structure | Bounded by | Order of destruction beyond the bound |
|---|---|---|
| Lists and hashes | `qore_container_free_helper` (`include/qore/intern/qore_container_free.h`), depth 16 | Depth-first, as with recursion: the explicit stack replays it |
| Objects and closure-bound variables | `RObject::deleteOrDefer()`, combined depth 16 (see `design/dgc.md`, "Deleting long chains") | Depth-first, as with recursion |
| Code: parse and expression nodes, statements | `qore_release_node()`, `qore_release_simple_node()` and `qore_delete_statement()` (`include/qore/intern/QoreDeferredRelease.h`), depth 16 | Shallower before deeper: see below |
| Other runtime values: references, call references, closures, weak references, numbers, enums, buffers, plugin values | Not bounded themselves: a chain of closures is bounded by its closure-bound variables | Unchanged |

`qore_release_node()` defers only code. `qore_is_code_node_type()` decides by node type, with no virtual call or cast
on the release path: the parse and expression node types are code, and every runtime value type keeps the release path
it always had. Runtime values are bounded by the containers, objects and closure-bound variables that hold them, which
replay the depth-first order of the recursion, so the objects that runtime values hold are destroyed in exactly the
same order and at the same point as before.

## The mechanism

`AbstractQoreNode::deref(ExceptionSink*)` and `SimpleQoreNode::deref()` release a code node through `release()` in
`lib/AbstractQoreNode.cpp` when its last reference goes. Statements are not nodes, but they delete the statements they
hold when they are deleted (`StatementBlock::del()`, `IfStatement`, `TryStatement`, the loops, `CaseNode`,
`ContextStatement`, `OnBlockExitStatement` and `DebugStatement`), so they delete them with `qore_delete_statement()`,
which goes through the same function.

`release()` counts the releases in progress on the thread's stack (`t_release.depth`). A release made while 16 are in
progress is pushed to the thread's deferred list instead of being made. When the outermost release returns to depth
zero and the list is not empty, it makes the deferred releases one by one from its own shallow frame, counting one
level, so that what each of them releases nests up to the same bound and defers the rest to the same loop. The release
path for a code node costs one thread-local read, one increment and one decrement; for any other node it costs only
the type test.

## Destruction order

- Objects held by runtime values (lists, hashes, objects' members, closures, references) are destroyed in the same
  order and at the same point relative to the surrounding code as before: the lists, hashes, objects and
  closure-bound variables that bound the recursion keep its depth-first order, and every deferred deletion is made
  before the outermost one returns.
  `examples/test/qore/misc/destructor-order.qtest` records the destructor order of objects held at every level of a 40
  level nested list, nested hash, object chain and closure chain, and of a 1000 level closure chain, and the point
  where they run between the code before and after the release, in AST and JIT execution.
- Code is freed when a Program, a function or a closure's code is freed, or when parsing fails. Everything a release
  frees is still freed before the outermost release returns. For code nested 16 levels or less, nothing changes.
  Beyond that, the deeper parts of a code tree are freed after the shallower parts of the same outermost release. Code
  does not hold objects whose destruction the language orders: the values in a parse tree are literals and the values
  of constants, which the Program's constant lists release before its code.
- A deferred release is made with the exception sink of the outermost release, or, when the outermost release has
  none (a statement's or a simple node's), with a sink of the loop that handles an exception raised there as an
  unhandled exception.

## Tests

- `examples/test/qore/misc/deep-release.qtest` parses and runs code nested close to the parser's limit (lists, calls
  and blocks) on the test's thread and frees it with its Program on a thread with a 128KB stack, which overflowed the
  stack before, and a chain of closures each capturing a variable holding the next, which overflowed the stack before,
  and checks that a list of lists, nested hashes, a chain of objects and a chain of objects holding closures that
  capture the next object, which were already bounded, are still freed there.
- `examples/test/qore/misc/destructor-order.qtest` checks the destructor order described above.
- `examples/test/qore/parser/deep-nesting.qtest` parses, lowers and runs code nested beyond small thread stacks.
