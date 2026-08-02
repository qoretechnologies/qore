# `%try-child-module` Parse Directive

/*  parse-directive-try-child-module.md Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

## Status

Implemented; see issue #5364.

## Motivation

Modules extend base modules today by pushing themselves into the base: the
extender declares `%requires(reexport) Base`, and in its `init` closure calls
`Base::registerChild(...)` and `DataProviderActionCatalog::registerAction()`
with `"app": Base::AppName`.  `QorusOpenAiServices`, `QorusDiscordServices`,
`QorusGoogleServices` and `QorusSlackServices` all work this way.

The dependency direction is correct and must be preserved — it is what lets a
proprietary module extend an open one.  The problem is **activation**: the
registration only happens if something loads the extender, and nothing does
unless a solution names it explicitly, so extension actions are silently absent
rather than diagnosably missing.

`%try-child-module` inverts only the *declaration*, not the dependency: the
base module declares which optional extensions may exist, and the module loader
attaches them once the base is fully loaded and published.

```qore
# in the parent module
%try-child-module SalesforcePubSubDataProvider
```

## Why the obvious approaches do not work

| Mechanism | Result |
|---|---|
| `%try-module Ext` in the parent | fails — parse time, far too early |
| `load_module("Ext")` in the parent's `init` closure | fails — `PARSE-EXCEPTION: cannot find any namespace or class 'Base' in 'Base::x'` |
| `load_module("Ext")` lazily on first use | works, but activation is tied to a call, cannot distinguish absent from broken, and needs boilerplate in every extensible base |

The middle row is the important one, and the cause is visible in the loader.
While a user module's `init` closure runs, the module's feature is reserved in
`QoreModuleManager::module_load_map` as `MLS_INITIALIZING` owned by the current
thread (`ModuleLoadMapHelper`, wrapping `QoreModuleDefContext::init()` in
`QoreModuleManager::setupUserModule()`).  When the child then parses
`%requires(reexport) Base`, `QoreModuleManager::loadModuleIntern()` matches
`e.owner_tid == q_gettid()` and returns `nullptr` **with no exception**, so the
parent's namespace is never merged into the child's `QoreProgram` and every
`Base::` reference fails to resolve.  The parent is only published into the
module map by `setupUserModule()`, strictly after both the parse-phase and
init-phase reservations have been released.

Therefore child modules can only be attached **after the parent module load has
returned**, which is what this directive implements.

## Syntax

```
%try-child-module <feature> [<|<=|=|>=|> <version>]
```

The argument uses the same module specification grammar as `%requires` and
`%try-module`, including optional version constraints.  There is no block form
and no `%endtry`: the directive is a declaration, not a conditional parse
region.

The directive is only valid inside a user module (a `.qm` file, or one of the
`.qc` / `.ql` files of a separated module).  Using it anywhere else is a parse
error.

Filesystem paths are rejected exactly as they are for `%requires` (see
`parse_check_no_filesystem_module_path()`): a child is always named by feature.

## Semantics

| Situation | Behaviour |
|---|---|
| child not installed | silent; status `absent`; the parent works exactly as before |
| child installed and loads | status `attached`; the child's `init` has run before the parent's load returns |
| child installed but broken (parse error, init error, version mismatch) | the child's own exception is raised, decorated with the parent and child names; the parent's load fails |
| child load already in progress on this thread | no-op; status `attached`; the in-flight load completes normally |
| parent module `Program` has `PO_NO_MODULES` | status `skipped`, `MODULE-CHILD-SKIPPED` warning; retried on a later load where modules are allowed |
| module declares itself as a child | parse error |
| same child declared twice by one module | parse error |

Failures are **sticky**: once a declared child is found present-but-broken, the
recorded error is re-raised on every subsequent load of the parent, so the
parent never silently starts working after a failed extension load.  This keeps
`%requires Parent` deterministic: it either always succeeds or always fails for
a given installed file set.

### Attach point

Children are attached at the **outermost module-load boundary on the current
thread** (`module_load_depth == 0`), not inline at the end of the parent's own
load.

If the parent is loaded from within another module's parse — for example a
program requires `X`, and `X` requires the parent `P` — then attaching `P`'s
children immediately would run the child's parse while `X` is still reserved as
`MLS_INITIALIZING` on this thread.  A child that (directly or transitively)
requires `X` would then fail to resolve `X`'s symbols: exactly the failure mode
described above, moved up one level.  Deferring the attach until the outermost
load completes removes that class of failure by construction.

Within one drain, children attach in declaration order, depth-first: a child
that itself declares children has them attached before the next sibling of the
first child.

The attach runs **before** the parent is merged into the requesting
`QoreProgram`, so a broken child is all-or-nothing for the caller.

If the outer load is already failing when the queue is drained, statuses are
still recorded but no new exception is added to the caller's sink; the recorded
failure is raised on the next load attempt.

### Program and parse-option context

The child is loaded with `pgm = nullptr`: it is registered globally and its
`init` runs, but its namespace is **not** merged into any `QoreProgram`.  A base
module must not gain symbols from an optional extension, otherwise the parent's
API surface would depend on which extensions happen to be installed.  Code that
wants the child's classes must `%requires` the child itself.

Parse options and the `%prepend-module-path` / `%append-module-path` search
lists for the child load are taken from the **parent module's** `QoreProgram`
(passed as `path_pgm`), not from whichever program happened to trigger the
load.  This makes child resolution deterministic and lets a parent loaded from a
non-standard search path find its children in that same path.

### `PO_NO_MODULES`

`load_module()` carries the `MODULES` functional domain, and hosts such as
Qorus parse interface code with restricted parse options, so the behaviour must
be defined rather than accidental.

`%requires` and `Program::loadModule()` already fail on a `PO_NO_MODULES`
program before any parent could be loaded, so the reachable case is
`Program::loadApplyToUserModule()`, which passes a restricted program as the
module container.  In that case the parent module's own `QoreProgram` carries
`PO_NO_MODULES`.

The attach step checks that program explicitly (the child load itself passes
`pgm = nullptr` and so would bypass the loader's own check) and:

- does not load the child,
- records status `skipped`,
- raises a `MODULE-CHILD-SKIPPED` warning under the `QP_WARN_MODULES` mask, and
- leaves the declaration non-terminal.

A module's `QoreProgram` does not change after it is loaded, so the skip is in
practice permanent for that module instance: every program that loads the
parent gets the warning again, and the child only attaches if the module itself
is later reloaded (for example by reinjection) into a container that allows
module loading.  Honoring the container's restriction is the point — a module
placed in a sandbox must not load further modules behind the host's back.

### Teardown ordering

A child module holds references to the parent's classes and registers data into
the parent's structures, so the parent must be destroyed after the child.  A
child that declares `%requires(reexport) Parent` records that dependency
automatically.  For children that do not, the attach step records the same edge
explicitly with `setUserModuleDependency(parent, child)` when both modules are
user modules (binary modules are not torn down by `delUser()` and must not be
tracked).

### Cycles and re-entrancy

- Two modules naming each other as children: the second attach re-enters for a
  parent already being attached on this thread and is skipped via a
  thread-local in-progress set, so both modules load and neither recurses.
- A child loaded explicitly, pulling its parent in via `%requires`: when the
  parent then attaches that same child, the loader sees the child's own
  in-progress reservation owned by this thread and returns without error; the
  in-flight load completes normally and the child is recorded as attached.
- Concurrent loads of the same parent on two threads need no new lock: both
  threads run the attach loop and serialize per child on the existing
  `module_load_map` reservation, and status bookkeeping is idempotent under the
  module-manager mutex.

## Introspection

`QoreAbstractModule::getHashIntern()` adds a `"child-modules"` key when the
module declares any children, so `get_module_hash()` and `get_module_list()`
answer "did my extension attach?" directly.  The value is a hash keyed by child
feature name in declaration order:

```qore
{
    "SalesforcePubSubDataProvider": {
        "status": "attached",       # attached|absent|skipped|failed|pending
        "spec": "SalesforcePubSubDataProvider",
        "err": NOTHING,             # set only for "failed"
        "desc": NOTHING,            # set only for "failed" and "skipped"
    },
}
```

## AOT-compiled modules

`QORE_BUILD_AOT_MODULES` defaults to `ON` and a `.qmod` artifact wins over the
`.qm` source in the module search order, so a qlib module normally reaches
deployments as an AOT binary module.  Child declarations must therefore survive
AOT compilation, otherwise the feature would be silently inert exactly where it
matters.

- `QoreAOTModuleInfo` gains a `child_modules` list, populated by the same raw
  source scan that collects `%requires` dependencies.  Declared children are
  **not** emitted as module dependencies — a dependency is mandatory and loads
  before `init`, a child is optional and loads after.
- The generated module description function emits the list and calls
  `qore_aot_fill_module_children()`, a separate entry point from
  `qore_aot_fill_module_desc()` so that previously compiled artifacts, which
  never call it, keep working unchanged.
- `QoreModuleInfo` gains a matching `child_modules` field; the loader copies it
  onto the module object, after which binary and user modules share one attach
  implementation.
- The Program that supplies parse options and the module search path for the
  child load comes from `qore_aot_get_module_pgm()` for a binary module, since
  an AOT-compiled user module has no `QoreUserModule` but does register a module
  Program with the AOT runtime.  Without this, a child installed next to its
  AOT-compiled parent outside the standard search path is reported absent.
- An AOT child loaded with `pgm = nullptr` is initialized in its private module
  Program after registration.  This executes the child's `init` closure just as
  source-module loading does, while preserving the rule that the optional
  child's namespace is not merged into the parent.  Module-init side effects
  execute only during the first shared AOT initialization; importing the child
  explicitly later does not repeat registrations or other global effects.  An
  exception from that initializer retains its original error code and detail,
  marks the child attachment as failed, and is raised again on later attempts
  to load the parent, matching the source-module failure contract.
- The directive is stripped from embedded source before it is re-parsed at
  runtime (`stripRequiresDirectives()`), because at that point the declaration
  has already arrived through the description function.

## Files

| File | Change |
|---|---|
| `lib/scanner.lpp` | `%try-child-module` directive |
| `include/qore/intern/qore_thread_intern.h` | `QoreModuleDefContext::child_vec`, `addChild()` |
| `include/qore/intern/ModuleInfo.h` | child list + status on `QoreAbstractModule`; `attachChildModules()` |
| `lib/ModuleManager.cpp` | declaration transfer, attach engine, deferral, introspection |
| `include/qore/ModuleManager.h` | `QoreModuleInfo::child_modules` |
| `include/qore/intern/QoreAOT.h` | `QoreAOTModuleInfo::child_modules` |
| `lib/QoreAOT.cpp` | source scan + description-function emission |
| `lib/QoreAOTRuntime.cpp` | `qore_aot_fill_module_children()`, source stripping |
| `modules/astparser/grammars/tree-sitter-qore/grammar.js` | directive in the tree-sitter grammar |
| `doxygen/lang/245_parse_directives.dox.tmpl` | directive reference |
| `doxygen/lang/120_modules.dox.tmpl` | child-module section for user modules |

## Out of scope

Two related gaps noted in issue #5364 remain open and are independent of this
directive:

- `AbstractDataProvider::registerChild()` has no defined duplicate/conflict
  policy; each of the 27 overriding modules chooses its own.
- Child registration and action registration are two unconnected steps, with
  nothing checking that an action's `path` resolves to a registered child.
