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

A fast path runs its work as before (`qore_rt_string_fast_path()`).  If the work raised an exception, which it detects
in a branch that it takes anyway (a status that it computes, or its own test of the sink after the operation that can
raise), the fast path discards the exception and makes the ordinary call of the builtin, which raises it again in the
builtin's frame, and returns the result of that call:

- a pseudo-method is called by name on the value (`pseudo_classes_eval()`), as the AST interpreter calls one, and a
  function is called by name in the current Program (`qore_rt_builtin_function_call()`);
- a fusion makes the calls that it stands for, in their order, and stops at the first exception: a case conversion
  consumer the conversion and then the search, a concatenation search the concatenation and then the search; the
  concatenation is an operator, which has no frame, and is made as the work makes it, and so is the comparison of a
  case comparison, which the work makes again after the conversion.

No caller makes a fast path's call with an exception pending (which the work would add its exception to, and which
the discarding would release): JIT and AOT code test the sink after every instruction that can raise and branch to the
handler (deferred exception checking is not enabled by any caller of the code generator), the IR interpreter too, and
a block-exit handler runs with the exceptions of its block set aside; a debug build asserts it.

The work of these builtins has no effect but its result, so making the call after the work raised gives the
exception that the call would have raised.  The calls are in `lib/QoreAOTStringRuntime.cpp`
(`QoreStringFastPathCalls.h`), out of line and outside the single compilation unit.  A fast path that raises nothing
makes no test that its work did not make before; `<string>::length()` and `length()` count the characters of UTF-8 text
that is all ASCII without a test of the sink.

The fast paths of the functions have runtime functions of their own (`qore_fast_tolower_string()`,
`qore_fast_toupper_string()`, `qore_fast_length()`, `qore_fast_substr_string()`, `qore_fast_index_string()`,
`qore_fast_rindex_string()`), as the frame of the function is not that of the pseudo-method; `<string>::length()` uses
`qore_rt_pseudo_string_length_xsink_noguard()`.

The lowering makes `substr()`, `index()` and `rindex()` of strings as calls of the pseudo-methods `substr()`, `find()`
and `rfind()`, whose expression is the function call (`qore_ir_is_string_function_form()`).  The code generator and
the IR interpreter give such a call the fast path of the function, and a case conversion consumer that is such a call
tells its runtime function (`QORE_STRING_CASE_CONSUMER_FUNCTION`), so its exception has the function's frame; the
summaries and the call result consumer fusions, whose runtime functions make the pseudo-methods' calls, do not take
it.  An instruction read from serialized IR (the debug IR of AOT code) has no expression and is made as the
pseudo-method (qore#7733).

## List joins

`<list>::join()` and `join()` of a list (`list.string.join`, `list.string.join.identity-map`) raise
`ENCODING-CONVERSION-ERROR` for a value that cannot be converted to the encoding of the separator.  Their fast paths
(`qore_rt_list_string_join_checked()`, `qore_rt_list_string_join_identity_map_checked()`, and for `join()`
`qore_fast_join_list()` and `qore_fast_join_list_identity_map()`) make the ordinary call when the join returns no
string, which it does only for an exception (`join_intern()`); the code generator and the IR interpreter tell the forms
apart by the expression of the instruction (`qore_ir_is_list_join_function_form()`), as above (qore#7731).

`<list>::join()` of a map is fused with the map (`string.method.join.start`): the AST interpreter evaluates the whole
map, and then the join raises for the first value that it cannot convert, while the fused loop joins each value as the
map gives it.  So the join of a value that cannot be converted gives the failed join, a list of the join of the values
before it and the value, and drops the exception; the map gives its other values, which are added to it (an exception
that it raises then is the exception, as the join never runs in the AST interpreter), and the finish of the join, after
the loop, makes the ordinary call of `<list>::join()` with the value and the values after it, which raises the exception
of that value in the frame of the join; should the call join them, the result is the join of all the values.  The join
converts each value on its own and in order, so the exception is the one that the join of all the values raises
(`qore_rt_string_method_join_start()`).  The partial join is never visible: the fused loop's accumulator is internal.

The folds of a concatenation (`foldl $1 + sep + $2`, fused or not) are operators, whose exceptions have no frame of
their own, and are evaluated lazily by the AST interpreter too, so their fast paths raise as the AST interpreter does.

`StringFastPathFrames.qtest` and `ListJoinFastPathFrames.qtest` compare each exception and its whole call stack (and
the output of the joined map) with the AST interpreter's in every execution mode, on the IR tier and in AOT
executables, also without prelinked calls.
