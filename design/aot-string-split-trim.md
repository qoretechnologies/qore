# AOT Calls of `<string>::split()` and `trim()`

AOT code calls two string builtins without the argument list, variant search and default arguments of the ordinary
builtin call: the canonical `<string>::split(string separator)` variant (with its `with_separator` and
`keep_trailing_empty` defaults, both `False`) and the canonical `trim(string str)` variant (with its `chars` default,
`NOTHING`).  Text processing calls them once per line or field (`trim(tolower(line)).split(" ")`), where the
ordinary call's argument list, default argument copy and call setup cost more than the work of the builtin.

## Compile-time admission

`QoreIRToLLVM` admits a call in AOT mode only (`qore_ir_is_aot_string_split_call()`,
`qore_ir_is_aot_string_trim_call()`):

- `split`: a pseudo-method call (`DotEvalMethodDirect` or, in a `try` block, `InvokeDotEvalMethodDirect`) with one
  argument whose receiver and argument are declared as `string` or `*string`, resolved to a builtin method named
  `split` of the `<string>` pseudo-class, which has no Program or namespace.
- `trim`: a `CallDirect` of the `Qore` builtin function `trim` with one argument.
- The variant is recognized by its signature (`string, bool = False, bool = False` and `string, *string = NOTHING`),
  its flags and its functional domain (`QDOM_DEFAULT`), so a change of the builtin's declaration ends the
  specialization rather than changing what the call does.
- A call with a reference argument, explicit type arguments or arguments named at run time is made as before.
- `QORE_DISABLE_AOT_STRING_SPLIT` and `QORE_DISABLE_AOT_STRING_TRIM` turn the admission off when `qcc` runs (the
  controls of the benchmarks and of `AOTStringSplitTrim.qtest`).

The code calls `qore_rt_pseudo_string_split_guarded_aot()` or `qore_rt_string_trim_guarded_aot()` with the call's
slot, the operands (borrowed) and the cleanup slots of its temporary arguments, which it clears after the call.  The
exception, result and local variable handling of the call are those of the ordinary call, which the runtime function
can make.

## Runtime functions

The functions are in `lib/QoreAOTStringRuntime.cpp`, compiled outside the single compilation unit so that they do not
change how the compiler inlines the call helpers that they share with the other calls.

A string operand (a string node, an inline short string, or an enum member whose value is a string) is handled by
the function; any other value, a call while a Program has a debugger or with `QORE_AOT_DISABLE_PRELINKED_CALLS` set
(`qore_aot_direct_call_mode`, whose pending check of the variable the first call makes, as the ordinary direct call
does), a `trim()` target that is not a builtin variant, and a `trim()` call whose Program is not the thread's current
Program take the ordinary call (`qore_rt_dot_eval_pseudo_method_direct_aot_consume_args()`,
`qore_rt_call_direct_aot_consume_args()`), which takes the argument over as the caller gave it, so their results and
errors are those of the ordinary call.

`QoreAOTBuiltinCallFrame` does what `CodeEvaluationHelper` does for such a call, in the same order: it takes the
pending call site and the call reference's mark, checks the stack, takes the caller's parse options (the first call in
a `QoreCallbackCallerHelper` scope), enters the call's Program for `trim()` (with the call Program context that
`QoreFunction::evalFunctionTmpArgs()` sets) and switches the runtime parse options to its options, sets the return type
and the variant's runtime flags, and pushes the builtin's stack location.  The builtin's code runs in its
`CodeContextHelper` and `qore_eval_builtin_call()`, as in the ordinary call; an exception has the builtin's frame
(`<string>::split` or `trim`, type `builtin`) at the top of its call stack.

The work is that of the builtin's body: `split_intern()` with the separator converted to the receiver's encoding if
they differ (`ENCODING-CONVERSION-ERROR`; a stateful encoding is split in UTF-8), and a copy trimmed by
`QoreString::trim(xsink, nullptr)`, which decodes the characters, so text that is not valid in its encoding raises
`INVALID-ENCODING`.

## AOT identity

The change of the generated code changes the AOT runtime identity through `QoreIRToLLVM.cpp` and `JITRuntime.h`,
which are fingerprinted; `QoreAOTStringRuntime.cpp` emits nothing and is in `_qcc_format_excluded_sources`.  No
format or feature version changes: the runtime functions are exported from libqore and resolved as the other runtime
functions are.
