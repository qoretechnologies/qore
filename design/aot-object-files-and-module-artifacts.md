# AOT Object Files and Module Artifacts

## Status

The `.qo` object-file pipeline is implemented in `qcc` and the AOT runtime.
This document records the current design contract rather than the historical
phase plan.

Relevant code:

- `qcc-main.cpp`
- `include/qore/intern/QoreAOT.h`
- `include/qore/intern/QoreAOTBinary.h`
- `lib/QoreAOT.cpp`
- `lib/QoreAOTRuntime.cpp`
- `lib/QoreAOTBinary.cpp`

## Artifact Types

| Artifact | Produced by | Purpose |
|---|---|---|
| Executable | `qcc -o app script.qr` or `qcc -o app *.qo` | Native launcher for a Qore script/app |
| `.qmod` | `qcc -m Module.qm` or `qcc -m qlib/Module` | Loadable binary user module |
| `.qo` | `qcc -c file.qr` / `file.qc` / `file.qm` | Relocatable object containing native code and AOT metadata |
| `.qoa` | `qcc -a --context=DIR *.qo` | Static archive exposing `qore_qoa_register_all()` |

`.qo` is both object code and declaration carrier. It contains compiled native
functions plus enough AOT metadata for later compile/link stages to preload
cross-file declarations.

## Object Registration

Script-context `.qo` files expose registration entries that a host or
qcc-generated glue object calls to add metadata and native function tables to a
`QoreProgram`.

Per-file module `.qo` files produced with `--context=DIR` are intermediate
fragment carriers. They do not emit the module self-registration ABI; `qcc -m
--from-objects` reads their fragment symbols and synthesizes a single `.qmod`
registration path.

Batch registration uses the multi-deserializer:

1. Open all metadata blobs and create namespace/class/hashdecl/typedef shells.
2. Resolve types and base classes across all sessions.
3. Register constants across all sessions.
4. Resolve own members, then inherited members.
5. Deserialize functions, methods, globals, static members, and init functions.
6. Commit class structures and run final fixups.

The cross-session barriers are required because one `.qo` can reference classes,
constants, or members declared in another `.qo`.

## Compile-Time Preload

`qcc -c -L<dir> file.qr` scans `.qo` files in preload directories and preloads
their declaration metadata before parsing `file.qr`. This gives Qore the C-style `.o + .h`
workflow without separate header files.

The preload step must not execute user code. It only creates declaration shells
needed for parse-time name and type resolution.

## Module Aggregation

`qcc -m <module-dir>` is the preferred clean-build path for split-directory
modules. It parses the module once and emits one `.qmod`.

`qcc -m --from-objects --context=<dir> *.qo` remains the object aggregation
path for workflows that need per-file object granularity, but it is not the
default standard-library module build path.

## Metadata Compatibility

Every AOT metadata blob carries:

- QORD magic and format version.
- Qore producer version.
- Maximum IR opcode ID.
- AOT feature flags.
- Module dependencies and reexports.
- Program metadata such as execution class where applicable.
- Module path prepend/append lists.
- Optional build information for diagnostics.

Loaders must reject metadata requiring unsupported features or newer opcodes.

## Symbol Index

Newly generated metadata may also carry an optional `SYMBOL_INDEX` section. The
section is versioned independently from the core QORD format and is advisory for
current runtimes: old readers can ignore it, and absence of the section remains
valid for older `.qo` files.

The first index version records:

- defined Qore symbols: namespaces, classes, hashdecls, enums and enum members,
  typedefs, constants, globals, functions, methods, constructors, static
  methods, static vars;
- known native symbols associated with compiled Qore bodies and init functions;
- compilation context key/value pairs such as producer Qore version, AOT binary
  version, feature flags, opcode limit, and source/module filters.

Imports are intentionally conservative in the first version. The index format
has import fields for provider/consumer source files, required hashes, and
dependency classes, but the writer omits unsafe guesses until parse/codegen
sites can report symbol identity precisely.

Hashes are split by use:

- `signature_hash` covers callable name, return type, parameter names/types,
  and varargs flags;
- `declaration_hash` covers the API surface relevant to recompilation, such as
  symbol kind, path, visibility, type surface, callable signature, and method
  modifiers;
- `value_hash` covers folded constant and enum values, while runtime-initialized
  constants are marked as `pending`;
- implementation/native body changes are represented by the native symbol
  association and future linker metadata, not by changing API hashes.

Build tools should treat these hashes as a dependency-planning contract. Runtime
loading still depends on the normal AOT metadata sections, not on the symbol
index.

## Inspection

`qcc --dump-info <path>` inspects executables, `.qmod`, `.qo`, `.qoa`, and
ordinary object files without executing embedded Qore code.

Optional flags:

- `--dump-symbols`: print an nm-like symbol table.
- `--dump-sections`: print object and AOT section tables.

With `--dump-symbols`, AOT metadata dumps also include `SYMBOL_INDEX` defined,
imported, native, and context records when the section is present.

The dump path is for diagnostics only; runtimes ignore `BUILD_INFO` and do not
require `SYMBOL_INDEX`.

## Build-System Contract

When `QORE_BUILD_AOT_MODULES=ON`, CMake builds standard user modules as `.qmod`
artifacts and installs them beside their `.qm` sources. Runtime module lookup
prefers `.qmod` in the same location, so installed modules use compiled code by
default.

The `qcc-format` stamp controls mass qmod rebuilds. Only changes that can alter
qmod output or qcc compile semantics should update that stamp.
