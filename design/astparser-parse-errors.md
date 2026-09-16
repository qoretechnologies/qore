# Parse errors in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

`AstParser::collectErrors()` in `modules/astparser/src/AstParser.cpp` walks the
tree-sitter tree and reports one error for each of the following:

- an `ERROR` node: `Syntax error near: <text>`, or the named-argument message
  for a positional argument that follows a named argument
- a visible missing node, which error recovery inserts for an expected token:
  `Missing <token>`, located right after the previous token
- a missing hidden token: `Missing token in <node>`, located at its nearest
  visible ancestor

The tree-sitter node API does not return hidden nodes, such as the name token of
a `variable_name` or the empty `_same_line` token that requires a directive
argument on the directive's line. A missing hidden token is therefore only
visible as `ts_node_has_error()` on a node where none of the visible children
has an error. The walk descends only into nodes with errors.

In `modules/astparser/grammars/tree-sitter-qore/grammar.js`, `identifier` is a
rule that wraps the hidden `_identifier_token` rather than a token. During
error recovery, tree-sitter inserts a missing token only if the next token then
reduces a rule. The wrapper lets recovery insert a missing name before a token
such as `)` or `;` and keep the rest of the tree. With a plain token, recovery
wraps the surrounding source in `ERROR` nodes instead, and a single error
cascades into many.

`lib/scanner.lpp` reads a variable reference such as `$name` as one token, so
the name in `variable_name` is an immediate token: `$ name` is an error.

`examples/test/modules/astparser/parse-errors.qtest` checks the message and line
of each kind of error against sources that the runtime rejects.
