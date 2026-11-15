# Locked Parse Options

## Invariant

A Program whose parse options are locked keeps the restrictions that were locked in:

- a child Program created by a Program without `PO_NO_CHILD_PO_RESTRICTIONS` keeps the restrictions of its parent
  (inherited when it is created) and those it was created with;
- a child that is locked without inheriting (created with explicit options that lack `PO_NO_CHILD_PO_RESTRICTIONS`)
  keeps the restrictions it was created with;
- a Program locked with `Program::lockOptions()` keeps the restrictions it has when it is locked;
- a Program that restricted code creates without becoming its parent keeps the restrictions of that code: an
  independent Program (`Program::constructor()` with `independent` set) and an evaluation Program
  (`ProgramControl::createEvalProgram()`, a child of the Program evaluated).  It takes them as a child would
  (`qore_program_private_base::restrictForCreator()`: the restrictions are added, and the positive options that the
  creating code lacks are removed, except `PO_ALLOW_REPARSE`, which only allows code to be parsed more than once and
  which `QoreRepl` needs) and locks them (`lockCreatorRestrictions()`).

Creating code restricts such a Program (`restrictForCreator()`) when it does not have `PO_NO_CHILD_PO_RESTRICTIONS`
and has restrictions or is locked itself (a locked Program without restrictions still lacks positive options, which
the new Program must not get), and whenever it is sandboxed (`qore_caller_is_sandboxed()`): an independent or
evaluation Program of sandboxed code takes its restrictions also when it has `PO_NO_CHILD_PO_RESTRICTIONS`, and does
not get `PO_NO_CHILD_PO_RESTRICTIONS` itself, so that its own children inherit them.  The ordinary children of a
sandboxed Program with `PO_NO_CHILD_PO_RESTRICTIONS` get the options they are created with, as with any parent that has
it; the sandbox's runtime policy governs their code.  Code without restrictions that is not locked creates an
independent Program with the options it gives (`QoreRepl` creates one with `PO_ALLOW_REPARSE`).

The restrictions taken (`getCreatorRestrictions()`) are those that limit what code can do, including the options that
keep system code from a Program (`PO_NO_INHERIT_SYSTEM_*` but `PO_NO_INHERIT_PROGRAM_DATA`): an independent Program
copies its namespace from the system namespace, not from its creator's, and would otherwise get the system API that a
creator without it (`PO_NO_API`) lacks.  An independent Program keeps its own style and compatibility options
(`%modern` with its implied options included), and the options that decide what user code a child inherits from its
parent are not taken.  The creator's options are read once, under its `po_mutex`, and the restrictions locked in are
the ones that were added (`lockCreatorRestrictions()`).

These restrictions (`qore_program_private_base::locked_restrictions`) cannot be removed by
`Program::disableParseOptions()`, by a parse directive that disables options, or by `Program::replaceParseOptions()`
(which checks the options its replacement would remove and add, as the two others do).  Positive options (those that
give more freedom) cannot be added to a locked Program either (`checkSetParseOptions()`).

What may still be disabled on a locked Program:

- free options (`QoreParseOptions::FREE_OPTIONS`), which have no effect on code access or safety;
- options of style and backward compatibility: `PO_STYLE_OPTIONS`, the other options of `PO_MODERN`,
  `PO_REQUIRE_OUR`, and the keyword switches `PO_NO_TYPEDEF`, `PO_NO_ENUM` and `PO_NO_TRANSIENT`, which restrict how
  code is written, not what it can do, so that a locked child can parse code in another style;
- positive options, which make the Program more restrictive;
- restrictions that the Program added itself after it was locked in, which its creator was free not to set.

## Thread safety

The options can be changed and locked from any thread.  `po_mutex` (a leaf lock) makes every check against the lock
and the change it allows one step with `QoreProgram::lockOptions()` (`changeIfNotLocked()`); exceptions and parse
errors are raised after it is released, as raising one takes the locks of the Programs on the call stack.
`restoreParseOptions()` (`%push-parse-options`) restores the snapshot of the file with the restrictions locked in
since it was taken, and without the positive options that a Program locked since then no longer has.

## Implementation

`qore_program_private::checkDisableParseOptions()` (`include/qore/intern/qore_program_private.h`) is the one check;
`locked_restrictions` is set in `qore_program_private_base::setParent()` (`lib/QoreProgram.cpp`), in
`QoreProgram::lockOptions()` and in `lockCreatorRestrictions()`, which `Program::constructor()`
(`lib/QC_Program.qpp`) and `ProgramControl::createEvalProgram()` (`lib/QC_ProgramControl.qpp`) call.

The sandbox's runtime policy is a separate layer: the backquote operator and `backquote()` (`backquoteEval()` in
`lib/BackquoteNode.cpp`, which the IR and JIT paths call too) check that the policy allows `/bin/sh` to be executed,
as `system()` does.

## Tests

- `examples/test/qore/classes/Program/positive-options.qtest`: a locked child is refused positive options;
- `examples/test/qore/classes/Program/parse-options.qtest`: a child may disable a restriction it set itself;
- `examples/test/qore/classes/Program/locked-options-allowed.qtest`: what a locked child may still disable (style,
  free and self-added options, and the directives of another style);
- `examples/test/qore/classes/Program/inherited-restrictions.qtest`: a child cannot disable a restriction it
  inherited (with an int mask or a `ParseOptions` object, also when its parent is sandboxed), code parsed into it
  afterwards still cannot use what it restricts, and a Program locked with `Program::lockOptions()` keeps the
  restrictions it had then but may disable those added after the lock;
- `examples/test/qore/classes/Program/independent-restrictions.qtest`: an independent Program of restricted code (also
  sandboxed) takes and keeps each restriction, keeps its own style and namespaces and does not get positive options
  that its creator lacks; code without restrictions gives an independent Program the options it wants; an evaluation
  Program takes the restrictions of the code that creates it; a child keeps the restrictions it was created with;
- `examples/test/qore/classes/Program/independent-restrictions-sandboxed.qtest`: a creator with
  `PO_NO_CHILD_PO_RESTRICTIONS` gives an independent Program the options it wants unless it is sandboxed;
- `examples/test/qore/security/backquote-sandbox.qtest`: a sandbox that does not allow the shell denies the backquote
  operator and `backquote()` in every execution mode;
- `examples/test/qore/classes/Program/locked-options-replace.qtest`: `Program::replaceParseOptions()` cannot drop a
  locked restriction (one created with, inherited, locked with `lockOptions()`, or an extended one that an int
  replacement cannot hold), while a replacement that only adds restrictions is made.
