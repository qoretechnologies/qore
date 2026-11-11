# Lexical compatibility options (#5831)

Status: revised design for approval; no implementation in this branch. Inventory baseline: `c160ce9369`.
Scope decision: legacy code remains AST-only; the `PO_MODERN` native eligibility contract is unchanged.
Implementation precondition: [#5853](https://github.com/qoretechnologies/qore/issues/5853) must enforce that
contract for mixed-directive bodies before #5831 implementation.
Copyright (C) 2026 Qore Technologies, s.r.o.

A compatibility option belongs to the source operation that was parsed with it. Changing a Program's options,
parsing another pending source, or executing shared code in another Program must not change that operation.
The proposed implementation makes semantic policy immutable and explicit in legacy AST operations and their
shared runtime helpers. Modern compiled code always has canonical default compatibility semantics. Program
restrictions and sandbox/module authority remain dynamic. Ordinary calls must acquire no new
compatibility check, TLS save/restore, allocation, or argument on their execution path.

## Existing state and the failure

`AbstractStatement::pwo` already contains the full 128-bit `QoreParseOptions` snapshot. Its constructors capture
it, `AbstractStatement::parseInit()` restores it with `ParseWarnHelper`, and `AbstractStatement::exec()` passes it
to `RuntimeConfigLocationHelper`. `UserVariantBase::getParseOptions()` obtains the function body's snapshot;
source-stripped AOT obtains it from `aot_entry_statement` instead.

This is not yet a reliable runtime semantic context:

- `runtime_get_parse_options()` returns `ThreadData::runtime_po`, not an immutable code property. Statement and
  call helpers change it; `apply_runtime_po_override()` also combines module/caller restrictions.
- `CodeEvaluationHelper::init()` (`lib/Function.cpp:1222,1343`) replaces the runtime/RuntimeConfig options with
  the executing Program's current options. Arguments, overload selection, default arguments and the body have
  distinct contexts, so merely preserving the caller's entire option word would also be wrong.
- Several consumers bypass RuntimeConfig and read the Program directly. Others use
  `rc.getParseOptions() ? rc.getParseOptions() : runtime_get_parse_options()`: a zero lexical snapshot is valid,
  not a request to substitute a mutable Program's options.
- `QoreIRFunction`/ordinary instructions have no general lexical semantic policy. Native execution updates
  locations without necessarily updating AST statements/options. An AST statement pointer or source location
  lookup is consequently not a sufficient source of an inherited legacy context at a compiled-call boundary.
  No legacy policy is stored in compiled code: compiled operations always select default semantics.
- Cross-Program/module overrides (`qore_set_user_call_runtime_po_override*`, `Function.cpp`, `thread.cpp`) exist
  to implement authority and ownership rules. They must not supply compatibility semantics or be weakened to
  preserve a legacy option.

## Runtime-read inventory

The audit searched `runtime_get_parse_options`, `runtime_check_parse_option`, `getParseOptions`, every
`PO_BROKEN_*` use, and the extended compatibility bits in `lib/` and `include/`; the same compatibility-read
search in `modules/` found no additional direct reader. Calls to the central conversion APIs below are
transitive consumers even when they do not mention an option. Line numbers refer to the inventory baseline.

### Compatibility reads and their shared native counterparts

The legacy AST/shared-helper paths bind lexical policy. A modern compiled counterpart uses canonical default
semantics, even if it currently reaches the same mutable reader. The native line references below are an
inventory of those shared boundaries, not a proposal to carry legacy policy in compiled code.

- **Broken range**: `lib/ql_list.qpp:71` (`range_intern`, used by `range()`),
  `include/qore/intern/RangeIterator.h:67`, `lib/QoreRangeOperatorNode.cpp:154`, and
  `lib/QoreSquareBracketsRangeOperatorNode.cpp:313,325`. Preserve the separate builtin/operator endpoint
  conventions; capture policy before constructing an iterator and retain it for deferred iteration.
- **Broken list range**: `lib/QoreSquareBracketsOperatorNode.cpp:83,847`;
  `lib/QoreSquareBracketsRangeOperatorNode.cpp:191,313,353` (the associated tests of the bit occur at 192,314,362);
  `lib/Variable.cpp:3746,4134`; and `lib/JITRuntime.cpp:2488,2538,2689,15292,15442`.
  These cover value slicing, functional slicing, removal and lvalue path helpers. A policy passed only to the
  top-level read operator would miss mutation and nested selectors.
- **Strict boolean evaluation**: `lib/CallReferenceNode.cpp:358`, `QoreHashNode.cpp:1083`,
  `QoreStringNode.cpp:163`, `QoreBufferNode.cpp:2259`, `QoreObject.cpp:2862`, `QoreClosureNode.cpp:145`,
  `QoreListNode.cpp:1416`, and `BinaryNode.cpp:265`. Their `getAsBool()` implementations are reached by
  conditions, logical operators, casts/soft conversion, and native code using the value API. Bind the
  conversion operation's policy, never the value's allocation Program or lifetime.
- **Broken sprintf**: `lib/QoreLib.cpp:1044,1045` (`qore_sprintf_intern`) reads the Program directly. Audit all
  callers of this shared formatter, including formatting builtins and native callers. Preserve the existing
  explicit `ignore_broken_sprintf` behavior; a policy does not override that API choice.
- **Broken namespace resolution**: `include/qore/intern/QoreNamespaceIntern.h:3203` is the direct Program read.
  Its callers at 1692,1721,1748,1773,2100,2268,2293 resolve classes, hashdecls, enums, functions, globals and
  constants. Pass the lookup operation's policy through dynamic/reflection entry points; the target Program
  still determines which namespace is searched. The parse helper at 3199 is a separate parse-time read.
- **Broken loop statement**: `lib/SwitchStatement.cpp:210,253` reads the Program while interpreting switch
  control flow. Bind the AST switch's policy. Legacy loop semantics never enter IR/LLVM lowering under the
  unchanged native eligibility gate.
- **Broken soft types**: `lib/QoreTypeInfo.cpp:4446`, `QoreTypeInfo::acceptsBrokenSoftNull()`. Trace both
  acceptance/overload selection and actual filtering/coercion, including nested containers. Shared global
  `QoreTypeInfo` objects must remain policy-independent. Pass a conversion policy to the operation or choose
  its filter at binding time; do not mutate a type singleton.

### Already captured, adjacent semantics, and excluded reads

`NO_STRING_INDEX_CHAR` is already captured in `QoreSquareBracketsOperatorNode::parseInitImpl()` (line 94),
exposed by `hasStringIndexChar()`, and lowered into constants/collection-operation metadata
(`QoreIRLowering.cpp:10624,11083`; `QoreIRToLLVM.cpp:34193,34242,34912`;
`QoreAOTBinary.cpp:7488,7730,9128`). Keep this explicit carriage and test it across source/AOT reconstruction;
do not replace it with an ambient policy read. Its existing native representation is unchanged; it needs no
new general compiled compatibility policy.

`PO_NEGATIVE_OFFSETS` is not in the deprecated compatibility mask, but is already captured alongside string
indexing by the AST operator and still read dynamically elsewhere. The remaining reads are:

- `lib/Variable.cpp:2314,2561,4135`;
- `lib/QoreIRInterpreter.cpp:10781,14825,19637`;
- `lib/JITRuntime.cpp:2431,2723,2888,7427,7519,7609,7657,15245,15443`.

These adjacent reads are recorded for completeness and remain outside this compatibility-only change.
`PO_NEGATIVE_OFFSETS` can affect modern code; changing its binding would require separate native-policy scope.
Preserve existing behavior and the AST operator's existing capture; do not freeze these remaining reads here.

`PO_STRICT_ARGS` / `PO_REQUIRE_TYPES` also affect runtime semantics, although they are not deprecated
compatibility flags. Record these separately so security/domain checks are not accidentally converted:

- Invalid-target `push`/`pop`/`shift`: `QorePushOperatorNode.cpp:65`, `QorePopOperatorNode.cpp:51`,
  `QoreShiftOperatorNode.cpp:117`; IR paths at `QoreIRInterpreter.cpp:15610,15626,15846`;
  native paths at `JITRuntime.cpp:15898,15911,16056`.
- Extra/default arguments and no-op variant eligibility: `Function.cpp:1608,1610,3207,3280,3596,3793`, the
  corresponding `variant->getParseOptions()` uses, and `Function.h:2136` (`getUniqueReturnTypeInfo`). User
  variants already consult their body snapshot in several of these paths; builtins fall back to runtime
  options. Preserve that distinction while separating semantic filtering from domain authority.
- Local/parameter initialization passes option words through `UserVariantBase`, `LocalVar::instantiate`,
  interpreter execution and native parameter/handler helpers (`JITRuntime.cpp:11599,13411,17561,19421`).
  Inspect the receiving initialization/filter operation rather than treating every option-word transfer as
  a new compatibility reader.

Do not expand #5831 to redefine strict-argument/type-requirement semantics. Preserve existing declaration
snapshots and modern defaults. The affected compatibility conversions (broken soft types and strict boolean
evaluation) still need explicit policy during parameter/default filtering, including at mixed call boundaries.
That is distinct from changing these adjacent strictness options or admitting legacy code to compiled modes.

The other deprecated bits currently act during parsing: broken list parsing, logic precedence, integer
assignments, operators, narrowed types, references, cast, auto cast, and varargs. Relevant sites include
`scanner.lpp`/`parser.ypp`, `QoreAssignmentOperatorNode::parseInitImpl`,
`Var::parseGetTypeInfo` (`Variable.cpp:388`), `LocalVar::parseGetTypeInfo` (`LocalVar.h:1023`),
`QoreCastOperatorNode::parseInitImpl` (broken cast/auto cast), `QoreTypeInfo.cpp` parse-type resolution, and
`MethodVariantBase` construction (`QoreClassIntern.h:248`). Do not add runtime guards for them. Preserve their
already-resolved AST meaning, including narrowed types and `QCF_USES_EXTRA_ARGS`; modern native lowering
remains unchanged.

Dynamic security/authority reads remain dynamic: filesystem/terminal/network/module restrictions, functional
domains, module ownership/caller restrictions, sandbox policies, thread permissions, and external-info
redaction. Program/ProgramControl/get_parse_options introspection is not itself a semantic consumer; this
proposal does not silently redefine those APIs. Plugin floating-point reassociation acceptance is a separate
compiler policy, not a deprecated compatibility option.

## Binding rules proposed for approval

1. An expression, builtin invocation, lookup, conversion or control-flow operation uses its own lexical
   snapshot. This includes a builtin reached through a call reference or dynamic lookup: its invocation site
   supplies policy, not the site that happened to create the builtin reference.
2. A user function/method/closure retains its own body policy when called, shared or imported into another
   Program. Its default expressions, parameter binding and return coercion use declaration policy. A builtin
   has no Qore declaration source, so its argument filtering uses invocation policy.
3. Explicit argument expressions execute with the caller's policies before callee binding. Each default
   expression retains its own snapshot. Legacy bodies/defaults are interpreted, never inlined into modern
   compiled code. Modern compiled defaults and operations have canonical default compatibility semantics.
4. Values and shared type objects do not acquire compatibility flags. Assignment/container mutation uses the
   source operation's conversion policy, not the Program that allocated the target value. Initializers retain
   their declaration's operation policy. Namespace contents and authority still come from the target Program.
5. Zero is a complete policy. Any distinction between an absent AST/code-owned descriptor and a zero policy
   is explicit; zero must never mean "read the current Program". No new AOT presence contract is required,
   because the native gate excludes legacy policy from compiled artifacts.

These rules need approval, particularly builtin references versus user references and declaration-time
parameter/return conversion. Tests must state those decisions directly rather than infer them from current
mutable-Program behavior.

## Representation and execution

Define one internal `LexicalSemanticPolicy` projection of the relevant low and high option bits, with named
fields/queries and a canonical default. Retain the original 128-bit snapshot for diagnostics and code metadata;
project it once at parse/bind time. The compatibility projection excludes all authority bits.

Capture operation policy in `parseInit` while `ParseWarnHelper` has restored the statement's snapshot. Standalone
expressions, defaults, member/global initializers, closure bodies and deferred handlers need explicit owners
and capture points too. Store immutable policy with the executable operation or a code-owned descriptor whose
lifetime follows that code, including code whose defining Program has been released. Never key it solely by a
raw Program address or mutable Program option word.

Use separate default and compatibility execution entries, selected during AST parsing or cold call binding:

- Default AST call nodes and normal IR/native call helpers retain their ordinary entry. AST call
  nodes/descriptors with non-default compatibility policy select the alternate entry; do not add `if (compat)` to every ordinary call. Dispatch
  through an existing virtual/function-pointer selection may choose a different entry for a compatibility
  node without adding a second dispatch to default nodes. No policy-bearing IR opcode or native function
  prologue is added.
- Operations consume explicit policy bits or preselected helpers. Boolean/range/slice/mutation/type-filter
  default helpers must have constant default semantics. Compatibility helpers receive their captured policy.
  A normal user function entered from legacy code must never inherit that caller's semantic policy.
- Dynamic call caches bind a native invocation with its call site's immutable policy; cache identity must
  distinguish policies whenever an entry can be shared. Default call sites retain their existing cache path.
  A selected user variant uses its own declaration/body policy.
- Overload selection and argument/default filtering occur **before** the existing native flags store
  (`Function.cpp:1259-1333`, versus 1367-1369). They must not read that ambient flags state. Bind candidate
  parameter/conversion filter descriptors to each user declaration's policy; builtin candidates use the
  invocation site's policy. Pass those policies explicitly through acceptance and coercion, including soft
  NULL, soft-bool and recursive container filtering. Default-expression evaluation uses that expression's
  snapshot, independently of the candidate filter. A failed candidate/retry restores original arguments as
  today, without leaving a semantic context behind. The default resolver/filter entry retains constant
  default semantics; a function/call site needing compatibility selection binds a separate resolver entry
  through existing cold binding/cache invalidation. The late native-body flags store is not a solution for
  this earlier phase, nor proof of zero default-call overhead. Test legacy native -> modern callback argument
  filtering before the callback body starts, including a rejected soft-NULL/soft-bool argument.
- Native C++ value/format/type APIs are part of the inventory, not an escape hatch. Builtins and native module
  calls need their invocation policy available through RuntimeConfig, including callbacks and reentrancy.
  Prefer carrying a compact internal policy encoding in the runtime flags state already saved/set/restored
  by `CodeEvaluationHelper`, using separate internal raw-state accessors so public code-flag APIs retain their
  existing meaning. Bind the encoded native entry state before execution; default entry state is the existing
  flags value with a zero semantic projection. The existing store then clears an outer compatibility state
  when a default native call is made, without a new per-call store or branch. Reserve/disjoint-check the internal
  encoding against public QCF bits; do not expose internal bits through reflection or public flag getters.
- Native fast helpers that bypass CodeEvaluationHelper must use explicit policy/default-specialized operations;
  they must not accidentally consume an outer native call's RuntimeConfig policy. The same applies to direct
  user callbacks. A native call with no Qore lexical source uses the canonical default policy. An internal
  API may explicitly pass a captured code-owned policy when invoking on behalf of a Qore operation; absence
  of that descriptor never means inherit ambient compatibility state or read the destination Program's
  mutable compatibility bits. Public native callers retain default semantics without a new required argument.

The native bridge is a design constraint requiring implementation verification: if it cannot preserve the
public API and callbacks without adding work to default calls, stop and revise this design. A global TLS
compatibility guard on every function entry is not an acceptable fallback. An override around legacy calls
alone is also insufficient: a modern callback would otherwise inherit it.

No mutable global compatibility state, per-call allocations, or new locks are needed. Descriptors are immutable
before publication, owned by code, and restored native context is scoped through exceptions, thread exit and
cancellation. All slow-path context installation must use existing exception-safe ownership/context machinery.

## Modern compiled boundaries and the unchanged native gate

`QORE_IR_REQUIRED_PARSE_OPTIONS == PO_MODERN` remains the native eligibility contract. The current saved-variant
check is incomplete: `UserVariantBase::getParseOptions()` reads the body block's snapshot, which the parser
establishes from its first statement. A body beginning under `%modern` can later use `%loose-args` and
`%broken-range` and still be lowered by explicit IR. The review reproduced this with the available debug build; the inventory-baseline source at
`Function.cpp:5770` and `parser.ypp:3442-3447` explain the gap.

[#5853](https://github.com/qoretechnologies/qore/issues/5853) is a separate implementation precondition. It must
check at parse time that every statement/expression was parsed with `PO_MODERN` and no compatibility bits;
otherwise tiered uses AST and explicit IR/JIT/qcc refuse the body, naming the directive and line. It covers
nested blocks, closures, methods and AOT, with zero runtime overhead. This enforces the existing contract;
it does not admit legacy semantics to native code. Its branch follows #5836 and precedes #5831 implementation.
No gate changes are part of this design branch or the #5831 implementation.

After that precondition, legacy non-modern bodies, defaults and handlers execute only through AST. Modern
Programs cannot acquire deprecated compatibility bits. A modern caller can invoke a legacy user variant
through existing runtime dispatch; this never makes the legacy body eligible for IR/JIT/AOT or inlining.

With #5853 in place, no compiled operation needs a captured legacy policy. Boundary dispatch still needs the policies of the AST
code it invokes:

- A modern IR/JIT/AOT caller calling a legacy user variant selects that variant's declaration-bound parameter
  filters and AST defaults/body. The variant already owns its source/statement snapshots; cold binding selects
  the compatibility resolver/entry. No caller IR field, native argument or image metadata carries that policy.
- Legacy AST calling a modern user variant evaluates explicit arguments under the AST caller's policies, then
  uses the modern declaration's default matching/filtering and body semantics. Defaults use their own modern
  declaration semantics. This includes a callback made before a native builtin's legacy context is unwound.
- A native builtin invoked by legacy AST has no Qore declaration policy. Its invocation-site descriptor supplies
  the legacy policy to candidate matching/filtering and shared value/format/lookup APIs. This is runtime context
  for a C++ builtin, not legacy Qore code compiled by JIT or qcc.
- Modern compiled native helpers and fast callbacks must select default semantic operations explicitly. They
  must not consume an enclosing legacy builtin's ambient compatibility context. Existing helper implementations
  may need default-specialized entry points; that does not add policy carriage to IR/LLVM, and it must not add
  work to ordinary calls. Native public value APIs reached from a legacy builtin still use that builtin's
  scoped invocation context. Validate the full legacy builtin -> modern callback -> native helper chain.
- Shared/imported legacy user code retains the defining AST descriptors across Program mutation, sharing and
  destruction. Its target Program still supplies namespace contents and authority. Compiled modern libraries
  retain default compatibility semantics whichever Program invokes them.

The following proposals from the earlier note are withdrawn: per-operation IR policy references/instruction
forms, policy pools in compiled functions, policy-aware LLVM lowering/inlining/CSE, JIT/native artifact policy
cache keys, a new AOT format version or feature bit, and rejection/recompilation of old images for missing
lexical policy. There is no native eligibility expansion and no new AOT serialization layout or feature.
Existing string-index metadata and signature/body parse-option records remain unchanged.

AOT format/feature impact: **none**. Runtime AOT identity impact: **implementation-dependent and must be
reported**. Qore's identity hashes runtime/helper sources as well as the format; changes to shared runtime
helpers or their contracts can change the identity without a wire-layout change. Do not promise old native
binaries remain identity-compatible, and do not add a separate old-image rejection rule for lexical policy.

## Acceptance and performance evidence

For each in-scope runtime compatibility option, test original AST behavior, Program mutation after parsing,
two pending sources with opposing directives, and AST code shared between Programs with opposing options.
Cover modern callers of legacy user code and legacy callers of modern code, closures/method references,
builtin references invoked from both policies, explicit/default arguments, overload/parameter/return filters,
initializers, deferred iterators, background execution, handlers and debugger attachment. A real native
module/value-API callback fixture must verify the native bridge and reentrancy; a Qore-only wrapper is
insufficient for that boundary.

Test setup must create actual legacy child code: a `%modern` parent can cause `Program(PO_NEW_STYLE)` to
inherit modern restrictions. Use the established `%no-child-restrictions` test setup or explicit isolated
parse contexts, and assert the child's effective parse options before the semantic checks.

Run modern callers in AST, explicit IR, JIT, promoted tiered and qcc (including stripped AOT), with legacy
callee/default/body execution explicitly remaining AST. Test the existing rejection/fallback contract when
native execution is requested for a legacy body. Do not claim the legacy code ran natively because the outer
test did. Retest module/sandbox/cross-Program authority regressions so semantic isolation cannot grant
permissions. Include concurrent shared-code execution and Program destruction/reuse; run Valgrind and, for
any new shared publication mechanism, ThreadSanitizer.

Before implementation, retain Release callgrind baselines for plain direct/user/builtin calls, method/closure
calls, map/filter callbacks, argument conversion, and a legacy-native-modern callback chain. Default paths must
show no added compatibility branch/TLS operation/allocation in instruction annotation and no instruction-count
regression. Compare 1,000/10,000 or similarly scaled runs to remove startup costs. Attribute every positive
call-path delta and recover it; a small aggregate percentage is not sufficient. Report compatibility-path cost
separately. Verify both IR and native/AOT generated code rather than relying on a source-level claim.

Approval requested for the revised AST binding rules; explicit policy plus separate legacy execution/filter
entries and the scoped native RuntimeConfig bridge; and the mixed modern/legacy boundary tests. Negative
offsets and adjacent strictness changes are excluded. The native eligibility contract remains unchanged; its existing mixed-body defect is fixed separately by
#5853. Implementation remains blocked on approval of this revised design and completion of that precondition.
