# The cost of creating and destroying objects

Creating an object and destroying it runs the general call path for each constructor and destructor in its class
hierarchy (`CodeEvaluationHelper`: the variant search, the argument checks, the Program context, the call frame), as
for any call.  For classes that need little of it, the shortcuts below do only what the general path does for them
(qore#7336).  Each shortcut is taken only when everything it leaves out cannot apply; anything else takes the general
path.

The environment variable `QORE_DISABLE_DIRECT_OBJECT_CALLS` (testing only; read once when the library is loaded; see
`qore_direct_object_calls_disabled()` and the environment variables page of the language documentation) turns all of
them off.  It can only turn optimizations off, as the shortcuts do what the general path does; it lets the tests
compare the two
(`examples/test/qore/classes/object-construction-shortcuts/`: every script runs in every execution mode, on the IR
tier and compiled with qcc, with and without the shortcuts, and must give the output of the general path).

## Members without initializers

`qore_class_private::initMembers()` sets the object context (a `CodeContextHelperBase` and `self`) and initializes
each member of the member initialization list with `initMember()`, which evaluates the member's initializer, if any.
When no member of the list has an initializer, no code runs and no value is assigned: the members are created without
a value in their storage (the object's hash, or the `private:internal` hash of their class), as `initMember()` creates
a member without an initializer, without the object context.  Whether the list has an initializer is cached with the
code level of its initializers (`getMemberInitState()`, `memberInitIsPlain()`), computed when the first object of the
class is created and computed again when the list changes.

## Builtin constructors without arguments and builtin destructors

`ConstructorMethodFunction::evalConstructor()` and `DestructorMethodFunction::evalDestructor()` call a builtin
constructor without arguments (the constructor of a builtin class created without arguments, and the implicit
constructor of a builtin base class) and a builtin destructor without a `CodeEvaluationHelper`
(`qore_eval_direct_builtin_constructor()`, `QoreDirectBuiltinCall` in `lib/QoreClass.cpp`).  `QoreDirectBuiltinCall`
makes what `CodeEvaluationHelper` makes for such a call, in the same order, and restores it in the same order: it
takes the call site (a destructor hides it), takes the pending call of a call reference, applies the object's
creation context (sandbox provenance) and checks it for tenant isolation, checks the stack, takes the pending caller
parse options, sets the receiver type, the return type and the runtime flags of the variant, and pushes the variant's
call stack frame (the same name, call type, location, statement and Program as the frame of `CodeEvaluationHelper`),
so that exception locations, `get_thread_call_stack()` and every check that reads this state see the same thing.

What the variant search and `CodeEvaluationHelper` check at run time for such a call, and how the shortcut keeps it:

|!Check|!Where (general path)|!Shortcut
|the object is not deleted|`CodeEvaluationHelper::CodeEvaluationHelper()`|a deleted object takes the general path, which raises the error
|the variant: the only one, no inherited variants; the variant resolved at parse time, if any, is that one|`QoreFunction::runtimeFindVariantImpl()`|`hasSingleCallVariant()` and the parse-time variant are checked; otherwise the general path
|functional domains of a variant resolved at run time|`runtimeFindVariantImpl()`|checked with the same function (`qore_get_variant_denied_domains()`) against the same runtime parse options; a denied variant takes the general path, which raises `INVALID-FUNCTION-ACCESS` (keep these checks in sync)
|uncontrolled API domain against the sandbox|`check_sandbox_uncontrolled_api()`|a variant with `QDOM_UNCONTROLLED_API` takes the general path
|`QCF_NOOP` and `QCF_RUNTIME_NOOP` variants skipped under strict arguments|`runtimeFindVariantImpl()`|such a variant takes the general path
|variant access (private/internal)|`CodeEvaluationHelper::findVariantImpl()`|only a public variant
|argument conversion, default arguments, excess arguments|`prepareCallArgumentsImpl()`, `processDefaultArgs()`|only a constructor without parameters called without arguments; no destructor has parameters
|type parameters and a generic receiver|`prepareCallArgumentsImpl()`, `setObjectReceiverTypeInfo()`|a variant with type parameters and an object with an instantiated type take the general path
|the Program context of the class (module calls, parse options, Program entry)|`qore_get_method_pgm_context()`, `CodeEvaluationHelper::initImpl()`|a class with a Program context takes the general path (a builtin class has none)
|`QCF_NO_DOMAIN_THROW`, verified in debug builds|`~CodeEvaluationHelper()`|such a variant takes the general path
|sandbox isolation of the object's creation context, the stack, the call frame, the runtime flags|`CodeEvaluationHelper::initImpl()`|`QoreDirectBuiltinCall`

The builtin variant itself then runs as it does with a `CodeEvaluationHelper`: `BuiltinConstructorValueVariant::
evalDirectConstructor()` sets the code context, runs the constructors of the builtin class's own base classes and calls
the native constructor without arguments; a builtin destructor variant's `evalDestructor()` is called as is.

## The thread's data

The call state is held in the thread's data (`ThreadData` in `lib/thread.cpp`), and each helper that reads or writes
it looks it up (`thread_data.get()`, a call through the thread-local storage).  `rc_get_current_ref()`, which
`qore_rt_new_object_nb()` and the deletion of an object call, looks it up once for all the fields that it copies
(`rc_set_current_from_thread()`), and `QoreDirectBuiltinCall` looks it up once for the whole call and uses the
overloads of the helpers that take it (`take_runtime_call_location(void*)`, `update_get_runtime_stack_builtin_location
(void*, ...)` and the others with a `thread_data_ptr` parameter, from `get_thread_data_for_native_calls()`); each helper
without it calls its overload with the thread's data.

## User constructors

`ConstructorMethodFunction::evalConstructor()` calls a user constructor without a `CodeEvaluationHelper` and without
a `UserVariantExecHelper` (`qore_eval_direct_user_constructor()`, `QoreDirectUserCall`,
`UserConstructorVariant::evalDirectConstructor()`) when its variant is known (resolved at parse time, or the only
public variant for a call without arguments, such as the implicit constructor call of a base class) and its arguments
need no conversion.  `QoreDirectUserCall` makes what `CodeEvaluationHelper` makes for the call, in the same order and
with the same helpers, and restores it in the same order: as for a builtin call (call site, call reference, sandbox
provenance and isolation, stack, caller parse options, receiver and return types, runtime flags, a `CT_USER` frame),
and the Program context of the class: the call Program context (`get_set_program_call_context()`), the entry of the
Program (`ProgramThreadCountContextHelper::set()`) and its parse options (`swap_runtime_parse_options()`); the frame
shows the caller's Program.  `UserConstructorVariant::evalDirectConstructor()` then makes what `UserVariantExecHelper`
makes (the background-entry check `qore_check_background_direct_entry()`, the entry of the Program that the call runs
in, the frame boundary, the parameters) and what `UserConstructorVariant::evalConstructorImpl()` makes (the code
context with `CodeContextHelper`, `self`, the base class constructors) and runs the body with
`UserVariantBase::evalIntern()`, as the general path does: execution tiers, debugger events, the gate and
cancellation are those of the general path.

Mirrored sequences (keep them in sync): `QoreDirectUserCall::enter()` and `~QoreDirectUserCall()` with
`CodeEvaluationHelper::CodeEvaluationHelper()`, `CodeEvaluationHelper::initImpl()` and
`CodeEvaluationHelper::~CodeEvaluationHelper()` (`lib/Function.cpp`); `UserConstructorVariant::evalDirectConstructor()`
with `UserVariantExecHelper` (`include/qore/intern/Function.h`), `UserVariantBase::setupCall()` and
`UserConstructorVariant::evalConstructorImpl()`.  The rules themselves (the call Program context of a class's code,
Program entry, parse options) are in the helpers that both paths call.

|!Check|!Where (general path)|!Shortcut
|the object is not deleted|`CodeEvaluationHelper::CodeEvaluationHelper()`|a deleted object takes the general path
|the variant search and its access check|`runtimeFindVariantImpl()`, `CodeEvaluationHelper::findVariantImpl()`|the variant resolved at parse time; without one, only a call without arguments of the only public variant with no inherited variants
|functional domains declared on the variant, checked against the caller|`runtimeFindVariantImpl()`, `UserVariantBase::evalIntern()`|a variant that declares one takes the general path (`isStaticallyFastCallEligible()`)
|synchronized variants, reads of the implicit arguments, type parameters, default arguments, no-narrow container parameters|`UserVariantBase`|take the general path (`isStaticallyFastCallEligible()`)
|argument conversion and the retry of the variant search when an argument does not match|`prepareCallArgumentsImpl()`, `prepareDefaultArgsImpl()`|only arguments that need no evaluation, one for each parameter, no hash or list (`setupCall()` filters them), each accepted by its type without a filter (`acceptsRuntimeValueWithoutFilter()`, the test of `prepareDefaultArgsImpl()`); the arguments of a named call take the general path
|a generic receiver|`setObjectReceiverTypeInfo()`|takes the general path
|a call that enters a module's Program from another Program (the module call's parse options override)|`CodeEvaluationHelper::initImpl()`|takes the general path
|`QCF_NO_DOMAIN_THROW`, verified in debug builds|`~CodeEvaluationHelper()`|takes the general path
|the background-entry check, the Program entry and the frame boundary of the call|`UserVariantExecHelper`|`evalDirectConstructor()` with the same functions
|lexical semantic policies of the variant: lexical parameter policies (`LexicalUserVariant::prepareCallArguments()`) and base class constructors invoked with the declaration's policy (`LexicalBaseConstructorVariant::evalConstructor()`)|`include/qore/intern/Function.h`, `QoreClassIntern.h`|such variants take the general path; a call made with a lexical invocation policy is not made here either
