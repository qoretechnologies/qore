# Unified Const Follow-Up Plan

Status: pending execution plan for readonly `const` binding hardening and
builtin const-method annotation follow-up work.

## Ordering

Execute the readonly-binding hardening work first. Builtin const-method
annotations make more readonly receiver calls legal, so annotation expansion
should happen only after the shared readonly infrastructure is verified:
lvalue-root analysis, reference bypass rejection, IR/AOT verification, and
cross-mode tests.

The builtin candidate inventory is safe to start early because it is
read-only analysis. Actual annotation changes must wait until the readonly
foundation phases are complete.

## Phase 0: Baseline Inventory

Create two inventories before behavior changes:

- a readonly-binding behavior matrix recording current parser, AST execution,
  IR interpreter, JIT, and AOT behavior
- a builtin const-method candidate inventory

Readonly-binding matrix entries:

- local readonly declarations, closure captures, and readonly parameters
- missing initializer rejection and repeated initialization on each call or loop
  entry
- direct assignment, compound assignment, increment/decrement, `remove`, and
  `delete`
- index/member writes and in-place mutating operators
- mutable reference creation and writable reference-parameter calls
- comma declaration forms
- `%require-types`, `%allow-bare-refs`, and old-style `$` syntax interactions
- `foreach const`, `catch (const ...)`, and `our const`

Builtin candidate inventory entries:

- methods already carrying const-method metadata
- instance methods tagged `QCF_CONSTANT`
- instance methods tagged `QCF_RET_VALUE_ONLY`
- read-only inspector/accessor methods without either code flag
- methods that return references, iterators, views, or call references

For each builtin candidate, record class or pseudo-class, method name, current
flags, current const-method state, return/alias behavior, and initial decision:
`const`, `non-const-mutates`, `non-const-alias-risk`, `non-const-dynamic`, or
`needs-review`.

Suggested discovery commands:

- `rg -n "QCF_CONSTANT|QCF_RET_VALUE_ONLY|add.*Method|\\[const\\]" lib include modules qlib`
- `rg -n "Pseudo_QC_|add.*PseudoMethod|iterator\\(|Reference|view\\(|materialize\\(" lib include modules`

Expected outputs:

- a checked-in behavior matrix or test-comment block for readonly bindings
- an updated builtin audit table covering every candidate found
- one gap list, with each item mapped to parser, runtime, IR, AOT, tooling, or
  documentation

Suggested verification:

- `./run_tests.sh -d qore/misc/readonly-const-bindings.qtest`
- `./run_tests.sh -d qore/parser/const-methods.qtest`
- targeted parser-only checks for syntax that should reject before execution

## Phase 1: Readonly Syntax Boundary

Make statement-scope readonly syntax unambiguous before deeper runtime or
compiled-path work:

- reject `foreach const ...` and `catch (const ...)` unless a complete separate
  initialization/rebinding design exists
- keep `our const` out of the readonly-binding feature
- reject ambiguous comma declarations such as `const int a = 1, b = 2`
- preserve the structural split between namespace/class constants and local
  readonly bindings
- mirror accepted and rejected forms in astparser and QoreCodeFormat
- verify `%require-types`, `%allow-bare-refs`, and old-style `$` behavior

Expected outputs:

- parser tests for accepted and rejected local readonly declarations
- astparser/QoreCodeFormat tests proving the same syntax classification
- documentation text for comma declarations and unsupported contexts

Suggested verification:

- `./run_tests.sh -d qore/misc/readonly-const-bindings.qtest`
- `./run_tests.sh -d qlib/QoreCodeFormat/QoreCodeFormat.qtest`
- astparser qtest with the local module path used by this branch

## Phase 2: Readonly Lvalue And Reference Bypass Closure

Audit every path that can acquire a readonly binding as a writable lvalue.
Static cases should reject during parse analysis; dynamic-only cases must raise
`RUNTIME-READONLY-VIOLATION`.

Cover at least:

- direct assignment, compound assignment, increment/decrement, `remove`, and
  `delete`
- index/member lvalue writes and in-place mutating operators
- parse references such as `\x`
- `reference<T>` and `*reference<T>` construction rooted at readonly values
- passing readonly values to writable reference parameters
- readonly bindings whose own value type is `reference<T>` or `*reference<T>`
- closure-variable lvalue writes and local/closure removal through runtime APIs
- mutable alias positive cases that should remain allowed

Expected outputs:

- one negative test per bypass class
- runtime tests for paths that cannot be proven statically
- clear diagnostic split between parse-time
  `READONLY-VARIABLE-ASSIGNMENT-ERROR` and runtime
  `RUNTIME-READONLY-VIOLATION`

Suggested verification:

- `./run_tests.sh -d qore/vars/reference.qtest`
- `./run_tests.sh -d qore/misc/readonly-const-bindings.qtest`
- focused IR/JIT runs for reference-heavy negative cases

## Phase 3: IR, JIT, And AOT Verification Foundation

Treat parser checks as the first line of defense, not the only line. The IR/AOT
verifier must reject malformed or hand-crafted compiled artifacts that encode
readonly violations. This phase gates builtin annotation expansion.

Cover at least:

- `StoreLocal`, `StoreClosure`, fused local updates, and lvalue-path
  store/remove/delete records
- index stores and in-place mutation records rooted at readonly values
- `CreateParseRef` and reference-foreach setup rooted at readonly values
- preservation of readonly and `initial_assignment` bits in AOT metadata
- clean rejection of new artifacts by old runtimes through unknown feature flags
- malformed new artifacts missing `QORE_AOT_FEAT_READONLY_LOCALS` when the
  header/version metadata makes the mismatch detectable
