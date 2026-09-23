# Brace-delimited regular expressions in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

The tree-sitter Qore grammar accepts the runtime scanner's brace forms:
`m{pattern}`, `s{pattern}{replacement}`, `tr{source}{target}`, and
`x{pattern}`. Existing slash forms use their original grammar rules.
The syntax tree retains the existing operation node kinds used by AST consumers.

The grammar reserves each opening prefix only after `=~`, `!~`, or `case`, and delegates the remaining token to
`modules/astparser/grammars/tree-sitter-qore/src/scanner.c`. The scanner counts
nested braces, consumes escapes, and retains newlines and comment-like text
inside a body. Only whitespace can separate the two bodies of substitution and
transliteration. Match, substitution and extraction accept the same modifiers
as `lib/scanner.lpp`; global matching applies to substitution and extraction.
Transliteration has no modifiers.
Negative matching and switch cases accept only match expressions. Outside regex
contexts, `m{key}`, `s{key}`, `tr{key}`, and `x{key}` remain hash access expressions.

The external scanner is stateless, allocation-free and iterative. Its depth
counter is bounded by the source length. It consumes input on every successful
loop iteration and refuses all-token error-recovery states. Missing closing
braces or second bodies produce parse errors, and a failed parse cannot leave
scanner state behind for another tree.

CMake compiles and installs both the generated parser and its external scanner;
other consumers of the installed grammar must compile both sources. Regenerate
`parser.c`, `grammar.json` and `node-types.json` with Node 24 and the pinned
`npx tree-sitter-cli@0.26.13 generate` after grammar changes. Normal builds use the
committed generated sources.

`modules/astparser/test/brace-regex.qtest` compares valid expressions with actual
Qore runtime evaluation. It covers each operation, flags, escaped/nested braces,
multiline content, long and deeply nested replacement bodies, invalid input,
parser reuse and declarations following a regex. The existing astparser suite
checks the unchanged declaration and source-token metadata consumed by Qdx.
The regression suite also checks operation node types and complete token text.

The standalone `astparser-brace-regex-test` CMake target checks the actual
tree-sitter parser and scanner, including hash-access compatibility, recovery,
and large nested bodies. It can be memory-checked without loading the runtime
or PCRE2:

```sh
cmake --build build-debug --target astparser-brace-regex-test
valgrind --leak-check=full --error-exitcode=99 build-debug/modules/astparser/astparser-brace-regex-test
```
