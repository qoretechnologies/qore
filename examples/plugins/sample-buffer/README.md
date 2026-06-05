# sample-buffer plugin

`sample-buffer` is a minimal build-tree binary module showing the plugin-type
registration ABI. It registers one placeholder dense value type and two
operations:

- `add`: scalar `BinaryValue` helper
- `dense_add_i64`: `DenseBufferBinary` helper for `int64_t` arrays

Build it with:

```sh
cmake --build build --target sample-buffer-plugin
```

Then inspect it with the advisory linter:

```sh
QORE_MODULE_DIR=build/modules/reflection:build/examples/plugins/sample-buffer \
    ./build/qore examples/plugins/qore-plugin-lint sample-buffer
```

Or run the plugin-type verification smoke and linter together:

```sh
tools/plugin-type-verify.sh --module sample-buffer \
    --module-ref build/examples/plugins/sample-buffer/sample-buffer-api-*.qmod
```

The type currently uses `auto` as its reflected Qore type because native
plugin value instances are still reserved for a later implementation pass. The
module is still useful as a complete reference for descriptor construction,
module-init handle usage, lifecycle hooks, serialization callbacks, operation
metadata, and cooperative cancellation in dense helpers.
