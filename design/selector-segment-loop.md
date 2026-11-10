# Selector-segment loop of lazily iterated lists of selectors

A list of selectors with ranges (`l[a, b..c]`) that a `foreach` statement or a `map`, `select`, `foldl` or
streaming operator iterates is iterated lazily, as the AST interpreter iterates it
(`QoreFunctionalSquareBracketsComplexOperator`): each selector is evaluated when the values of the previous ones have
been used (qore#5675), and the values are those of the list of selectors as a value (qore#5688).

## IR

The lowering (`QoreIRLowering::lowerSourceIterator()`, `emitSelectorIteratorNext()`) emits:

- `IteratorCreateSelectors`: the iterator, which keeps the sequence;
- `IteratorNextSelectors`: one instruction per value, which branches to a block when the iterator has none;
- `SelectorIteratorPending`: the selector needed next, or -1 when every value has been given, and a switch to the
  block that evaluates it in a temp scope of its own;
- `SelectorIteratorFeed`: gives the selector's value to the iterator, which raises `RANGE-ERROR` for a bound of a
  range that is `NOTHING`.

The IR interpreter runs these with `QoreRtSelectorIterator` (`lib/JITRuntime.cpp`).

## Native code

When the sequence is a list whose values hold no reference (`list<int>`, `list<float>`, `list<bool>` and their
`*` forms; `QoreIRLowering::useSelectorSegmentLoop()`), the lowering sets `QoreIRListSelectorFlag::Segment` on
`IteratorCreateSelectors`, and native code (JIT and AOT, `QoreIRToLLVM`) iterates the list with the selector-segment
loop (qore#5711):

- the iterator is still created and released as before, and only keeps the sequence;
- the cursor lives in allocas of the function's frame (`SelectorSegmentState`): the next index, the number of
  indices left of the current selector, the step (1 or -1), the size and the values of the list
  (`QoreRtSelectorSegmentCursor`), and the selector needed next;
- `SelectorIteratorFeed` calls `qore_rt_selector_segment_feed()` once per selector, which computes the first index,
  the number of indices and the step as the iterator does (normalized index, effective range with negative offsets,
  `RANGE-ERROR` for a `NOTHING` bound);
- `SelectorIteratorPending` reads the selector needed next;
- `IteratorNextSelectors` (`lowerSelectorSegmentNext()`) reads each value from the list inline: the value at the
  index, or `NOTHING` outside of the list, with the output slot and release protocol of the iterator's values
  (`iterator_element_allocas`); only a node value (an int or float that the NaN-boxed representation cannot hold) is
  referenced and released.

The cursor is native state only: a loop never changes tiers while it runs (on-stack replacement requests only a
native compilation for the next call), and no layout of a runtime object is compiled into AOT code.  The pending,
feed and next steps of a loop must be lowered after its `IteratorCreateSelectors`, which registers the cursor
(`checkSelectorIteratorLowered()` fails the lowering otherwise).  A sequence that is not a list, a string or a binary
(`NOTHING` in a list-typed variable with `%loose-types`) has no iterator, which the lowering of each consumer checks
before its loop, so the cursor is never used for it.

A list of stable selectors (constants, and int locals that the loop does not write) over such a list is iterated
lazily as well, instead of building the list of its values first (`decideLazySelectorSource()`), when it has at
least as many ranges as single indices: a lazy range gives its values without a list of them, while a lazy single
index costs a step of the iteration protocol (pending, feed, next) where the list costs one element, in the IR
interpreter as in native code.  A `map`, `select` or `foldl` operator whose expressions only compute values from
numeric elements keeps the eager path too.

`QORE_DISABLE_IR_SELECTOR_SEGMENT_LOOP` disables both the flag and the lazy iteration of stable selectors;
`QORE_DISABLE_LLVM_SELECTOR_SEGMENT_LOOP` makes native code use the iterator for flagged iterators.

## Cost

The values of the iteration are typed `*T` (an index outside of the list gives `NOTHING`), so the consumer's code
costs what it costs for a plain `list<*T>`: the loop itself costs about as much as iterating a plain list of the
same type in every mode (see the benchmarks of qore#5711).  A call that takes a `T` argument from a `*T` value
resolves its variant at run time in the IR interpreter and JIT, which is the larger part of the cost compared with a
plain `list<T>`.
