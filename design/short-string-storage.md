# Inline Short String Storage

**Status:** Implemented.

**Target:** Qore 3.0.

## Summary

A `QoreValue` holding a string of `QoreValue::SHORTSTR_MAX_BYTES` (6) bytes or fewer stores the
bytes inline in the 64-bit value instead of pointing at a heap-allocated `QoreStringNode`. This
removes the allocation and the reference counting for the very common case of short text, and makes
equality a single `uint64` compare.

The consequence is that **a Qore string value has two runtime representations**, and every piece of
C++ code that reads a string value has to accept both.

## Runtime Representation

Inline strings use the NaN-box tag `0xFFC`:

- `(bits >> 52) == 0xFFC` identifies an inline short string (`QoreValue::isShortString()`)
- bits 51-48 hold the byte length (0-6), returned by `QoreValue::shortStringLen()`
- bits 47-0 hold the bytes, packed **big-endian** (`strBits |= byte << (40 - i * 8)`) so that a
  single `uint64` compare orders two inline strings lexicographically
  (`QoreValue::shortStringEquals()`)
- the encoding is always UTF-8; `QoreValue::makeStringValue()` only stores inline when the effective
  encoding is `QCS_UTF8`, otherwise it allocates a `QoreStringNode`

Two properties follow from the packing and matter to callers:

- the bytes are **not** contiguous in ascending memory order on a little-endian host, so no
  `const char*` can alias them; `QoreValue::getShortString(char* buf)` unpacks into a caller buffer
- there is **no NUL terminator** in the value: at the maximum length all 48 payload bits are bytes,
  so the terminator has to come from the caller's buffer (which is why it must hold 7 bytes)

## Producers

Inline strings are created by `QoreValue::makeStringValue()` / `tryMakeShortString()`, which are
called only from the execution engines and columnar storage:

- the IR interpreter (`QoreIRInterpreter.cpp`), the JIT (`JITRuntime.cpp`), and AOT constant
  deserialization (`QoreAOTExprHandlers.cpp`, `QoreAOTExprNodeHandlers.cpp`, `QoreAOTBinary.cpp`)
- columnar and dataframe storage (`QoreColumnarResult.cpp`, `QoreBufferNode.cpp`,
  `modules/dataframe`)

Ordinary C++ code that builds values with `new QoreStringNode(...)` always produces the heap
representation. This is why the representation of a given value is not predictable from the source
text: the same string literal is inline when it comes from an AOT constant and a heap node when it
comes from the legacy evaluator.

Inline values are **not** confined to the engine that produced them: they are stored in hashes,
lists, object members and lvalues like any other value, and are handed to builtin C++ code from
there.

## The Consumer Contract

`QoreValue::getType()` returns `NT_STRING` for both representations, but
`QoreValue::get<QoreStringNode>()` returns `nullptr` for the inline one, because there is no node to
point at. The historically idiomatic

```cpp
if (v.getType() == NT_STRING) {
    const char* s = v.get<const QoreStringNode>()->c_str();   // WRONG
}
```

is therefore a null dereference for any string of 6 bytes or fewer. It faults at address `0x10` —
the non-virtual base offset applied to `nullptr`, which is also `offsetof(qore_string_private, buf)`
— a distinctive signature worth recognising in a backtrace.

The supported accessors are:

| need | use |
|---|---|
| bytes, byte length, encoding | `QoreStringDataHelper` — no allocation, no conversion |
| a `const QoreStringNode*` (to take a reference, or for an API that takes one) | `QoreStringNodeValueHelper` — materializes an inline string, returns the existing node otherwise |
| a `QoreString` in a specific encoding, or stringification of non-string values | `QoreStringValueHelper` |
| raw inline bytes in a caller-owned buffer | `isShortString()` + `getShortString(char[7])` |

`QoreStringDataHelper` unpacks an inline string into a buffer it owns, so pointers returned by
`c_str()` are only valid for the lifetime of the helper; keep it in scope for as long as the pointer
is used, and copy the bytes if they must outlive it.

Two places already satisfy the contract without the helpers, and code there does not need changing:

- **builtin parameters** — `qpp` generates `QoreStringNodeValueHelper` for `string` and `*string`
  parameters, so declared string arguments always arrive as a valid `QoreStringNode`
- **lvalue mutation** — `LValueHelper::ensureUnique()` (`include/qore/intern/Variable.h`) and
  `ensure_unique(QoreValue&, ExceptionSink*)` (`lib/QoreLib.cpp`) both materialize an inline string
  into a unique heap node, so the `trim` / `chomp` / `splice` / `extract` / `+=` operator paths may
  use `get<QoreStringNode>()` after calling one of them

`QoreValue::get<T>()` asserts in debug builds when it is applied to an inline short string with a
string type parameter, so new occurrences of the unsafe idiom fail loudly rather than silently
returning `nullptr`.

## Testing

- `lib/ql_debug.cpp` — `ut_string_data_helper()` covers `QoreStringDataHelper` against both
  representations, the 6-byte boundary, the empty string, non-string values, cross-representation
  comparison, and a short string read back out of a hash; it runs from `run_debug_unit_tests()` and
  `run_unit_tests()` in release builds as well
- `test/qore/strings/short_strings.qtest` — language-level coverage that pushes inline strings
  through `join()`, the `foldl`/`foldr` string-join fast path, containers, the lvalue operators, and
  builtin string parameters

Forcing the inline representation from a script needs `dbg_make_short_string()` /
`dbg_is_short_string()`, which are compiled in only for debug builds (`#ifdef DEBUG`) and, like every
other hook registered in `init_debug_functions()`, are tagged `QDOM_PROCESS | QDOM_UNCONTROLLED_API`
so that sandboxed code cannot reach them. The test gates those cases on the `QoreDebug` parse define.
