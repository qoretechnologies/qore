# Scoped names in astparser

Copyright (C) 2026 Qore Technologies, s.r.o.

`lib/scanner.lpp` reads a scoped name as one `SCOPED_REF` token, so its parts
cannot be separated by whitespace:

- `({WORD}::)+{WORD}` is a namespace or class path such as `Qore::True`.
- `(::{WORD})+` is a path from the root namespace, including a single name such
  as `::foo`.
- Generic paths such as `Buffer<int>::sized` and `Chooser<int>::pick<int>` are
  matched with their type arguments, as the longest possible token.

The tree-sitter grammar mirrors these rules in `scoped_identifier` and
`generic_scoped_identifier` in
`modules/astparser/grammars/tree-sitter-qore/grammar.js`:

- A `::` separator after a name is `token.immediate`, while a leading `::` may
  follow whitespace. `A::b` is one name, and `A ::b` is the name `A` followed by
  the root namespace name `::b`, as in the typed global declaration
  `our Type ::name`. `A :: b` is a syntax error, as in the runtime.
- Each generic component of a `generic_scoped_identifier` has dynamic
  precedence 1. Without the longest-token rule, `Chooser<int>::pick<int>(1)` could
  also be read as the comparison chain `(Chooser<int>::pick < int) > (1)`, and
  `f(Buffer<int>::sized(n))` as `(Buffer < int) > ::sized(n)`. A comparison chain
  that ends with a root namespace name requires whitespace before `::`, so it
  still parses as a comparison: `a < b > ::c(n)`.

`our` and `thread_local` declarations accept scoped variable names, as the
runtime's `gvardecl` rule does: `our int N::x = 1;` and `our int ::y;`. The
grammar represents such a name as a `variable_declarator` whose `name` field is
a `scoped_identifier`.

`examples/test/modules/astparser/scoped-names.qtest` compares the runtime and
AstParser for root namespace calls, types, constants, class declarations and
base classes, scoped global declarations, generic calls and comparisons, and
separated scope operators.
