# Functional-domain declarations on Qore code (qore#5803)

Functions and methods written in Qore declare the functional domains they provide, as builtin code does in qpp, and
a user module declares the domains its own code uses.  User documentation: `doxygen/lang/200_functions.dox.tmpl`
(`function_domains`) and `doxygen/lang/120_modules.dox.tmpl` (`user_module_declarations`, key `domains`).

## Syntax

```
public sub register(string name, code c) [dom=PROCESS] { ... }
string sub fetch(string url) [dom=NETWORK,FILESYSTEM;] { ... }
static setLogger(*LoggerInterface l) [dom=PROCESS] { ... }
int count() const [dom=PROCESS] { ... }
abstract reg(string name) [dom=PROCESS];
constructor(string s) [dom=PROCESS] : Base(s) { ... }
static Cls::m() [dom=PROCESS] { ... }                 # out-of-line
```

- The attribute list follows `)` and `const`, and precedes a base class constructor list and `returns`.
- `key=value,...` elements separated by `;`, optional trailing `;`.  Only key: `dom`.  Domain names are qpp's
  (`dmap` + `dnmap` in `lib/qpp.cpp`), case-insensitive, without `QDOM_`; the table is `source_domain_names` in
  `lib/QoreProgram.cpp` (`qore_get_source_domain()`, `qore_describe_source_domains()`), checked against `QDOM_ALL`
  by a `static_assert`.
- **Scanner token.**  `[` followed by `ws* dom ws* =` (not `==`, `=~`) is `TOK_ATTR_OPEN` (`lib/scanner.lpp`).  With
  plain `'['` the out-of-line form `Cls::m(args) [` conflicts with an index expression `Cls::m(x)[i]` (the
  out-of-line rules share the call prefix, see the note above `outofline_methoddef`); the token makes every form
  conflict-free (bison reports only the pre-existing r/r conflict).  Cost: `x[dom = y]` is no longer an index
  expression (release note); an unknown key `[foo=...]` is a plain syntax error.
- `method_qualifier` is a `CallableQualifier {mod, dom}` (`include/qore/intern/ParserSupport.h`); `sub_def` /
  `scoped_sub_def` take `callable_attrs_opt`.  Destructor, `copy`, `memberGate`, `methodGate`, `memberNotification`
  refuse a domain (`set_method_dom()`); closures have no attribute list (syntax error).
- tree-sitter: `callable_attributes` / `callable_attribute` nodes (`grammar.js`), highlighted in `highlights.scm`,
  shown in hover (`CSTSearcher::buildFunctionSignature()`), kept by `QoreCodeFormat` (`callableAttributes`), and
  documented by qdx with qpp's `@par Restrictions:` paragraph (`AstProcessor::addDomainRestrictions()`).

## Storage

`UserVariantBase::declared_domain` (set by the parser before the variant is added to its function, and by the AOT
reader); `getFunctionality()` of user variants returns `declared_domain | class domain`.
`QoreFunction::parseGetUniqueFunctionality()` computes the AND over the variants on each call: user variants added
while parsing (`addPendingVariant()`) never updated the cached `unique_functionality`, so it was always 0 for user
functions.

## Parse-time checks (direct references)

The existing `parseAddDomain()` sites use the variant's (or the function's unique) functionality: calls, static
calls, typed method calls, constructors (`new`, variable declarations, base class constructor lists), `\f()`.  Added:
`\Cls::m()`, typed `\obj.m()`, `\m()` / scoped self references (`check_call_reference_domain()` in
`lib/CallReferenceNode.cpp`); `\Cls::m()` now checks `parse_context.pgm`.  Messages name the denied domains and the
restricting option (`invalid_access()`, also for builtins).

## Run-time checks (indirect calls)

