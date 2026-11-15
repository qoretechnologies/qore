# Locked Parse Options

## Invariant

A Program whose parse options are locked keeps the restrictions that were locked in:

- a child Program created by a Program without `PO_NO_CHILD_PO_RESTRICTIONS` keeps the restrictions of its parent
  (inherited when it is created);
- a child that is locked without inheriting (created with explicit options that lack `PO_NO_CHILD_PO_RESTRICTIONS`)
  keeps the restrictions it was created with;
- a Program locked with `Program::lockOptions()` keeps the restrictions it has when it is locked.

These restrictions (`qore_program_private_base::locked_restrictions`) cannot be removed by
`Program::disableParseOptions()`, by a parse directive that disables options, or by `Program::replaceParseOptions()`
(which checks the options its replacement would remove and add, as the two others do).  Positive options (those that
give more freedom) cannot be added to a locked Program either (`checkSetParseOptions()`).

What may still be disabled on a locked Program:

- free options (`QoreParseOptions::FREE_OPTIONS`), which have no effect on code access or safety;
- options of style and backward compatibility: `PO_STYLE_OPTIONS`, the other options of `PO_MODERN`,
  `PO_REQUIRE_OUR`, and the keyword switches `PO_NO_TYPEDEF`, `PO_NO_ENUM` and `PO_NO_TRANSIENT`, which restrict how
  code is written, not what it can do, so that a locked child can parse code in another style;
- positive options, which makes the Program more restrictive;
- restrictions that the Program added itself after it was locked in, which its creator was free not to set.

## Implementation

`qore_program_private::checkDisableParseOptions()` (`include/qore/intern/qore_program_private.h`) is the one check;
`locked_restrictions` is set in `qore_program_private_base::setParent()` (`lib/QoreProgram.cpp`) and in
`QoreProgram::lockOptions()`.

## Tests

- `examples/test/qore/classes/Program/positive-options.qtest`: a locked child is refused positive options;
- `examples/test/qore/classes/Program/parse-options.qtest`: a child may disable a restriction it set itself;
- `examples/test/qore/classes/Program/locked-options-allowed.qtest`: what a locked child may still disable (style,
  free and self-added options, and the directives of another style);
- the regression test of the restrictions that cannot be removed is pending (see the branch's notes).
