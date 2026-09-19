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
- The streaming operator keywords `all`, `any`, `count`, `drop`, `first`,
  `iterate`, `take`, `takeuntil` and `takewhile` are names when the characters
  after them match `SOFT_IDENTIFIER_FOLLOW`, and operators otherwise:
  - `+` or `-` immediately after the keyword;
  - after optional whitespace, one of `= ; , ) ] } { * / % & | ^ < > ? : . [ ~`;
  - after optional whitespace, `++`, `--`, `+=`, `-=`, `!=`, or `+` or `-`
    followed by whitespace;
  - after whitespace, the complete word `in` or `instanceof`.

  So `count - 1` subtracts from a variable, while `count -$1 < 0, l` counts the
  matching elements, and `count inputs` counts the elements of `inputs`.
- After `find`, `first`, `last` or `one` followed by whitespace is the find mode;
  `find first(x) in q where (...)` calls a function named `first`.

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
- `_soft_identifier` is a streaming operator keyword used as a name, aliased to
  `identifier`. It is valid in expressions, declared names and named arguments.
  The scanner produces it when `SOFT_IDENTIFIER_FOLLOW` matches, so the
  generated lexer reads the keyword as an operator only where the runtime does.
  A member name, as in `h.count`, is not an expression start, so the keyword
  token there remains a `streaming_keyword_identifier`.
- `_find_modifier` is the find mode, aliased to `find_modifier`. It is only valid
  after `find`, where it takes precedence over the other keyword tokens, as the
  `find_state` start condition does.

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
restriction because they only match before a declaration name. For the same
reason, every rule that shares a parse state with an expression start and
accepts a plain name, such as a declared name or a named argument, also accepts
`_soft_identifier`. The runtime reads `count::x` as one scoped name, which the
grammar does not accept for these keywords.

The scanner looks at the characters after the keyword without including them in
the token. Tree-sitter records how far the scanner looked, so an incremental
parse reads the keyword again when an edit changes those characters.

These rules determine the syntax tree for valid code:

- `trim(s).size()` calls `size()` on the result of `trim(s)`; `trim (s)` remains
  the trim operator.
- `first(values)` and the other streaming keyword calls have an `identifier`
  function node; `first (values)` remains the streaming operator.
- `count - 1`, `first;` and `f(count: 1)` have `identifier` nodes for the
  keyword and no streaming operator node.
- `class` and `module` parse like any other identifier outside declarations,
  including named arguments, parameters, `::` paths and top-level statements.

## Type names

`lib/scanner.lpp` has no reserved type names: `int`, `string`, `hash`, `data`,
`timeout` and the other built-in type names are identifiers that the parser
resolves as types. The grammar has keyword tokens for them, listed in
`TYPE_NAMES` in `grammar.js`, so that it can parse types. They are also valid in
two other places:

- `_type_keyword`, an expression such as the call `int("1")` or the variable
  `data`
- `_declared_name`, the name of a declaration, aliased to `identifier`, as in
  `our int;`, `const hash = {};`, `sub f(data) {}`, `class C { data() {} }`,
  `foreach my int in (l)`, `catch (int)` and `my (int, data) = l;`

After a type, a type name can only be the declared name. Without a type, the
token after the name decides: `our int;` declares `int`, and `our int x;`
declares `x`. The GLR conflicts for this are listed together in `grammar.js`.
Without a type or a `my`, `our` or `thread_local` keyword, a name is an
expression, as in the runtime: `data += 1;` and `x = 1;` assign, and
`code(a, b);` calls. A local declaration without `my` requires a type. A type before `sub`
makes a closure with a return type, as in `hash<auto> sub () {}`, rather than
comparisons, and `instanceof` is always followed by a type.

`examples/test/modules/astparser/declarations.qtest` checks the declarations
that the runtime accepts with type names and some that it rejects.

## Changing and testing the keyword rules

When `lib/scanner.lpp` changes these rules, update the table in `src/scanner.c`,
keeping it sorted, and the keyword lists in both tests. Then regenerate
`parser.c`, `grammar.json` and `node-types.json` with Node 24 and
`npx tree-sitter-cli@0.26.13 generate`.

`examples/test/modules/astparser/keyword-call.qtest` compares the runtime and
AstParser for each keyword in each call context: statements, expressions,
`foreach` iterables, `switch` bodies, top-level code, member, scoped and
named-argument calls, call references and chained calls. It also checks spaced
operators, `class` and `module` as names and declarations, streaming keywords as
names and operators, the find mode, word boundaries and malformed input. The standalone `astparser-keyword-identifier-test` target checks
token boundaries, extras, end of input, error recovery and incremental reparsing
without the Qore runtime:

```sh
cmake --build build-debug --target astparser-keyword-identifier-test
valgrind --leak-check=full --error-exitcode=99 build-debug/modules/astparser/astparser-keyword-identifier-test
```