`qore_get_variant_denied_domains()` (`lib/Function.cpp`) is used by `runtimeFindVariant()`, `checkVariantDomain()` and
`runtimeGetCallVariants()`.  A user variant's **declared** domains are checked against the caller's restrictions, not
against `getParseOptions()` (the variant's own body options, which made a declaration meaningless at run time); the
rest (class domain, builtins) is checked as before.

- Caller = `CodeEvaluationHelper::caller_runtime_po` (taken before `init()` switches to the callee's Program;
  includes the restrictions module code inherits from a sandboxed caller).
- **Call references run with the permissions of their creator** (David, 2026-11-11): the value reference types
  (`FunctionCallReferenceNode`, `StaticMethodCallReferenceNode`, `MethodCallReferenceNode`, the run-time method
  references) capture `runtime_get_parse_options()` when created (`ResolvedCallReferenceNodeIntern::
  captureCreatorParseOptions()`); `execValue()` marks the call with `QoreCallReferenceCallHelper(args, creator_po)`;
  the `CodeEvaluationHelper` with the **same `args` pointer** takes the marker before evaluating arguments
  (`runtime_take_call_reference_call()`), so a call in an argument expression is not mistaken for the reference's
  call.  A path that rewraps the arguments leaves the marker untaken: the call is then checked against the caller,
  the stricter check.  In-place references (parse-tree `Local*`/`Deferred*` nodes) record nothing.
- Variants bound at parse time are not checked again (as builtins); module-internal calls are therefore free.
- Implicit base class constructors are resolved at run time and checked against the code running `new`.
- No-domain fast path: `qore_get_variant_denied_domains()` returns at once for functionality 0.

IR/JIT/AOT: the IR analysis passes skip user variants before looking at functionality.  Variants with declared
domains are never put in a run-time dispatch cache (IR interpreter inline method cache:
`tryExecuteInterpreterInlineIRDotEvalMethod()`; AOT/JIT polymorphic dispatch cache in `JITRuntime.cpp`), so each
run-time resolution is checked; hits for other variants are unchanged.

## Overrides

`MethodFunctionBase::checkDomainOverrides()` (next to `checkConstOverrides()`): an override of a user method variant
with declared domains must declare all of them (`DOMAIN-METHOD-ERROR`); it may add domains.  Static methods do not
override.  Builtin base methods are not covered (their class domain is inherited by subclasses).

## Module opt-in (`domains` module key)

A module Program inherits the loader's restriction options, so module code using a tagged API (or a builtin of a
domain) cannot be loaded by a restricted Program.  `module X { ...; domains = "PROCESS"; }`
(`QoreModuleDefContext::setDomains()`):

- only restriction domains (not `INJECTION`, `DEBUGGER`, `IN_MODULE`);
- recorded always (`qore_program_private_base::module_own_dom`, written to the AOT `MODULE_DOMAINS` section);
- **applied only to library code**: a module whose file is under a process-wide module directory
  (`QoreModuleManager::isTrustedModulePath()`, the rule already used for sandbox provenance), read from the module
  load frame (`qore_module_load_is_trusted()`).  Applying removes the restrictions from the module Program's options
  and their `%ifdef PO_NO_*` defines; `QoreUserModuleDefContextHelper::close()` keeps them removed when it restores
  the pre-header options;
- the header's own lockdown (`MOD_HEADER_PO`, applied to the `init`/`del` closures) does not restrict the declared
  domains, unless they are inherited (`QoreUserModuleDefContextHelper::applyDomains()`);
- each module declares its own (no propagation to its dependencies).

AOT: the loader reads `MODULE_DOMAINS` and, for a trusted qmod, removes the domains from the module Program and from
the inherited options before the `FUNCTIONAL_DOMAIN` check (`AOT-MODULE-RESTRICTED`).

## AOT format

v25: signature flag bit 4 (`0x0010`) + `i64` declared domains after the variant parse options
(`writeVariantSignature()`, `readAndSetupVariantSignature()`, `skipAOTVariantSignature()`); included in the variant
declaration digests.  `MODULE_DOMAINS` section (33).  AOT identity changes (generated from the format sources).

## qlib

37 static registration and setter APIs in DataProvider, AbstractDataProvider, DataProviderActionCatalog and
WebhookSupport declare `[dom=PROCESS]` (those with `isSandboxed()`/`checkNotSandboxed()`/`checkRegistration()`
refusals, `registerFactory()`, and the builder/scheme setters guarded in module init); the `isSandboxed()` refusals
stay (a sandbox does not imply `PO_NO_PROCESS_CONTROL`).  The 150 modules that call them declare
`domains = "PROCESS"`.  Not tagged (no refusal today, decision pending): `AbstractDataProvider::registerChild()`,
`DataProviderActionCatalog::registerLazyValue()`, `addCatalogChangeListener()`.

Follow-up (per module, needs approval): modules with `%ifdef PO_NO_PROCESS_CONTROL` / `PO_NO_THREAD_CONTROL`
workarounds (e.g. `WebSocketClient`) can declare the domains instead.
