# Final Methods Across the Class Hierarchy

Issue #5591. A `final` method is a guarantee that trusted frameworks rely on: a subclass, including one supplied by
sandboxed code, cannot replace a method that the framework calls. Before Qore 3.0 only a direct override with an
identical signature was refused (`MethodFunctionBase::checkFinal()`), and the guarantee did not hold in two ways.

## How a call selects a variant

- **Lookup.** A call of a method's name on an object resolves the class's own method of that name if it has one,
  otherwise the first one found depth-first through its parent classes, in the order they are listed
  (`qore_class_private::runtimeFindCommittedMethodIntern()`).
- **Selection.** The variants considered are those of the resolved method and of the methods of the same name in its
  inheritance list (`QoreFunction::ilist`, built depth-first by `addAncestors()`), in that order
  (`QoreFunction::runtimeFindVariant()`). The highest score wins, and **an earlier variant wins a tie**.

So with `class T1 inherits Other, Base {}` the call resolves to `Other::m()`, whose inheritance list does not include
`Base::m()`; with `class T2 inherits Plain, Base {}` it resolves to `Root::m()` through `Plain`. And a subclass
variant with another signature comes before the final variant in the list: `h(object, *binary)` is selected over
`final h(object, *data)` for a binary argument. A *broader* type wins as well when it scores as well as the final
variant's - `data`, `softint`, `auto` and `*string` all took calls from a `final h(string)` in a check of the runtime -
so narrowing is not the criterion.

## The rule

For each method of a parent class that has `final` variants (`MethodFunctionBase::hasFinal()`), and each final
variant `F` that is not `private:internal` (such variants are not inherited and cannot be overridden):

1. The class's lookup of the name must reach `F`'s method: it must be in the inheritance list of the method that
   the lookup starts with (the class's own method, or the first found depth-first in its parent classes).
2. No variant of a method before `F`'s method in that list may accept arguments that `F` accepts.

`qore_class_private::parseCheckFinalHierarchy()` applies both, for normal and static methods, at
`qore_class_private::parseInit()` for every user class with parent classes, also one without methods of its own
(`collect_final_methods()`, `find_first_parent_method()`, `MethodFunctionBase::checkFinalResolution()`). The class's
own variants are checked by `checkFinalVariant()`, which refuses an identical signature as before and now also an
overlapping one, so that the error names the variant's location.

"Accepts arguments that `F` accepts" (`final_variant_may_be_bypassed()`) is decided statically and conservatively:
the two signatures accept the same number of arguments for some call (a parameter that has a default value, is
untyped or accepts `NOTHING` can be omitted; varargs accept any number), and at each position before that number the
types intersect - one accepts a value of the other's type (`QoreTypeInfo::parseAccepts()` either way), or either is
untyped or contains a type parameter. Whether overload resolution would actually prefer `F` for every common
argument is not decided: scores depend on the values, and a tie goes to the other variant. A variant that accepts
none of `F`'s arguments is allowed.

## Consequences

- A subclass of a class with `final string log(string)` can still add `log(int)` or `log(string, string)`.
- A class with multiple parent classes lists the one with the final method first, or does not inherit a method of
  that name from the others.
- A class that was accepted because it overrides nothing (`class Derived inherits Base {}`) passes the problem to
  its own subclasses, which are checked in turn.

Tests: `examples/test/qore/parser/final-methods.qtest`. Documentation: `final_methods` in
`doxygen/lang/215_classes.dox.tmpl`.
