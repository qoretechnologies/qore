# Functional-domain declarations on Qore code (qore#5803)

Functions and methods written in Qore declare the functional domains they provide, as builtin code does in qpp, and
a user module declares the domains its own code uses.  User documentation: `doxygen/lang/200_functions.dox.tmpl`
(`function_domains`) and `doxygen/lang/120_modules.dox.tmpl` (`user_module_declarations`, key `domains`).

## Syntax

```
public sub register(string name, code c) [dom=PROCESS_CONTROL] { ... }
string sub fetch(string url) [dom=NETWORK,FILESYSTEM;] { ... }
static setLogger(*LoggerInterface l) [dom=PROCESS_CONTROL] { ... }
int count() const [dom=PROCESS_CONTROL] { ... }
abstract reg(string name) [dom=PROCESS_CONTROL];
constructor(string s) [dom=PROCESS_CONTROL] : Base(s) { ... }
static Cls::m() [dom=PROCESS_CONTROL] { ... }                 # out-of-line
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
  references, and the static reference `get_call_reference()` returns) capture
  `runtime_get_effective_program_parse_options()` when created (`ResolvedCallReferenceNodeIntern::
  captureCreatorParseOptions()`); `execValue()` marks the call with `QoreCallReferenceCallHelper(rc, args,
  creator_po)`, a marker in the thread's `RuntimeConfig::Impl` (a plain load on every call; a `thread_local` cost a
  `__tls_get_addr` per call, as libqore is `dlopen()`ed and cannot use initial-exec TLS).  The first
  `CodeEvaluationHelper` set up takes the marker, before it evaluates arguments (`rc_take_call_reference_call()`):
  it is always cleared, so it never applies to a later call, and the creator's options apply only if the helper's
  argument list is the marked one; otherwise the call is checked against its caller, the stricter check.  A
  cross-Program call that evaluates the arguments first (`QoreCallProgramEntryHelper`) hides the marker while they
  are evaluated and re-targets it to the evaluated list (`QoreCallReferenceArgEvalHelper`).  In-place references
  (parse-tree `Local*`/`Deferred*` nodes) record nothing.
- Variants bound at parse time are not checked again (as builtins); module-internal calls are therefore free.
- **Module code may use the domains its module declares** (C2, David 2026-11-12: a header declaration means the
  module itself ensures safety for restricted callers): the restrictions that a declared domain is checked against
  do not include the domains that the module of the **calling code** declares in its header and that apply to it
  (`module_trusted_dom`, set when the declaration applies: library code).  The calling code's Program is taken with
  the caller's options before `init()` switches Programs (`runtime_take_call_caller_parse_options(code_pgm)`, one
  thread-data lookup for both, so the call path got cheaper: release callgrind on the call benchmark, ast -0.61%,
  ir -0.28% outside malloc) and kept in `CodeEvaluationHelper::caller_code_pgm`; the relief is computed only for a
  variant with declared domains (`qore_relieve_module_declared_domains()` in
  `qore_get_variant_denied_domains_intern()`).  A reference that module code creates while it runs with a sandboxed
  caller's restrictions (`runtime_get_module_call_program()` is its Program) is relieved too; one that the caller's
  code creates to module code is not, although the current Program can be the module's then.  The current Program
  at the check (already the callee's) and the stack location's Program do not tell the calling code; the first
  attempt used the former and let the caller's own `call_function()` of a tagged module function through.  Test:
  `user-functional-domains-modes.qtest`, with and without a `SandboxManager` (the module's own `call_function()`,
  untyped calls and references of its tagged code; the caller's closure run on a module thread stays refused; an
  untagged method using `thread_yield()` works for the restricted caller by every route).  Review fixes: the
  iterator helper of `foreach` (`AbstractIteratorHelper::getCheckVariant()`), the type-based lookups
  (`checkVariantDomain()`) and `runtimeGetCallVariants()` pass the current Program, which is the calling code's
  there (no switch).  Only the variant lookup of reflection is relieved: its own call check
  (`QoreReflectionCaller::allowsDomain()`, `reflection_check_variant_call()`) is not, so a module's reflection call of
  its own declared code is still refused for a sandboxed caller (it fails closed).  An AOT image binds variants at
  load time without a domain check (`qore_aot_resolve_variant_from_arg_type_signature()` falls back to
  `parseFindVariantNoDiagnostics()`); the image-level `FUNCTIONAL_DOMAIN` check governs there, so the current Program
  passed by `checkVariantDomain()` does not matter for it; the creator relief tests `::getProgram()`, the Program
  running the code, not the reference's `getProgram()` (the object's Program for a method reference: a module's
  `\o.m()` on an object of a caller subclass was refused).  Implicit base class constructors stay checked against
  the code running `new` (see below: no module code runs for a module subclass without its own constructor); an
  explicit base constructor list in the module is module code.  A module that hands out a generic dispatcher (`\call_function()`
  or a `call_object_method()` wrapper that it creates) lets its callers reach its declared code through it.
- The thread's module definition context (`ThreadData::qmd`) applies only while its module's Program is parsed
  (`QoreModuleDefContext::getDefinitionProgram()`, `get_module_def_context()`): a Program that a module's `init`
  closure parses applied its own `module` block to the loading module (qore#5938: a nested `version` key raised
  "given multiple times", a nested `url` key asserted, and a nested `domains` key would have added domains to the
  loading module); test `nested-module-parse.qtest`.  A `%try-child-module` in such a Program is ignored (the
  thread's module context name is still set, as for the re-parse of an AOT module), where it used to add a child
  to the loading module.
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
domain) cannot be loaded by a restricted Program.  `module X { ...; domains = "PROCESS_CONTROL"; }`
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
- the header's own lockdown does not restrict the domains declared **on user code** either, whatever the module
  declares: an `init` closure that registers a factory (`DataProvider::registerFactory()`) is the common pattern, and
  modules outside qlib must keep loading.  The bits that `MOD_HEADER_PO` adds to the module's options are recorded
  (`qore_program_private_base::mod_header_dom`) and subtracted from the parse-time denial of user-declared domains
  (`qore_program_private::parseDeniedDomains()`); header closures are parse-initialized after the header with the
  options saved when they were parsed, so the record stays for the module's parse.  Inherited restrictions are not
  in it and still apply;
- each module declares its own (no propagation to its dependencies).

AOT: the loader reads `MODULE_DOMAINS` and, for a trusted qmod, removes the domains from the module Program and from
the inherited options before the `FUNCTIONAL_DOMAIN` check (`AOT-MODULE-RESTRICTED`).

## AOT format

v33 (v25 before the merge of develop, whose batch F used v25-v28; v29 on develop, renumbered in batch I after v29-v32): signature flag bit 4 (`0x0010`) + `i64` declared domains after the variant parse options
(`writeVariantSignature()`, `readAndSetupVariantSignature()`, `skipAOTVariantSignature()`); included in the variant
declaration digests.  `MODULE_DOMAINS` section (33).  AOT identity changes (generated from the format sources).

## qlib

37 static registration and setter APIs in DataProvider, AbstractDataProvider, DataProviderActionCatalog and
WebhookSupport declare `[dom=PROCESS_CONTROL]` (those with `isSandboxed()`/`checkNotSandboxed()`/`checkRegistration()`
refusals, `registerFactory()`, and the builder/scheme setters guarded in module init); the `isSandboxed()` refusals
stay (a sandbox does not imply `PO_NO_PROCESS_CONTROL`).  The 150 modules that call them declare
`domains = "PROCESS_CONTROL"`.  Not tagged (no refusal today, decision pending):
`AbstractDataProvider::registerChild()`, `DataProviderActionCatalog::registerLazyValue()`, `addCatalogChangeListener()`.

### qlib module audit (PO_NO_PROCESS_CONTROL, PO_NO_THREAD_CONTROL)

A module declares the domains its own code uses so that a Program restricted like a Qorus interface
(`PO_NO_PROCESS_CONTROL | PO_NO_THREAD_CONTROL`) can load it; before, such a Program could use these modules only if
a less restricted Program had loaded them first (a loaded module is not checked again: `QoreUserModule::addToProgramImpl()`).
A declaration must not turn the module's public API into a bypass, so for each module:

- **tag** (`[dom=...]`) the public functions and methods whose effect is the restricted capability itself:
  process-wide state (environment variables, registrations, process-wide limits and settings), exiting the process,
  signal handlers; a restricted caller is then refused at the API, at parse time or at run time;
- **declare without tagging** what the module does for its own work: threads it starts for its own operations
  (`background`, timers and tasks: the scheduling builtins are `MODULE_OWNED` and refuse a restricted caller's own
  code, and `background` in module code is checked against the module's options, see `222_sandboxing.dox.tmpl`),
  thread data it keeps, waits between retries or polls (`usleep()`, `sleep()`: these let a caller wait, as
  `Counter::waitForZero()` with a timeout already does; decided (David, 2026-11-12): modules that wait declare `PROCESS`; the `%ifdef` workaround code stays for a
  module loaded by its path, where the declaration does not apply (C1);
- **guard** module code that would use a capability for its caller: QUnit exits the process for a usage error and
  installs its `SIGUSR1` handler only if no Program in the call stack restricts process control
  (`QUnit::Priv::callersMayControlProcess()`), as `ConnectionSchemeCache::deregisterScheme()` and
  `DataProviderRetryUtil::setTransportRetryDelayUnit()` already did;
- the scheduling wrappers `AsyncSocketIoController::submitTask()`, `addTimer()` and `cancelTimer()` are not tagged:
  module code arms its own timers for restricted callers (`ir/ModuleOwnedAsync.qtest`; WebSocketClient and
  DiscordWebSocketClient do), and for a sandboxed caller (a Qorus interface has a `SandboxManager`) the
  `MODULE_OWNED` checks of the builtins refuse the caller's own code and timers; a Program restricted only with
  parse options has no such composition, so its closures can run on module threads (C4: kept as designed and
  documented, David 2026-11-12: parse options restrict a Program's own code, a `SandboxManager` contains it); modules with
  `%ifdef PO_NO_THREAD_CONTROL` workarounds (QoreAgentStrategies, QoreStructuredExtraction, WebSocketClient, ...)
  do not declare `THREAD_CONTROL`, which would disable the workaround (C1);
- C3, accepted (David, 2026-11-12; documented in `222_sandboxing.dox.tmpl`): DataProvider declares `THREAD_CONTROL` for its own `background` threads, but its default
  `*Async()` methods and `DataProviderPipeline` run the caller's code (overrides, callbacks) on them; the same holds
  for overridable methods that other declared modules run on their threads (`HttpServerUtil`'s
  `StreamResponseProducer::run()`, the `eventLoop()` methods of the OpenAI and Anthropic stream providers,
  ConnectDataProvider's `streamLoop()`/`datagramLoop()`, `McpClient::handleServerRequest()`, and the callback that
  `NatsUtil::NatsClient::subscribe()` runs for each message on a thread of its own: `callbackLoop()`, started with
  `background` in module code), and a
  `SandboxManager` does not stop it, as `background` in module code is checked against the module's options;
- C5, kept as designed and documented (David, 2026-11-12): a module that module code loads at run time (`load_module()`, the on-demand factory and type
  loading of DataProvider) is loaded with the restrictions that the loading module's Program has, which a
  `domains` declaration relaxes, not with those of the restricted caller; the explicit "load everything" APIs
  (`ProviderIndexUtil::tryLoadModule()`, `DataProvider::registerKnownFactories*()`) declare `PROCESS`, but the root
  fix is in the core (add the caller's restrictions to a run-time module load made for it);
- the core `AsyncIoController::setLogger()`, `setAutostop()` and `cancelByOwner()` act on the process-wide
  singleton but are only `dom=NETWORK` (`set_async_io_logger()` is `PROCESS`); the AsyncSocketIo wrappers
  `setLogger()` and `setAutostop()` declare `PROCESS`, and the builtins should too (follow-up);
- **leave undeclared** (refused to restricted loaders) a module that is not meant for them: QLS, QoreRepl,
  DebugUtil, DebugCmdLine, DebugProgramControl, VscDebugAdapter, the servers HttpServer, HttpServerAsyncIo and MllpUtil
  (its listener) with the handlers that require them (A2aServerHandler, GraphQLHandler, McpServerHandler,
  ServerSentEventHandler, WebSocketHandler, DebugHandler, DebugMcpServer), UrlDownloader (its public `cancel()` cancels the thread running a download with `cancel_thread()`),
  and QoreMLUtils, whose searches reseed the process-wide random number generator with `srand()`.

The audit loads every qlib module, with its own header removed, in a Program restricted with each of the two
options, in a process of its own (so that no earlier load hides a use), and lists the uses in the module's own files.

Follow-up (per module, needs approval): modules with `%ifdef PO_NO_PROCESS_CONTROL` / `PO_NO_THREAD_CONTROL`
workarounds (e.g. `WebSocketClient`) can declare the domains instead.

## Performance

Release callgrind, `calls.q` (20000 iterations of direct, typed and untyped method, static, call reference, method
reference and `call_function()` calls, plus `map` with a call), develop c160ce9369 vs this branch, min of 3/5
alternating runs:

| mode | develop | branch | delta |
|---|---|---|---|
| ast | 2,425,272,617 | 2,426,583,019 | +0.054% |
| ir | 2,234,907,734 | 2,217,315,791 | -0.79% |

The AST delta is `__strcmp_avx2` (+1.27M at identical call counts and callers: string/code layout); the changed code
nets ~0 (`CodeEvaluationHelper` marker +2.1M, `execValue()` +0.76M, `findVariant()` +0.5M against
`runtimeFindVariant()` -1.8M, `getParseOptions()` -1.6M).

## Review findings (independent review, 2026-11-11)

Fixed: marker leak to later calls (take always clears; argument evaluation hides it), `get_call_reference()` static
reference without creator, creator capture now uses the effective options, the per-call TLS cost.  Open decisions:
the qlib modules with `%ifdef PO_NO_PROCESS_CONTROL` workarounds that now declare `domains` (their workarounds are
inactive when loaded from the module path), and run-time-resolved calls of a trusted module's own declared code for a
sandboxed caller (refused: sandbox restrictions are composed into module code at run time).