- const-method AOT metadata and readonly receiver call regression tests

Expected outputs:

- IR verifier tests for each instruction family
- AOT round-trip tests for readonly locals, readonly parameters, closure
  captures, const methods, and injected-write rejection
- updated stable design notes for verified compatibility behavior

Suggested verification:

- `./run_tests.sh -d qore/misc/readonly-const-bindings.qtest`
- focused AOT qtests under `examples/test/ir/`
- JIT and AOT execution-mode runs for the same readonly corpus

## Phase 4: Core Const Regression Gate

Before expanding builtin annotations, prove the implemented const-method core
still behaves correctly after readonly hardening:

- const method syntax and invalid declaration rejection
- readonly receiver calls to const and non-const user methods
- const method body rejection for self-rooted mutation
- override and abstract method constness rules
- methodGate/memberGate const behavior
- reflection and API metadata for user const methods
- QoreCodeFormat and astparser trailing `const` preservation

Expected outputs:

- green targeted const-method parser, reflection, metadata, formatter, and AOT
  tests
- no regressions in readonly-binding tests from Phases 1-3

Suggested verification:

- `./run_tests.sh -d qore/parser/const-methods.qtest`
- `./run_tests.sh -d qore/misc/reflection.qtest`
- `./run_tests.sh -d qlib/QoreApiMetadata/QoreApiMetadata.qtest`
- `./run_tests.sh -d qlib/QoreCodeFormat/QoreCodeFormat.qtest`
- focused AOT const-method tests under `examples/test/ir/`

## Phase 5: Alias-Sensitive Builtin Decisions

Resolve the deliberately unannotated pseudo-methods before broad builtin class
annotation. These methods are most likely to expose writable receiver-rooted
aliases.

Audit:

- `<list>::first()`, `<list>::last()`, `<list>::iterator()`, and
  `<list>::rangeIterator()`
- `<hash>::values()`, `<hash>::firstValue()`, `<hash>::lastValue()`,
  `<hash>::iterator()`, `<hash>::keyIterator()`, `<hash>::pairIterator()`, and
  `<hash>::contextIterator()`
- `<object>::iterator()`, `<object>::keyIterator()`,
  `<object>::pairIterator()`, and `<object>::getCallReference()`
- `<value>::iterator()`
- `<buffer>::view()` and `<buffer>::materialize()`
- `<callref>::exec()`

Decision rules:

- mark const only when the method does not mutate receiver state and does not
  expose a writable self-rooted alias
- leave iterator/view/reference-producing methods non-const unless the returned
  object is proven to be a detached snapshot or read-only view
- leave methods that execute arbitrary user code non-const unless a separate
  dynamic const-dispatch design exists

Expected outputs:

- an updated audit table with one row per alias-sensitive method
- tests showing readonly receiver calls are accepted for newly const methods and
  rejected for methods intentionally left non-const

## Phase 6: Builtin Annotation Batches

Apply const annotations in focused batches by class or pseudo-class. Prioritize
immutable or inspection-heavy receiver types before service objects:

1. Type, reflection, metadata, and immutable view classes.
2. Date/time, string-like, collection wrapper, and diagnostic classes.
3. Data/model/package descriptor classes that mostly expose accessors.
4. Service objects such as locks, sockets, files, datasources, streams, and
   connection managers.

For each batch:

- mark read-only inspectors/accessors const when safe
- leave mutating methods and stateful operations non-const
- document every `QCF_CONSTANT` instance method that remains non-const
- avoid inferring constness from `QCF_RET_VALUE_ONLY`; audit the receiver
  contract directly
- update readonly receiver parser tests
- update reflection tests for `isConstMethod()` and `"const"` / `MC_CONST`
- update API metadata tests for `is_const_method`
- add AOT metadata round-trip tests when declarations are serialized or used by
  compiled consumers

Suggested verification:

- `./run_tests.sh -d qore/parser/const-methods.qtest`
- `./run_tests.sh -d qore/misc/reflection.qtest`
- `./run_tests.sh -d qlib/QoreApiMetadata/QoreApiMetadata.qtest`
- focused AOT const-method tests under `examples/test/ir/`

## Phase 7: Tooling, Documentation, And Compatibility Closeout

Close both follow-up documents only after implementation and annotation work has
been verified as a single feature surface:

- astparser exposes local readonly declarations and const methods without
  confusing either with parse-time constants
- QoreCodeFormat round-trips readonly declarations and const methods
- reflection and API metadata agree with runtime declarations
- language documentation describes shallow readonly binding semantics,
  unsupported contexts, reference-bypass behavior, const method receiver rules,
  builtin annotation policy, error classes, and execution-mode guarantees
- corpus parse/format checks over `qlib/`, `examples/test/`, and representative
  modules show no unexpected reclassification of existing constants or locals

Completion requires:

- every Phase 0 readonly matrix entry resolved or explicitly moved to future
  design scope
- every Phase 0 builtin candidate has a final decision and rationale
- all bypass-sensitive negative tests covered in every applicable execution mode
- documented reasons for cases rejected before IR/JIT/AOT
- safe builtin candidates annotated const
- alias-risk methods tested as rejected on readonly receivers
- stable semantics promoted into `design/readonly-const-bindings.md`,
  `design/const-methods.md`, or language documentation
- pending follow-up docs reduced to future-only items or deleted
