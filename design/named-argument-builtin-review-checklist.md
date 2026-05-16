# Named-Argument Builtin Review Checklist

## Purpose

This checklist captures the review process for enabling named arguments on
builtin functions and methods. It applies to core Qore QPP files and should also
be used when the same migration is applied to binary modules under
`~/src/qore/git/module-*`.

Named arguments make formal parameter names part of the public API. Do not add
`NAMED_ARGS` until the names have been reviewed as stable, clear, and safe for
users to rely on.

## Inventory

Build the target first so qpp metadata is current:

```bash
cmake --build build --target qore
```

For core builtin functions, list fixed-arity variants that are not yet
named-callable:

```bash
jq -r '
  def row($x): select(($x.params|length)>0)
    | select((($x.flags//[])|index("NAMED_ARGS"))|not)
    | select((($x.flags//[])|index("NOOP"))|not)
    | select((($x.flags//[])|index("RUNTIME_NOOP"))|not)
    | select((($x.flags//[])|index("DEPRECATED"))|not)
    | [input_filename, $x.name, $x.signature, (($x.flags//[])|join("+")),
       (($x.params//[])|map(.name+":"+.type_name)|join(", "))] | @tsv;
  .functions[]? as $f | row($f)
' build/ql_*.meta.json | sed 's#build/##' | sort
```

For core builtin methods, list fixed-arity variants that are not yet
named-callable:

```bash
jq -r '
  def row($c; $kind; $x): select(($x.params|length)>0)
    | select((($x.flags//[])|index("NAMED_ARGS"))|not)
    | select((($x.flags//[])|index("NOOP"))|not)
    | select((($x.flags//[])|index("RUNTIME_NOOP"))|not)
    | select((($x.flags//[])|index("DEPRECATED"))|not)
    | [input_filename, $c.name, $kind, $x.name, $x.signature,
       (($x.flags//[])|join("+")),
       (($x.params//[])|map(.name+":"+.type_name)|join(", "))] | @tsv;
  .classes[]? as $c
  | ( ($c.instance_methods[]? | row($c; "instance"; .)),
      ($c.static_methods[]? | row($c; "static"; .)) )
' build/QC_*.meta.json build/Pseudo_QC_*.meta.json | sed 's#build/##' | sort
```

For a binary module, use the same query against that module's generated
`*.meta.json` files after building the module.

## Opt-In Rules

Enable `NAMED_ARGS` only when all of these are true:

- Every non-varargs parameter has a reviewed public name.
- The selected variant can be resolved at parse time from the supplied names and
  types.
- The function or method has a fixed parameter surface; free-form positional
  forwarding remains positional.
- Error messages, documentation, and reflection will expose the reviewed names.
- Any newly touched C++ code passes the sandboxing and cooperative cancellation
  audits.

Do not enable `NAMED_ARGS` for:

- Varargs-only variants such as `print(...)`, `exists(...)`, `min(...)`, and
  `max(...)`.
- Dynamic forwarding helpers such as `call_function(name, ...)`,
  `call_object_method(object_value, method, ...)`, and `create_object(class_name,
  ...)`.
- Deprecated, `NOOP`, or `RUNTIME_NOOP` compatibility variants.
- APIs where the meaningful names are option-hash keys rather than formal
  parameters.
- Ambiguous overload sets where a named call cannot reliably choose a single
  variant at parse time.

Fixed parameters before varargs can be enabled only when qpp and parser
semantics make the fixed names unambiguous and the remaining varargs remain
positional. Treat each such API as a case-by-case decision.

## Parameter Names

Prefer names that describe the user's intent at the call site:

| Avoid | Prefer | Notes |
|-------|--------|-------|
| `arg`, `value1`, `value2` | domain-specific names | Generic names are only acceptable for true converters like `type(value)`. |
| `l`, `h`, `strd`, `opts` | `values`, `hash_value`, `standard_fds`, `options` | Expand abbreviations unless they are established public terms. |
| `fmt` | `format` | Preserve only if the short form is already a widely documented API name and changing it is not worth the break. |
| `alg` | `algorithm` | Use the full noun for public calls. |
| `key_len` | `key_length` | Avoid internal abbreviations and underscore fragments that read like implementation variables. |
| `old_path`, `new_path` | `source_path`, `target_path` | Use semantic direction, especially for mutating filesystem/process APIs. |
| `uid`, `gid` | `owner`, `group` | Prefer the documented role when it is clearer than the system type name. |
| `sig` | `signal` | Avoid abbreviations in public names. |
| `usecs` | `microseconds` | Prefer full units. |
| `timeout_ms` | `timeout` | If the type is `timeout`, the unit belongs in the value type, not the name. |

Keep consistent names across related APIs:

- Data payloads: `data`, `compressed_data`, `base64_string`, `hex_string`.
- Containers: `values`, `hash_value`, `container`, `key`.
- Paths: `path`, `source_path`, `target_path`, `link_path`.
- Crypto: `algorithm`, `digest`, `key`, `iv`, `mac`, `mac_size`, `aad`.
- Time: `date_value`, `seconds`, `milliseconds`, `microseconds`.
- Object helpers: `object_value`, `class_name`, `method`, `arguments`.

If a rename is needed, update all of the following in the same commit:

- QPP signature parameter name.
- Doxygen `@param` name.
- Error messages that identify the argument.
- Tests and reflection assertions.

## Flags

Add `NAMED_ARGS` to the existing flag list:

```cpp
[flags=RET_VALUE_ONLY,NAMED_ARGS]
[flags=CONSTANT,NAMED_ARGS]
[flags=NAMED_ARGS;dom=FILESYSTEM]
```

