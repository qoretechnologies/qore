# Context-dependent keywords in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

`lib/scanner.lpp` reads some keywords differently depending on the characters
that follow them:

- A keyword immediately followed by `(` is `KW_IDENTIFIER_OPENPAREN`, a function,
  method or variable name. `select(x)` calls a function or method named `select`,
  while `select (x), y` is the select operator. The rule covers `all`, `any`,
  `background`, `case`, `chomp`, `count`, `delete`, `drop`, `exists`, `final`,
  `find`, `first`, `foldl`, `foldr`, `inherits`, `iterate`, `map`, `new`, `pop`,
  `private`, `push`, `select`, `shift`, `splice`, `take`, `takeuntil`,
  `takewhile`, `trim` and `unshift`.
- `default`, `deprecated`, `public`, `returns` and `static` are also names when
  `{WS}*` (spaces, tabs or carriage returns) separates them from `(`.
- `class` and `module` are not reserved. They are declaration keywords only when
  `{WS}+` and a name follow; a class name may also be a `::` path. Otherwise they
  are identifiers: `class = 1;` assigns a variable, `module\n(1)` is a call and
  `class\nT {}` is a syntax error.

The tree-sitter grammar mirrors these rules with external tokens in
`modules/astparser/grammars/tree-sitter-qore/src/scanner.c`:

- `_keyword_identifier` is a call target in `call_expression`, aliased to
  `identifier`. The scanner produces it for the reserved keywords above only when
  the parenthesis follows, so the parenthesis still starts a normal argument list.
- `_class_keyword` and `_module_keyword` start class and module declarations,
  aliased to the anonymous `class` and `module` nodes. The scanner produces them
  only when a declaration name follows. `class` and `module` are not keyword
  tokens anywhere else, so the generated lexer reads them as identifiers, as
  `lib/scanner.lpp` does.

The scanner skips the whitespace that the grammar's extras skip and then reads a
complete word from its sorted keyword table. It ends the token after the word and
only looks at the characters that follow. A comment before the word makes the
scanner decline; the generated lexer then consumes the comment and the scanner
runs again for the word. `newline`, which may end a parse directive, is also
listed as an external token but is matched by the generated lexer: when it is
valid, the scanner declines at a line break instead of skipping it, so that a
declaration after a directive does not absorb the directive's `newline` node.
Tree-sitter enables every external token during error recovery; the scanner then
matches only the declaration keywords, so that recovery can resume at a class or
module declaration.

The call target token is valid only where a call can start. Using it in other
rules would make it valid in more parse states, where it would take precedence
over an identifier that another rule in the same state requires, such as the
name in `int select(int a) {`. The declaration keyword tokens need no such
restriction because they only match before a declaration name.

These rules determine the syntax tree for valid code:

- `trim(s).size()` calls `size()` on the result of `trim(s)`; `trim (s)` remains
  the trim operator.
- `first(values)` and the other streaming keyword calls have an `identifier`
  function node; `first (values)` remains the streaming operator.
- `class` and `module` parse like any other identifier outside declarations,
  including named arguments, parameters, `::` paths and top-level statements.

When `lib/scanner.lpp` changes these rules, update the table in `src/scanner.c`,
keeping it sorted, and the keyword lists in both tests. Then regenerate
`parser.c`, `grammar.json` and `node-types.json` with Node 24 and
`npx tree-sitter-cli@0.26.8 generate`.

`examples/test/modules/astparser/keyword-call.qtest` compares the runtime and
AstParser for each keyword in each call context: statements, expressions,
`foreach` iterables, `switch` bodies, top-level code, member, scoped and
named-argument calls, call references and chained calls. It also checks spaced
operators, `class` and `module` as names and declarations, word boundaries and
malformed input. The standalone `astparser-keyword-identifier-test` target checks
token boundaries, extras, end of input, error recovery and incremental reparsing
without the Qore runtime:

```sh
cmake --build build-debug --target astparser-keyword-identifier-test
valgrind --leak-check=full --error-exitcode=99 build-debug/modules/astparser/astparser-keyword-identifier-test
```
