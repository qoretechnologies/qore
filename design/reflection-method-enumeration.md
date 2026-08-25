# Reflection Method Enumeration

This document describes how `Qore::Reflection::Class` enumerates methods, and in particular the contract
of the `getAll*Methods()` family, which returns inherited methods and therefore has to answer the question
*"which methods can actually be called on this class?"*

## Two families of enumerator

| Family | Methods | Scope |
|---|---|---|
| declared-only | `AbstractClass::getMethods()`, `Class::getNormalMethods()`, `Class::getStaticMethods()` | only methods declared in the class itself |
| accessible | `Class::getAllMethods()`, `Class::getAllNormalMethods()`, `Class::getAllStaticMethods()` | the class's own methods plus every inherited method accessible in the class |

Every `get*()` enumerator on `AbstractClass`/`Class` other than the `getAll*()` family is **declared-only**.
Inherited members are absent; `Class::getClassHierarchy()` is the intended way to reach them. This is easy
to miss, because `AbstractClass::getMethods()` returns normal *and* static methods mixed together, so the
declared-only restriction only bites code that walks a hierarchy.

## Why `getAll*Methods()` belongs in the runtime

A caller can approximate the accessible set with `getClassHierarchy()` + `getMethods()`, but that walk is a
pure structural traversal with no notion of access, so it reports members the caller cannot use:

- a parent's `private:internal` methods are **not** accessible in a subclass, but a structural walk returns
  them; the runtime already excludes them — `Class::findMethod()` reports `UNKNOWN-METHOD`
- a parent inherited with `private:internal` access contributes nothing outside the class itself, which a
  structural walk also cannot see
- access is a property of a method **variant**, not of the method, so filtering by hand means reaching
  through to each variant's `getAccessModifierString()` and then deciding what a multi-variant method with
  mixed access means

Re-deriving those rules at every call site produces call sites that disagree with each other and with the
runtime. `getAll*Methods()` delegates the decision to the runtime instead.

## Contract

- **Accessibility.** Methods declared in the class itself are always returned, including its own
  `private:internal` methods. Inherited methods are resolved with `QoreClass::findMethod()` /
  `QoreClass::findStaticMethod()`, so the answer matches `Class::findMethod()` /
  `Class::findStaticMethod()` exactly: a parent's `private:internal` methods and any method inherited with
  `private:internal` access are not returned.

  Note the asymmetry: `findMethod()` cannot see the class's *own* `private:internal` methods either,
  because reflection calls carry no class context, so `runtime_get_class()` is never the class being
  reflected. Those methods are nevertheless part of the class, so they are returned from the local
  declaration directly rather than through `findMethod()`.

- **Shadowing.** Each method name is returned exactly once, with the method that would actually be called.
  With multiple inheritance this is the first matching branch in the `inherits` list, because that is what
  `findMethod()` returns.

- **Special methods.** `constructor`, `destructor`, and `copy` are never returned, consistent with
  `getNormalMethods()` and unlike `AbstractClass::getMethods()`.

- **Ordering.** Most-derived first. `getAllMethods()` is exactly
  `getAllNormalMethods() + getAllStaticMethods()`.

## Implementation notes

`append_all_methods()` in `modules/reflection/src/QC_Class.qpp` walks the hierarchy with
`QoreClassDestructorHierarchyIterator`, which yields classes in destructor execution order — the
most-derived class first. That is the opposite of `Class::getClassHierarchy()` and of
`QoreClassHierarchyIterator`, both of which are in constructor execution order (base first) and therefore
give a hand-rolled dedupe the wrong precedence.

For each class in the walk, the local methods of the requested type are collected with
`QoreMethodIterator` (normal and special methods, map `hm`) or `QoreStaticMethodIterator` (static methods,
map `shm`) and filtered by method type. A name already emitted is skipped, so the most-derived declaration
wins; a name seen for the first time on a parent is resolved from the *starting* class with `findMethod()`
/ `findStaticMethod()`, and dropped when the runtime says it is not reachable.

Marking a name as seen even when it is dropped is deliberate: it stops a `private:internal` method in one
parent from being retried against every remaining parent.

`QoreMethodIterator` iterates only `hm` and `QoreStaticMethodIterator` only `shm`; neither sees the other
map. Code that filters `QoreMethodIterator` results for `MT_Static` therefore always produces an empty
result — this was the cause of `Class::getStaticVariants()` returning an empty list and of
`AbstractClass::getVariants()` omitting static method variants.
