# Walking syntax trees in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

A tree-sitter syntax tree is as deep as the nesting in its source: a chain of
`20000` binary operators, parentheses, blocks or namespaces makes a tree that is
`20000` levels deep. A Qore thread has a 512KB stack by default, so the code in
`modules/astparser/src` does not use the call stack for each level of a tree.

`CSTWalk.h` provides `cst_walk()`, which visits a node and its visible
descendants in document order with a `TSTreeCursor`. Its `enter` callback
decides whether to walk a node's children, skip them or stop the walk, and its
optional `leave` callback runs after a node's children. State for each level,
such as the scope prefix of `CSTSearcher::collectSymbols()` or the parent of a
node, is kept in a vector indexed by the depth that the callbacks receive.
Searches that follow the inheritance of classes use an explicit stack of the
classes whose parent classes are still to be searched.

`ts_node_parent()` searches from the root for each call, so code that needs the
ancestors of a node builds the path from the root down with
`ts_node_child_with_descendant()`, as `CSTSearcher::findNodeAndParents()` does,
or passes the parent that a walk already knows.

`ts_node_string()` recurses for each level of a tree. `AstTreePrinter` uses it
only for subtrees up to 512 levels high, which covers real source files; for a
higher tree, it writes the same S-expression for the upper levels itself. The
node API does not return missing tokens of hidden rules, so such a token is not
written if its parent is in the upper levels of a very high tree.

`AstParser::evaluateCondition()` evaluates `%if` expressions with an explicit
stack of parenthesized expressions, as a directive line may nest any number of
parentheses and negations.

## Cancellation and linear child iteration

Every operation over a tree takes a `CSTCancelCheck`, which calls `qore_check_cancel()` every 100 nodes and ends
the operation once any exception has been raised, so every enclosing loop that uses the same check ends in turn.
`cst_walk()` and `cst_for_each_child()` make the check for each node; `AstParser`, `AstTree` and
`AstTreeSearcher` methods then raise `THREAD-CANCELLED` or `PROGRAM-INTERRUPTED`.

`ts_node_child()` and `ts_node_named_child()` walk the preceding children on each call, so loops over children
use `cst_for_each_child()` or `cst_find_named_child()`, which use a cursor. Doc comments, the run of `/**` and
`#!` comments directly before a declaration, are recorded by `CSTDocComments` while the children of a node are
visited in order, instead of walking backwards over the preceding siblings of each declaration.
`examples/test/modules/astparser/large-sources.qtest` checks that the time to search sources with many members
grows linearly.

`examples/test/modules/astparser/deep-trees.qtest` runs the parser, the error
and comment collection, the printer, the searcher and the conditional directives
on deep trees in a thread with a 512KB stack.
