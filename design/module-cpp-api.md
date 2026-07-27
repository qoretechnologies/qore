# The module C++ API mechanism

Tracking issue: [#5371](https://github.com/qoretechnologies/qore/issues/5371)

## Goal

Let one binary module consume another's C++ API, without the API having to be promoted into
libqore.

Before this, there was no supported way to do that, and the workaround was to move the shared code
into the core library: [#5366](https://github.com/qoretechnologies/qore/issues/5366) did exactly
that with the JSON codec (`lib/QoreJson.cpp` plus an installed `include/qore/QoreJson.h`), and
[#5367](https://github.com/qoretechnologies/qore/issues/5367) considered doing the same for Avro.
That does not scale: libqore would accrete every codec worth sharing, even when the code has no
business in the core language library.

## Why a direct cross-module symbol reference is not enough

A direct reference to another module's exported symbol already *resolves* on both Linux and macOS:

- `lib/ModuleManager.cpp:44` opens modules with `RTLD_LAZY|RTLD_GLOBAL`, so a module's
  `DLLEXPORT` symbols are process-global once loaded.
- `cmake/QoreMacros.cmake:378` already links module bundles with `-Wl,-undefined
  -Wl,dynamic_lookup` on macOS.
- The `RTLD_NOW` reopen in `reopen_aot_binary_module_now()` (`lib/ModuleManager.cpp:2763`) is
  AOT-only, so ordinary C++ modules keep lazy binding.

The problem is how it *fails*. Binding is lazy, so a header/ABI mismatch between two independently
released repositories is a process abort at the first call rather than a load error. There is no
version gate — libqore consumers get `QORE_MODULE_API_MAJOR`/`MINOR`, a module-to-module C++
dependency gets nothing. Load order becomes an unwritten contract. And an optional dependency is
impossible: there is no C++ equivalent of `%try-module`.

`include/qore/sshutil.h` is the one place in the tree that already does cross-module C++ sharing,
and it documents the same territory from the other side: it uses accessor *functions* rather than
data symbols precisely because an undefined data symbol fails at `dlopen()` time, while a function
symbol binds lazily. It has no version negotiation at all, and both modules involved ship from the
same repository, which is what makes that workable there and not in general.

## The mechanism

`include/qore/QoreModuleCppApi.h` (installed) plus `lib/QoreModuleCppApi.cpp`.

An exporting module publishes one `extern "C" DLLEXPORT` entry point named
`<feature>_qore_cpp_api` — the same feature-name-with-hyphens-replaced convention the module
description function uses — returning a pointer to a static struct of function pointers whose
first member is a `QoreModuleCppApiHeader { unsigned major; unsigned minor; }`.

A consuming module includes only the producer's installed header and calls
`q_get_module_cpp_api(feature, major, minor, xsink)`, which:

1. finds the module, loading it on demand if necessary;
2. resolves the entry point through the module's own `dlopen()` handle;
3. calls it with the requested version;
4. validates the returned struct's header against the request;
5. returns the struct, or raises an exception.

There is no link-time dependency between the two modules, no new installed shared library, and no
new packaging entries.

## Decisions

### 1. The version check lives in `q_get_module_cpp_api()`, not (only) in the producer

The issue sketched the check on the producer side: the entry point refuses a version it cannot
serve by returning `nullptr`. Both sides are implemented, but the *authoritative* check is the one
in libqore, reading `QoreModuleCppApiHeader` out of the returned struct.

The requested version is still passed to the entry point, because a producer that supports more
than one major version needs it to select which struct to return. But a producer that supports
exactly one major — the normal case — may ignore both arguments and return its struct
unconditionally: the header check catches the mismatch anyway.

Three reasons:

- **A producer that forgets the check is still safe.** Under a producer-side-only policy, the
  boilerplate is repeated in every producer and a missing check is a silent ABI violation that
  surfaces as a crash in the consumer.
- **The error message can name the version actually served.** A `nullptr` return carries no
  information; libqore would have to say "cannot serve 1.3" without being able to say what the
  module does serve. Reading the header lets the message be "publishes version 1.1; the caller
  requires 1.3 or higher; upgrade the 'avro' module".
- **It is the only check that can catch a producer whose header does not describe its struct.**
  This is exactly the failure the mechanism exists to prevent, and the producer is by definition
  the party that got it wrong, so the producer cannot be the party that detects it.

`examples/test/module-cpp-api/cppapitest-module.cpp` implements a deliberately broken producer for
this case: for a reserved major version it accepts the request and answers with a struct whose
header declares a different version. The test asserts that the consumer gets an exception rather
than the struct.

### 2. Version semantics: exact major, minimum minor

`hdr.major` must equal the requested major; `hdr.minor` must be greater than or equal to the
requested minor.

The consumer passes the version its *header* declared at compile time, so "minimum minor" means "I
may call anything up to member N". Since the struct is append-only, a newer producer is a superset
and works unchanged; an older producer may be missing members the consumer would call, so it is
refused.

That is deliberately strict: a consumer that compiled against minor 3 but only uses minor 0
members is refused by a minor 0 producer even though it would work. The producer's accessor takes
a defaulted `minor` argument precisely so such a consumer can ask for less:

```cpp
static inline const QoreAvroApi* qore_avro_api(ExceptionSink* xsink,
        unsigned minor = QORE_AVRO_CPP_API_MINOR);
```

The alternative — inferring the minimum from which members are actually called — is not something
the compiler can tell us, so the explicit request is the only honest signal.

### 3. Resolution goes through the module's own `dlopen()` handle, not `RTLD_DEFAULT`

The issue proposed `dlsym(RTLD_DEFAULT, ...)`, which works because modules are opened
`RTLD_GLOBAL`. Using `QoreBuiltinModule::getPtr()` instead is strictly better: it cannot pick up a
same-named symbol from a different module, and "this module does not export the entry point" is
answered accurately rather than "nothing in the process exports it".

`RTLD_DEFAULT` remains the fallback for a module with no `dlopen()` handle — a statically
registered AOT module (`ModuleManager::registerAOTStaticModule()`), whose code is linked into the
host image.

### 4. The producer's namespace is not imported anywhere

`q_get_module_cpp_api()` loads the module with no `QoreProgram`. A C++ API consumer needs the
producer's *code*, not its Qore-language types, and injecting a namespace into whichever Program
happens to be current would be a surprising side effect of a C++ call.

`ModuleManager::runTimeLoadModule()` already supports this: `ProgramRuntimeParseContextHelper` is
a no-op for a null Program.

### 5. Only types with an already-public ABI cross the boundary

`QoreValue`, `BinaryNode*`, `QoreString&`, `ExceptionSink*`, plain C types, and opaque handles
with explicit `ref`/`deref` members in the struct. Never a module-defined class layout, never
anything with inline members or virtual functions, because the two sides are compiled from
different source trees at different times.

Ownership is documented per member exactly as it is for libqore's own API. The avro module's
valgrind pass found two reference leaks in code that crossed no module boundary at all
(`design/avro-module.md`); an API boundary where ownership crosses a `.so` is the same class of
risk with a worse failure mode, so the test module's opaque handle type carries a live-instance
counter that the Qore-level test asserts returns to zero.

### 6. The struct is append-only within a major version

New members go at the end and bump the minor version. Removing, reordering or retyping a member —
or changing the meaning or the ownership contract of an existing one — requires a new major
version, which means a new struct with a new name (ex: `QoreAvroApi2`) and its own entry point, or
a coordinated release of every consumer.

### 7. Rejected: a separate versioned `libqore-<mod>.so` per shared API

The conventional Unix answer, and better for a large, class-shaped, template-heavy API: normal C++,
working debug symbols, SONAME-based versioning. But it costs a new installed library plus entries
in `qore.spec-fedora` and `debian/control` for every module that wants one, which makes it the
wrong default. It stays available for a case that needs it.

| | direct symbol reference | separate `libqore-<mod>.so` | API struct (implemented) |
|---|---|---|---|
| behaviour on mismatch | abort at first call | load error | clean Qore exception |
| version negotiation | none | SONAME, per-library discipline | explicit major/minor |
| load order | implicit contract | link-time | resolved on demand |
| optional dependency | impossible | impossible | natural |
| new installed artifacts | none | one `.so` plus `-devel` per module | none |
| macOS | needs `dynamic_lookup` | fine | fine |

## Exceptions

| exception | condition |
|---|---|
| `LOAD-MODULE-ERROR` | the module could not be found or could not be loaded (raised by the loader) |
| `MODULE-CPP-API-ERROR` | empty feature name; the module is a Qore-language module; the module exports no `<feature>_qore_cpp_api` entry point |
| `MODULE-CPP-API-VERSION-ERROR` | the entry point refused the request; or the published header's major differs, or its minor is lower than requested |

## Lifetime and caching

`QoreModuleManager` only unloads binary modules at shutdown, so a resolved API pointer is valid for
the life of the process and needs no pinning. Resolution takes the module manager's lock, so
consumers cache the pointer in a static — `examples/test/module-cpp-api/cppapiuser-module.cpp`
shows the pattern.

Resolution may raise a Qore-language exception, so the first use has to be somewhere an exception
can be reported. A module that requires the API unconditionally can resolve it from its module init
function instead: init callbacks run with the module manager's mutex released
(`ModuleLoadMapHelper` unlocks around them), so a nested load from there is safe.

## Tests

`examples/test/module-cpp-api/` builds two binary modules into the build tree that are **never
installed**, and drives them from `module-cpp-api.qtest` so that the mechanism is covered by
`run_tests.sh` and therefore by CI:

- `cppapitest` — the producer. Publishes `QoreCppApiTestApi` (three versions' worth of members,
  including an opaque handle with `ref`/`deref` and a live-instance count), and for a reserved
  major version behaves as the deliberately broken producer described in decision 1.
- `cppapiuser` — the consumer. Publishes **no** C++ API of its own, which also makes it the
  "module exports no C++ API" negative case. It includes only `QoreCppApiTestApi.h`, resolves the
  struct at run time, caches it, and exposes the results as Qore functions.
- `cppapisrc.qm` — a Qore-language module, for the "a Qore-language module cannot export a C++
  API" case. This has to be a source module: most `qlib` modules are AOT-compiled `.qmod` binaries
  at test time and would take the "exports no C++ API" path instead.

Neither module links against the other, and neither declares the other as a dependency, so a
successful call proves on-demand loading, symbol resolution and the ABI contract together.

Covered: successful resolution at every minor from 0 to the published one; producer-side refusal of
a too-new minor and of an unknown major; libqore-side rejection of a mismatched header major and of
a header minor lower than requested; unknown feature; a binary module with no entry point; a
Qore-language module; an empty feature name; calls through the resolved struct with C scalars,
`QoreString`/`QoreStringNode` (including multi-byte characters) and opaque handles; balanced
handle reference counting; and that the producer is loaded on demand without becoming a feature of
the consuming Program.

## Relationship to the JSON codec in libqore

#5366 moved the JSON codec out of `module-json` into `lib/QoreJson.cpp` because this mechanism did
not exist — `design/json-module-migration.md` decision 1 records that "the alternative — leaving
the codec in the module and having `avro` resolve symbols across `dlopen()`ed modules — was
rejected as fragile." libqore itself does not use that API, so once this mechanism exists the codec
has no remaining reason to live in the core library. Moving it back is tracked separately.
