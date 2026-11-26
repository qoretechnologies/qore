# Exceptions of the String Fast Paths

The IR interpreter and JIT and AOT code make some string builtin calls through fast paths: the builtin's work runs in
a runtime function without the call, that is without an argument list, a variant search, defaults or a frame on the
call stack.  They are the `<string>` pseudo-methods `lwr()`, `upr()`, `substr()`, `find()`, `rfind()`,
`startsWith()`, `endsWith()`, `contains()` and `length()`, the functions `tolower()`, `toupper()` and `length()`, and
the fusions that run several of these calls, or a concatenation and these calls, in one runtime function (the case
conversion consumers, measures and comparisons, and the concatenation searches, measures and `substr()` calls of
`lib/JITRuntime.cpp`).

The work of these builtins can raise an exception: `INVALID-ENCODING` for text that is not valid in its encoding, and
`ENCODING-CONVERSION-ERROR` for an argument that cannot be converted to the encoding of the string.  The AST
interpreter raises it in the builtin's frame (function `<string>::lwr`, `tolower` and so on, type `builtin`, located
at the call), which is at the top of the exception's call stack.  The work of a fast path has no frame, so its
exception would have the frame of the calling function at the top instead (qore#7711).

## The ordinary call for an exception

A fast path runs its work as before (`qore_rt_string_fast_path()`).  If the work raised an exception, the fast path
discards it and makes the ordinary call of the builtin, which raises it again in the builtin's frame, and returns the
result of that call:

- a pseudo-method is called by name on the value (`pseudo_classes_eval()`), as the AST interpreter calls one, and a
  function is called by name in the current Program (`qore_rt_builtin_function_call()`);
- a fusion makes the calls that it stands for, in their order, and stops at the first exception: a case conversion
  consumer the conversion and then the search, a concatenation search the concatenation and then the search; the
  concatenation is an operator, which has no frame, and is made as the work makes it, and so is the comparison of a
  case comparison, which the work makes again after the conversion;
- if an exception is pending when the fast path starts, the work would add its exception to it without the frame, so
  the fast path makes the ordinary call at once.

The work of these builtins has no effect but its result, so making the call after the work raised gives the
exception that the call would have raised.  The calls are in `lib/QoreAOTStringRuntime.cpp`
(`QoreStringFastPathCalls.h`), out of line and outside the single compilation unit.  A fast path that raises nothing
tests the exception sink before and after its work and makes no call, so it costs what it cost before.

The fast paths of the functions have runtime functions of their own (`qore_fast_tolower_string()`,
`qore_fast_toupper_string()`, `qore_fast_length()`), as the frame of the function is not that of the pseudo-method;
`<string>::length()` uses `qore_rt_pseudo_string_length_xsink_noguard()`.

`StringFastPathFrames.qtest` compares each exception and its whole call stack with the AST interpreter's in every
execution mode, on the IR tier and in AOT executables, also without prelinked calls.