If the implementation gains cancellation checks or any other possible exception
path, it is no longer `CONSTANT`; use `RET_VALUE_ONLY` when it has no side
effects visible beyond returning or throwing.

Do not use `NAMED_ARGS` on `DEPRECATED`, `NOOP`, or `RUNTIME_NOOP` variants.

## User-Facing Errors

Named-call failures should use the most informative error available:

- Unknown names should report `NAMED-ARG-UNKNOWN` and list accessible named
  parameters when the target is named-callable.
- Calls to variants that are not opted in should report
  `NAMED-CALL-NOT-SUPPORTED`.
- Varargs-only and dynamic call-reference cases should remain
  `NAMED-CALL-NOT-SUPPORTED`.

When an existing named-call error is more informative, preserve it rather than
replacing it with a generic failure.

## Tests

For each converted batch, add or update:

- Runtime named-call tests in `examples/test/qore/vars/named-arguments.qtest`.
- Reflection tests in `examples/test/qore/misc/reflection.qtest` using
  `FunctionVariant::isNamedCallable()` and `getNamedParameterNames()`.
- Focused functional tests for the touched subsystem.
- Negative tests when the batch changes error behavior or intentionally leaves
  related variants unmarked.

Run named-argument tests in all execution modes:

```bash
LD_LIBRARY_PATH=build build/qore --exec-mode=ast examples/test/qore/vars/named-arguments.qtest
LD_LIBRARY_PATH=build build/qore --exec-mode=ir  examples/test/qore/vars/named-arguments.qtest
LD_LIBRARY_PATH=build build/qore --exec-mode=jit examples/test/qore/vars/named-arguments.qtest
```

Run reflection with in-tree modules:

```bash
LD_LIBRARY_PATH=build QORE_MODULE_DIR=build/modules/reflection:build/modules/astparser:qlib \
  build/qore examples/test/qore/misc/reflection.qtest
```

## Audit Requirements

Run the repository audit checklist before each commit that changes QPP/C++ or
tests.

For C++/QPP changes:

- Search for filesystem and network operations and verify sandbox checks.
- Add `qore_check_cancel()` to loops that can iterate more than 100 times.
- Use checks every 100 iterations for tight loops and every 10 iterations for
  expensive loops.
- Do not introduce `qore_check_io_interrupt()`.
- Preserve exception safety with `ReferenceHolder`, `SimpleRefHolder`, or
  `std::unique_ptr` as appropriate.
- Re-run `git diff --check HEAD`.

For tests:

- Keep `%modern`.
- Keep executable permission.
- Use hard `%requires` for Qore-shipped modules and `%try-module` only for
  external optional modules.

## Binary Module Migration

For each binary module repository:

1. Build the module so generated qpp metadata is fresh.
2. Run the metadata inventory query against the module's `*.meta.json`.
3. Group candidates by API family and risk.
4. Review names using this checklist before adding `NAMED_ARGS`.
5. Rename unsafe formal parameters before exposing them.
6. Update module docs and examples that mention renamed parameter names.
7. Add runtime named-call tests and reflection/metadata checks if the module has
   a reflection test harness.
8. Run the module's focused tests and any cross-module integration tests.
9. Run sandbox/cancellation audits for touched C++/QPP files.
10. Commit in small, reviewable batches.

Carry over exclusions explicitly. If a module leaves a variant unmarked because
it is varargs, dynamic, deprecated, or option-hash based, record that rationale
in the commit notes or migration tracking issue.

## Current Core Migration Notes

These examples are from the core migration and should be reused as precedent:

- `sort(arg: 1)` should report an unknown named argument with accessible
  parameters when the function is named-callable.
- `sprintf(fmt: ...)` remains not supported while the varargs formatter is not
  opted in.
- Crypto `alg` was renamed to `algorithm`; `key_len` was renamed to
  `key_length`.
- Fixed vector formatter variants use `format` and `values`; varargs
  formatters such as `sprintf(fmt, ...)`, `printf(fmt, ...)`, and
  `f_sprintf(fmt, ...)` remain positional until a safe varargs convention is
  designed.
- Filesystem `glob_str` was renamed to `pattern`; `rename(old_path, new_path)`
  was renamed to `rename(source_path, target_path)`; `symlink(old_path,
  new_path)` was renamed to `symlink(target_path, link_path)`.
- Library/process helpers expanded internal abbreviations before exposure:
  `rc` -> `status`, `gids` -> `group_ids`, `sig` -> `signal`, `usecs` ->
  `microseconds`, `d` -> `duration`, `addr` -> `address`, `type` -> `family`,
  `strd` -> `standard_fds`, `opts` -> `options`, and `msg` -> `message`.
- Timeout parameters with the Qore `timeout` type should use the public name
  `timeout` unless an established API already exposes `timeout_ms`.
- Thread helpers use intent names for public calls: `tid` -> `thread_id`,
  thread-local data hashes use `data`, thread-local key lists use `key_list`,
  worker counts use `thread_count`, thread init uses `callback`, resource
  callback arguments use `argument`, and `TimeZone` parameters use `timezone`.
- Pseudo-methods should not expose internal abbreviations: use `separator`
  instead of `sep` or `str` when the value separates output, `precision` instead
  of `prec`, `decimal_separator` and `thousands_separator` instead of
  `decimal_sep` and `thousands_sep`, `pattern` and `byte_offset` for binary
  searches, `length` instead of `len` for binary slices, `other_hash` instead of
  `oh`, and `method` instead of generic `name` for callable-method checks.
- Fixed-arity functions in reviewed batches should leave no unmarked entries in
  that batch's generated metadata except intentional excluded variants.
