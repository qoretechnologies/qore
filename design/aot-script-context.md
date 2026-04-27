# Script-Context AOT

## Status

Script-context AOT is implemented through `qcc -c`, `.qo` declaration preload,
and `qcc -o <binary> *.qo` link mode. This document describes the current
design contract for multi-file Qore applications that are not user modules.

For artifact details, see `design/aot-object-files-and-module-artifacts.md`.

## Goal

Support C/C++-style builds for multi-file Qore applications:

```sh
qcc -c -L build/qo -o build/qo/main.qo src/main.qr
qcc -c -L build/qo -o build/qo/lib.qo src/lib.qc
qcc -o app build/qo/main.qo build/qo/lib.qo
```

The resulting executable starts without parsing the original Qore sources.

## Source Context

Script-context AOT differs from module AOT:

- There is no `.qm` module entry file.
- Multiple source files contribute to one `QoreProgram`.
- A C++ host may inject namespaces, constants, or functions before Qore code
  runs.
- The final executable may use generated glue or a custom host.

`qcc` options supporting this model include:

- `-c` / `--compile-only`: emit one `.qo`.
- `-L <dir>`: preload sibling `.qo` declarations.
- `--stub=<file>`: preload declarative Qore source mirroring host-provided
  runtime declarations.
- `--define=NAME[=VALUE]`: mirror host parser defines.
- `--parse-option=NAME`: mirror host parse options.
- `-l MOD`: load external modules needed for parse-time type resolution.
- `-e FN`: choose the Qore function called by generated link-mode `main()`.

## Declaration Preload Rules

Compile-time preload may create shells for:

- Namespaces.
- Classes and base-class references.
- Hashdecls.
- Typedefs.
- Enums.
- Constants, including deferred runtime-evaluated constants.

Preload must not run top-level code or init functions. Runtime registration is
responsible for value initialization and execution ordering.

## Runtime Registration

The generated or custom host must:

1. Initialize Qore.
2. Create the target `QoreProgram`.
3. Apply the same parse options, defines, stubs, and injected C++ symbols that
   were visible at qcc time.
4. Begin AOT batch registration.
5. Register all linked `.qo` objects.
6. End the batch, resolving cross-file metadata.
7. Run top-level code, `%exec-class`, or the selected entry function.
8. Destroy the program and shut down Qore.

## Ordering

`.qo` inputs must be registered in the same logical source order expected by the
application. Declaration shells allow forward references, but init functions and
top-level effects still follow registration order.

## Compatibility

Script-context AOT requires `%modern` code. Use `.qr` for new scripts; `.q`
keeps legacy parsing defaults unless `%modern` is explicit.

Artifacts carry feature flags so older runtimes reject unsupported metadata
rather than loading partially compatible binaries.
