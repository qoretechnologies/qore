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
- [x] Expose structured wildcard argument metadata for API metadata consumers.
- [x] Add positive and negative tests for source classes, builtin classes,
  source hashdecls, inherited hashdecls, raw compatibility, AOT, AST mode, and
  error messages.

## Phase 2: Expression-Site Class Type Argument Inference

Goal: infer class type arguments where the result is unique and explain
ambiguity when it is not.

Design checklist:

- [x] Infer constructor type arguments from explicit constructor arguments.
- [x] Infer constructor type arguments from assignment target type.
- [x] Infer static factory receiver type arguments from assignment target type
  only when the receiver is generic and otherwise unbound.
- [x] Infer from return context only where Qore already has a reliable expected
  type.
- [x] Reject ambiguous inference when overloads, defaults, or unrelated
  argument positions produce competing bindings.
- [x] Prefer explicit type arguments over inference.
- [x] Preserve default type argument behavior when inference does not bind an
  optional parameter.
- [x] Define diagnostics that tell users which explicit spelling to write.
- [x] Preserve `[legacy_raw]` and `[legacy_raw_construct]` raw construction
  semantics by skipping constructor argument inference for those classes.

Implementation checklist:

- [x] Reuse or extend method-level type-parameter inference helpers for class
  constructors.
- [x] Thread expected target type through declaration construction, assignment,
  and return-expression parse initialization where available.
- [x] Add inference bindings for constructor formal types containing class type
  parameters.
- [x] Apply bounds/default checks after inference.
- [x] Preserve inferred object type through IR, JIT helpers, and AOT.
- [x] Add tests for constructor inference, target-type inference, defaults,
  bounds, overload ambiguity, named arguments, AST mode, and AOT.

## Phase 3: Specialized Generic Code Generation

Goal: allow IR/JIT to emit specialized bodies for concrete generic
instantiations without changing source-visible semantics, while preserving AOT
compatibility through runtime-resolved generic metadata.

Design checklist:

- [x] Decide which execution tier owns specialization: specialized IR is
  lowered at dispatch time from the source body, JIT compiles that specialized
  IR when JIT or tiered execution is active, and AOT keeps using serialized
  generic metadata resolved at runtime.
- [x] Define specialization cache keys for class receiver type arguments,
  method/function type arguments, static generic calls, and parameterized
  hashdecl-derived records.
- [x] Keep the substitution-based interpreter path as the correctness baseline.
- [x] Define invalidation and reuse rules for source-stripped AOT and module
  loading: specialized IR is owned by the source variant and reused for the
  lifetime of that variant; source-stripped AOT bodies remain generic and
  resolve type metadata through the AOT type table at runtime.
- [x] Define when specialization is skipped, such as calls without concrete
  generic context, source-stripped AOT bodies, and failed specialized lowering.
  These paths use the substitution-based AST or runtime-metadata execution path.
- [x] Define observability rules so reflection and stack traces still describe
  the source method/function.

Implementation checklist:

- [x] Introduce an internal specialization descriptor.
- [x] Intern or cache descriptors per variant.
- [x] Clone or lower IR with concrete substituted types.
- [x] Make JIT lookup specialization-aware while keeping fallback execution.
- [x] Preserve AOT compatibility without adding new metadata by serializing
  generic hashdecl type paths and resolving them through the runtime
  receiver/type-argument context.
- [x] Add correctness tests that compare specialized and non-specialized paths.
- [x] Add stress tests for cache reuse and mixed instantiations.
- [x] Add performance-focused smoke tests only after correctness is stable.

## Phase 4: Doxygen And User Documentation

Documentation checklist:

- [x] Update `doxygen/lang/155_data_type_declarations.dox.tmpl`.
- [x] Update `doxygen/lang/215_classes.dox.tmpl`.
- [x] Update `doxygen/lang/217_hashdecl.dox.tmpl`.
- [x] Update `doxygen/lang/220_threading.dox.tmpl` if builtin generic examples
  change.
- [x] Update `doxygen/lang/900_release_notes.dox.tmpl`.
- [x] Include examples for defaults, bounds, method-level generics, explicit
  generic call type arguments, generic hashdecl inheritance, variance/wildcards,
  expression-site inference, and any visible specialization behavior.
- [x] Keep examples concrete and executable where practical.

## Phase 5: Local Verification

Run focused checks after each implementation phase, then run a complete local
verification before the final commit:

- [x] `cmake --build build --target qore -j$(nproc)`
- [x] `cmake --build build --target qpp astparser -j$(nproc)`
- [x] `./run_tests.sh -d qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=ast examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=ir examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=jit examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=tiered examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qcc -o build/generic-classes-qtest-aot examples/test/qore/misc/generic-classes.qtest`
- [x] `build/generic-classes-qtest-aot`
- [x] `build/qore modules/astparser/test/astparser.qtest`
- [x] Relevant docs fast targets, such as `make docs-lang-final-fast`.
- [x] Full `./run_tests.sh` attempted; branch regressions found by the run were
  fixed and rechecked with focused tests. The remaining local failures are
  environment/data setup issues: PostgreSQL is not running, the Qorus test data
  index is not readable, and Tesseract English data is missing.
- [x] Final post-cleanup `git diff --check`.
- [x] Apply `audit-changes` before each commit.
