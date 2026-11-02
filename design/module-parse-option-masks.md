# Module Restrictions and Module Parse Option Masks

Copyright (C) 2026 Qore Technologies, s.r.o.

## Rules

- A user module (source `.qm` or AOT `.qmod`) inherits the restrictions of the Program that loads it **first**, and
  is refused by it if its code uses functionality those restrictions forbid. The two forms of a module are loaded
  under the same conditions.
- A module that is already loaded is **not** checked against the restrictions of a Program that imports it: it is a
  capability that the host gave the Program, and module code runs with the restrictions of its callers as well as
  its own (`doxygen/lang/120_modules.dox.tmpl`, "Module Code at Run Time"). Qore's own tests rely on this (a
  `PO_NO_THREAD_CLASSES` Program importing `QUnit`, a `PO_NO_FILESYSTEM` Program importing `Util`).
- A `thread_local` declaration in a user module (`PO_IN_MODULE`) is not refused by `PO_NO_THREAD_CONTROL`
  (`qore_ns_private::checkGlobalVarDecl()`): thread-local storage is not thread control. In user code it still is.

## First load: source and AOT

| Source module | AOT module |
|---|---|
| the module Program inherits the loader's restrictions (`get_masked_module_parse_options()`, `lib/ModuleManager.cpp`) and the parser refuses code that conflicts with them | the image's `FUNCTIONAL_DOMAIN` section (the compiling Program's `aot_used_dom`) is compared with the inherited restrictions (`qore_aot_module_init_impl()`, `lib/QoreAOTRuntime.cpp`) |

`aot_used_dom` is filled by `qore_program_private::parseAddDomain()`, which every parse-time restriction check goes
through: builtin functions, methods and classes, and, through `parse_check_restriction()` (`lib/thread.cpp`), the
checks of the code itself: `background`, `thread_exit`, backquotes, definitions of functions, classes, constants and
namespaces, `new`, `%include` and the other filesystem directives, module path directives and summarize. Before,
those were checked with `parse_check_parse_option()`, which records nothing, so an AOT module was loaded by Programs
that refused its source.

### The functional domain of a Program

`parseAddDomain()` records a restricting domain in the Program's functional domain (`dom`) only when the Program's
own restrictions refuse it (since 899a44f110, Qore 0.8.12; before, every use was recorded). `dom` was only used by the
import check of `QoreUserModule::addToProgramImpl()`, which therefore never refused a module; that inert check has been
removed rather than restored, because restoring it refused imports that Qore relies on (see Rules).

## Module parse option masks

A mask (`QoreModuleParseOptionMask`, `include/qore/intern/ModuleInfo.h`) has `strip` and `add` sets of restrictions:
a masked module inherits `(inherited & ~strip) | add`. Only restrictions can be masked
(`QoreModuleParseOptionMask::maskable()`, public as `ModuleManager::getMaskableParseOptions()`): explicitly-allowed
(positive), language (free) and style options and the options every user module has are refused with
`MODULE-PARSE-OPTION-MASK-ERROR`, as is an option in both sets of one mask. When masks are combined, an option added by
one is not stripped.

| Mask | Set by | Applies to |
|---|---|---|
| process mask | `set_module_parse_option_mask()`, `ModuleManager::setModuleParseOptionMask()` | every user module loaded afterwards (replaces the previous one) |
| load mask | `load_module_masked()`, `ModuleManager::runTimeLoadModuleMasked()` | the module and every module loaded while it is loaded, in this thread (`thread_load_po_mask`), and the module if it was loaded already |
| module mask | recorded on the module at load (process mask + load mask); `add_module_parse_option_mask()`, `ModuleManager::addModuleParseOptionMask()` | adding a mask also strips/adds the restrictions on the module Program's parse options, with which its code runs (stripped restrictions are not restored later) |

### Privilege

The Qore functions carry the `PROCESS` and `MODULES` functional domains, so code with `PO_NO_PROCESS_CONTROL` (or
`PO_NO_MODULES`) cannot call them, and they refuse a caller governed by a `SandboxManager`
(`qore_find_thread_sandbox_manager_ref()`, which includes managers inherited by threads). The C++ API is not
restricted.

### Use

Code that may not use a functionality loads a trusted module that uses it internally when the module is loaded with a
mask that strips the restriction, for example `PO_NO_THREAD_CONTROL | PO_NO_THREAD_CLASSES` for a module that uses
threads.

## Tests

`examples/test/ir/ModuleParseOptionMask.qtest` runs every case with the source modules and with AOT modules compiled
with `--compile-module`, in every execution mode, against the output of the source modules with the AST interpreter.
