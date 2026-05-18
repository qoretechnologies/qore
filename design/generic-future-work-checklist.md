# Generic Class Future Work Checklist

This checklist covers generic class type work that remains after the core
source/builtin generic class, generic hashdecl, static generic method,
method-level generic, default type argument, bounded type argument, and explicit
generic call type argument implementation.

The work is intentionally split into semantic phases. Variance changes type
compatibility, expression-site inference depends on those compatibility rules,
and specialized code generation should only run after the language semantics are
stable.

## Phase 1: Variance And Wildcard Type Arguments

Goal: allow APIs to express "some instantiation compatible with this bound"
without relying on raw legacy annotations.

Design checklist:

- [x] Choose final wildcard syntax for class and hashdecl type arguments.
- [x] Support unbounded wildcards.
- [x] Support covariant upper-bound wildcards.
- [x] Support contravariant lower-bound wildcards if there is a clear,
  user-facing assignment/call use case.
- [x] Define how wildcards interact with `*T` optional types.
- [x] Define how wildcards interact with `auto`, which remains a concrete type
  argument.
- [x] Define compatibility between wildcard arguments and raw generic
  compatibility attributes.
- [x] Define diagnostic wording for rejected wildcard construction attempts.

Implementation checklist:

- [x] Extend parser and scanner type grammar.
- [x] Mirror parser/scanner changes in `modules/astparser/src/`.
- [x] Extend `QoreTypeInfo` representation for wildcard generic arguments.
- [x] Update class type compatibility checks.
- [x] Update hashdecl type compatibility checks.
- [x] Update `instanceof`, assignment, argument filtering, return checking,
  casts, and overload selection.
- [x] Preserve wildcard type paths through AOT type metadata.
- [ ] Expose structured wildcard argument metadata for API metadata consumers.
- [x] Add positive and negative tests for source classes, builtin classes,
  source hashdecls, inherited hashdecls, raw compatibility, AOT, AST mode, and
  error messages.

## Phase 2: Expression-Site Class Type Argument Inference

Goal: infer class type arguments where the result is unique and explain
ambiguity when it is not.

Design checklist:

- [ ] Infer constructor type arguments from explicit constructor arguments.
- [ ] Infer constructor type arguments from assignment target type.
- [ ] Infer static factory receiver type arguments from assignment target type
  only when the receiver is generic and otherwise unbound.
- [ ] Infer from return context only where Qore already has a reliable expected
  type.
- [ ] Reject ambiguous inference when overloads, defaults, or unrelated
  argument positions produce competing bindings.
- [ ] Prefer explicit type arguments over inference.
- [ ] Preserve default type argument behavior when inference does not bind an
  optional parameter.
- [ ] Define diagnostics that tell users which explicit spelling to write.

Implementation checklist:

- [ ] Reuse or extend method-level type-parameter inference helpers for class
  constructors.
- [ ] Thread expected target type through declaration construction, assignment,
  and return-expression parse initialization where available.
- [ ] Add inference bindings for constructor formal types containing class type
  parameters.
- [ ] Apply bounds/default checks after inference.
- [ ] Preserve inferred object type through IR, JIT helpers, and AOT.
- [ ] Add tests for constructor inference, target-type inference, defaults,
  bounds, overload ambiguity, named arguments, AST mode, and AOT.

## Phase 3: Specialized Generic Code Generation

Goal: allow JIT/AOT to emit specialized bodies for concrete generic
instantiations without changing source-visible semantics.

Design checklist:

- [ ] Decide which execution tier owns specialization: parse IR, JIT lowering,
  AOT emission, or a staged combination.
- [ ] Define specialization cache keys for class receiver type arguments,
  method/function type arguments, static generic calls, and parameterized
  hashdecl-derived records.
- [ ] Keep the substitution-based interpreter path as the correctness baseline.
- [ ] Define invalidation and reuse rules for source-stripped AOT and module
  loading.
- [ ] Define when specialization is skipped, such as raw generic calls,
  unresolved type parameters, dynamic calls, or excessive cache growth.
- [ ] Define observability rules so reflection and stack traces still describe
  the source method/function.

Implementation checklist:

- [ ] Introduce an internal specialization descriptor.
- [ ] Intern or cache descriptors per program.
- [ ] Clone or lower IR with concrete substituted types.
- [ ] Make JIT lookup specialization-aware while keeping fallback execution.
- [ ] Preserve AOT compatibility with older binaries by feature-gating new
  metadata.
- [ ] Add correctness tests that compare specialized and non-specialized paths.
- [ ] Add stress tests for cache reuse and mixed instantiations.
- [ ] Add performance-focused smoke tests only after correctness is stable.

## Phase 4: Doxygen And User Documentation

Documentation checklist:

- [ ] Update `doxygen/lang/155_data_type_declarations.dox.tmpl`.
- [ ] Update `doxygen/lang/215_classes.dox.tmpl`.
- [ ] Update `doxygen/lang/217_hashdecl.dox.tmpl`.
- [ ] Update `doxygen/lang/220_threading.dox.tmpl` if builtin generic examples
  change.
- [ ] Update `doxygen/lang/900_release_notes.dox.tmpl`.
- [ ] Include examples for defaults, bounds, method-level generics, explicit
  generic call type arguments, generic hashdecl inheritance, variance/wildcards,
  expression-site inference, and any visible specialization behavior.
- [ ] Keep examples concrete and executable where practical.

## Phase 5: Local Verification

Run focused checks after each implementation phase, then run a complete local
verification before the final commit:

- [ ] `cmake --build build --target qore -j$(nproc)`
- [ ] `cmake --build build --target qpp astparser -j$(nproc)`
- [ ] `./run_tests.sh -d qore/misc/generic-classes.qtest`
- [ ] `build/qore --exec-mode=ast examples/test/qore/misc/generic-classes.qtest`
- [ ] `build/qcc -o build/generic-classes-qtest-aot examples/test/qore/misc/generic-classes.qtest`
- [ ] `build/generic-classes-qtest-aot`
- [ ] `build/qore modules/astparser/test/astparser.qtest`
- [ ] Relevant docs fast targets, such as `make docs-lang-final-fast`.
- [ ] Full `./run_tests.sh`.
- [ ] `git diff --check`.
- [ ] Apply `audit-changes` before each commit.
