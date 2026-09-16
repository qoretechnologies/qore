# Parse directives in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

`lib/scanner.lpp` recognizes a parse directive at the start of a line and reads
its argument, if any, as the rest of that line. The tree-sitter grammar in
`modules/astparser/grammars/tree-sitter-qore/grammar.js` represents each
directive as a `parse_directive` node, except for the conditional, warning,
parse option stack, `%module-cmd` and `%exec-class` directives, which are extras
(`conditional_directive`) so that they may appear anywhere.

Every directive name that `lib/scanner.lpp` accepts is part of the grammar. The
parse options without arguments are plain tokens; when the runtime scanner
gains a directive, add it to the `parse_directive` choice as well.

Arguments follow the runtime rules:

- `%requires`, `%try-module`, `%try-reexport-module` and `%try-child-module`
  take a `module_spec`: a module name, a scoped name or a `module_path`, and an
  optional version constraint. A `module_path` contains a `.` or `/`, such as
  `../../qlib/QUnit.qm`, so an ordinary module name remains an `identifier`.
  `%try-module (ex) module` and `%try-reexport-module (ex) module` accept the
  variable that receives the load exception.
- `%include`, `%append-include-path` and `%append-module-path` take a string or,
  as written without quotes, a `directive_argument`; `%prepend-module-path`
  takes a string, and `%set-time-zone` a `directive_argument`.
- `%define NAME` takes an optional value, which is also a `directive_argument`
  and may be quoted.

The external scanner in `src/scanner.c` produces the rest-of-line tokens. It
skips spaces and tabs, declines at a line break, a comment or, for a directive
argument, a quote, and ends the token after the last character that is not a
space or tab. The empty `_same_line` token after each directive name that takes
an argument succeeds only if the argument starts on the same line, so an
argument on the following line is an error, as in the runtime. `newline`, which
may end a directive, is listed as an external token so that the scanner leaves
it to the generated lexer.

`AstParser` evaluates `%define`, `%ifdef`, `%ifndef`, `%if`, `%elif`, `%else` and
`%endif` lines before parsing and replaces them with spaces, so only other
tree-sitter consumers see `%define` nodes. As in the runtime, the text of a
`%define` ends before a carriage return, surrounding spaces, tabs and vertical
tabs are ignored, and the name ends at the first space, so `%define NAME value`
defines `NAME`. `examples/test/modules/astparser/conditional-directives.qtest`
checks that the runtime and AstParser select the same conditional branches.

`examples/test/modules/astparser/parse-directives.qtest` compares the runtime
and AstParser for the parse options, module paths, include files, directive
arguments and missing arguments. The standalone
`astparser-parse-directives-test` target checks the directive argument tokens,
including `%define` values, without the Qore runtime:

```sh
cmake --build build-debug --target astparser-parse-directives-test
valgrind --leak-check=full --error-exitcode=99 build-debug/modules/astparser/astparser-parse-directives-test
```
