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
7. Run the selected entry function (or `%exec-class`, for hosts that use it).
8. Destroy the program and shut down Qore.

## Top-Level Code

Script objects never carry their sources' top-level statements: registration
runs the global variable initializers the objects carry, and no host runs
anything else at the top level.  (A single-source executable, `qcc -o app
script.qr`, is not built from objects: it runs its top-level code.)

Each object records, in the optional `UNRUN_TOP_LEVEL_CODE` section, the file,
line and kind of every top-level statement of its sources that executes code
(an assignment, a call, a local variable initialization, a control statement);
declarations without an initializer execute nothing and are not recorded.

`qcc` refuses to link an entry-function executable (`qcc -o app *.qo`, or a
multi-source executable) from objects whose top-level code it would silently
skip:

- a function, method, closure or initializer using a top-level local variable,
  found through the `QORE_AOT_LOCAL_SLOT_TOP_LEVEL` local slot flag in the
  object's slot maps (so objects built before the section existed are checked
  too): the variable would never be created
- any statement recorded in `UNRUN_TOP_LEVEL_CODE`

Compiling objects (`qcc -c`) is unaffected, as are hosts that register objects
themselves (`--link-qo` aggregates, custom C++ hosts): they define what runs.

## Ordering

`.qo` inputs must be registered in the same logical source order expected by the
application. Declaration shells allow forward references, but init functions and
top-level effects still follow registration order.

## Compatibility

Script-context AOT requires `%modern` code. Use `.qr` for new scripts; `.q`
keeps legacy parsing defaults unless `%modern` is explicit.

Artifacts carry feature flags so older runtimes reject unsupported metadata
rather than loading partially compatible binaries.
