# Binary Structures (`bindecl`) and a Typed FFI / Syscall Gateway — Design

**Status:** Proposed.

**Target:** Pending. The v1 language and runtime changes proposed here are
additive (new declaration kind and new pseudo-class). They do not break
existing source or binary compatibility and could land in any 2.x release
once accepted. The typed FFI/syscall layer is kept as a follow-up module
design that consumes `bindecl`; this document records the integration
requirements but does not make broad FFI part of the v1 implementation.

**Branch context:** Targets Qore on top of the IR/JIT/AOT pipeline. See
[`design/qore-jit-aot-current-state.md`](../design/qore-jit-aot-current-state.md)
and [`design/qore-ir-spec.md`](../design/qore-ir-spec.md) for the substrate
this proposal builds on.

**Companion designs:**
- [`plugin-types-and-dense-data.md`](plugin-types-and-dense-data.md) — dense
  homogeneous typed arrays (`buffer<T>`) registered through a public
  plugin-type protocol. **Complementary, not overlapping**: `buffer<T>`
  expresses *N elements of one primitive type* (NumPy-style); `bindecl`
  expresses *one record with a fixed heterogeneous layout* (C-struct /
  Python-`ctypes`-style). The two compose: a `bindecl` member can be a
  fixed-size `buffer<T>` to express e.g. an MTU-sized payload following a
  packet header.
- [`hashdecls-cpp.md`](../design/hashdecls-cpp.md) — `hashdecl` is a
  conceptual sibling of `bindecl`: typed records with named members. The
  representation differs — `hashdecl` uses a `QoreHashNode` of boxed
  `QoreValue` cells; `bindecl` uses a contiguous `binary` buffer whose
  layout is pinned at declaration time.

---

## Summary

This proposal separates two layered capabilities for Qore:

1. **`bindecl`** — a new top-level declaration kind, parallel to
   `hashdecl`, that defines a *binary-layout record*: each member has a
   declared concrete bit/byte position, width, encoding, and byte order,
   so an instance is **backed by a contiguous `binary` buffer** rather
   than a hash of boxed cells. Member access is a typed offset read or
   write into that buffer, not a key lookup. The buffer itself is the
   on-the-wire / on-disk / on-FFI-stack representation, with no
   intermediate marshalling step.

2. **A future typed FFI module (`ffi`)** that lets Qore code call C
   library functions and (on Linux) raw syscalls through `libffi`, with
   `bindecl` instances usable directly as fixed-layout argument and
   return buffers where the C ABI matches their declared layout. This is
   the right primitive for the user's stated "syscall gateway" goal —
   raw `syscall(2)` is a thin specialization on top of this layer for
   newer Linux syscalls without libc wrappers (`io_uring_*`, `pidfd_*`,
   `landlock_*`).

The first capability is the v1 language change. The second is its natural
consumer and is described here only to keep `bindecl` compatible with it:
the full FFI design still needs a separate pass for C type aliases,
native ABI layout validation, pointer lifetimes, callbacks, varargs, and
sandboxing policy.

The combination unlocks a class of work that today requires writing C++
binary modules: network protocol implementations, file format parsers,
DB wire protocols, hardware/embedded register access, IPC payloads,
`ioctl` argument structures, forensics tooling, and wire-compatible RPC
with non-Qore peers. Each of those cases moves from "C++ module + qpp
binding" to pure Qore.

---

## 1. Motivation

### 1.1 What Qore can already do

Qore already has the **value-level** building blocks for binary work:

- `binary` — opaque byte buffer, refcounted, with `<binary>::size()`,
  `<binary>::get*()`/`<binary>::splice()`/concatenation operators.
- `<string>::getByte()` and `Qore::Scanner` (added on
  `bugfix/text_processing`) for byte-oriented scans of string sources.
- `BinaryInputStream` / `BinaryOutputStream` for stream-shaped I/O.
- Per-platform endianness handling inside individual modules
  (e.g. `xxhash.cpp:235` does explicit `XXH_swap32`).

What Qore lacks is **a way to express the layout of a binary record
declaratively, once, at parse time**, and have the parser, IR, JIT, and
AOT pipeline treat field accesses as typed offset reads.

### 1.2 The current cost of binary work in Qore

Today, a Qore programmer who wants to parse, say, an ICMPv4 echo header
writes one of two things:

```qore
# Option A: hand-coded byte-shuffling against <binary>/getByte
hash<icmp_hdr> parse_icmp(binary pkt) {
    return {
        "type":     pkt[0].toInt(),
        "code":     pkt[1].toInt(),
        "checksum": (pkt[2].toInt() << 8) | pkt[3].toInt(),
        "id":       (pkt[4].toInt() << 8) | pkt[5].toInt(),
        "seq":      (pkt[6].toInt() << 8) | pkt[7].toInt(),
    };
}
```

Tedious, easy to get wrong on endianness, no parse-time type checking,
no type-aware IR/JIT lowering, and the *write* path is even worse —
constructing the same packet for transmission requires `chr(b)` plus
explicit `<binary>::splice` or repeated concatenation.

The alternative is to drop into a C++ module and expose a hashdecl
binding, which is the path most existing protocol modules in `modules/`
have taken.

### 1.3 The gap stated precisely

There is no way today to say in Qore: *"here is a record with these
members at these byte offsets in this byte order, and I want member
access to compile to a typed load/store against the underlying buffer
with no boxing."* The result is that:

- **Pure-Qore protocol parsers/serializers are slow** because every
  field read goes through boxed `QoreValue` paths plus manual shifting.
- **C++ modules carry layout knowledge** that could otherwise live in
  Qore source, complicating the build and making protocols hard to
  iterate on.
- **FFI is unattractive** because there is no good way to construct
  argument structs (`struct sigaction`, `struct iovec`,
  `struct termios`, `struct sockaddr_in*`, `struct kevent`, etc.)
  without either copying through a hash representation or writing a C++
  helper.

### 1.4 Why now

This branch (`feature/5164_jit`) makes the gap larger and the fix
cheaper at the same time:

- *Larger*, because the IR/JIT/AOT pipeline pays a noticeable cost for
  every boxed `QoreValue` flowing through hot loops; binary protocol
  parsers are exactly the workload where that overhead dominates.
- *Cheaper*, because the substrate to lower a typed offset read into a
  small LLVM IR sequence (`load i32, ptr; bswap if needed; mask if
  bitfield`) is already in place — the same machinery that lowers
  `AddInt` to a single LLVM `add nsw` instruction. A `bindecl` field
  read is, conceptually, a much narrower operation than a generic
  `LoadHashKey`.

The companion plugin-type protocol proposal
(`plugin-types-and-dense-data.md`) makes the case at length that the
substrate is ready for typed dense data; `bindecl` reuses that argument
in a different shape — heterogeneous records instead of homogeneous
columns.

---

## 2. The `bindecl` declaration

### 2.1 Syntax

A `bindecl` declares a binary-layout record. The keyword sits at the
same position as `hashdecl` and `class`:

```qore
bindecl IcmpEchoHeader [endian=big] {
    uint8  type;
    uint8  code;
    uint16 checksum;
    uint16 identifier;
    uint16 sequence;
}
```

The declaration-level attribute list (the `[...]` after the name)
controls record-wide layout settings. Per-field overrides go in the
same syntax immediately after the type:

```qore
bindecl IpV4Header [endian=big] {
    uint8  version_ihl;                   # composite; access via bitfields below
    uint8  dscp_ecn;                      # likewise
    uint16 total_length;
    uint16 identification;
    uint16 flags_fragment_offset;
    uint8  ttl;
    uint8  protocol;
    uint16 header_checksum;
    uint32 source_ip;
    uint32 destination_ip;
    uint32 [endian=little] vendor_marker; # one little-endian field in an otherwise big-endian record
}
```

### 2.2 Member types

The v1 member types form a closed set chosen so that **layout is fully
determined by the declaration text**. The only host-sensitive marker is
the explicit `native` byte-order attribute, which is resolved at runtime
on the host that creates or loads the record.

| Type form                  | Width / encoding                                             |
|---|---|
| `uint8` / `int8`           | 1 byte                                                       |
| `uint16` / `int16`         | 2 bytes (endian per attribute)                               |
| `uint32` / `int32`         | 4 bytes (endian per attribute)                               |
| `uint64` / `int64`         | 8 bytes (endian per attribute)                               |
| `float32` / `float64`      | IEEE-754 binary32 / binary64 (endian per attribute)          |
| `bool8`                    | 1 byte, 0 = false, non-zero = true                           |
| `byte[N]`                  | N raw bytes, no encoding                                     |
| `string[N, encoding=...]`  | N-byte fixed buffer; trailing zero-bytes trimmed on read     |
| `zstring[N]`               | N-byte buffer, NUL-terminated, max N-1 chars                 |
| `bindecl-name`             | Nested record (contiguous, uses nested decl's own resolved layout); see "self-reference" rule below |
| `bindecl-name[N]`          | Fixed array of nested records                                |
| `<scalar-type>[N]`         | Fixed array of integer, float, or bool scalars               |
| `<int-type> : N`           | Bitfield — N bits within the parent integer (see §2.4)       |

**Variable-length tail members, unbounded `zstring`, and `buffer<T>[N]`
members are deferred to v2** to keep v1 records statically sized:
`size()`, every field offset, AOT encoding, and IR lowering all stay
constant. v1 protocol code parses a static header and slices the
remaining bytes manually. A roadmap sketch for the v2 length-driven
form is in §12.9.

Nested records are layout-stable by declaration, not by use site. A
member of type `OtherDecl` embeds the bytes described by `OtherDecl`'s
own resolved layout and layout fingerprint. The parent declaration only
chooses where that contiguous byte span is placed; parent `endian`,
`bitorder`, and member-level overrides do not reinterpret the nested
decl's fields. Reusing the same nested declaration in two parents
therefore always means the same on-wire/on-disk layout.

**Placement vs. internal layout:** the parent's `align` controls where
the nested span begins relative to the parent (i.e. any padding inserted
before the nested record). The child's own `align` controls its
internal layout — by v1 default, `packed`. So `bindecl A [align=8] {
B inner; }` places `inner` at the next 8-aligned offset within `A`, but
`inner` itself is laid out per `B`'s declared `align`.

**Self-reference is rejected.** A `bindecl` member whose type is the
enclosing `bindecl` (directly or transitively) implies infinite static
size; the compiler raises `BINDECL-SELF-REFERENCE` at the layout pass.
Indirect self-reference through a nested decl that contains the
parent is rejected the same way.

### 2.3 Declaration attributes

Record-level attributes (`bindecl Name [...] {}`):

| Attribute               | Values                   | Default       | Meaning                                      |
|---|---|---|---|
| `endian`                | `big`/`little`/`native`  | (no default — required when any multi-byte field is declared, see §10.2) | Byte order applied to multi-byte members unless overridden per field |
| `align`                 | `1`/`2`/`4`/`8`/`packed` | `packed`      | Alignment policy between members              |
| `bitorder`              | `msb-first`/`lsb-first`  | (no default — required when any bitfield is declared, see §10.3) | Bitfield packing direction                    |
| `size`                  | integer                  | (computed)    | Pin total record size — error if computed size differs |

Per-field attributes:

| Attribute  | Values                | Meaning                                     |
|---|---|---|
| `endian`   | `big`/`little`/`native` | Override record-level endian for this field |
| `offset`   | integer               | Pin field offset — error if computed offset differs |

`align=packed` is the default because the overwhelmingly common use
case is wire/disk formats where the layout is pinned by spec, not by
the host's natural alignment. Programmers who want C-struct-on-this-host
layout for FFI to C code use an explicit `align=` plus the ABI validation
helpers from the follow-up FFI design; `align=8` alone is not a portable
description of an arbitrary C ABI.

v1 requires **explicit `endian=`** on either the record or every
multi-byte scalar field. There is no implicit byte-order default. `uint8`,
`int8`, `bool8`, `byte[N]`, and raw byte/string buffers do not require an
endian attribute. `native` means "the byte order of the runtime host" and
is intended for host-local ABI layouts, not portable wire formats.

The `offset` and `size` attributes are *assertions*, not
*directives* — the compiler computes the layout and errors if the
declaration's pinned values don't match. This catches wire-spec drift
early.

### 2.4 Bitfields

A bitfield is `<integer-type> : <bit-count>`. Bitfields pack within
their declared parent type from MSB or LSB depending on a required
record-level `bitorder` attribute:

```qore
bindecl IpV4Header [endian=big, bitorder=msb-first] {
    uint8  version : 4;
    uint8  ihl     : 4;
    uint8  dscp    : 6;
    uint8  ecn     : 2;
    uint16 total_length;
    # ...
}
```

The compiler errors if a bitfield run does not exactly fill its parent
integer. Padding can be expressed as a named field `uint8 _pad : 3` or
via a `_:` anonymous syntax (`uint8 _ : 3`).

Bitfield run rules:

- A run is one or more consecutive bitfield members with the same parent
  integer type. Mixing `uint8 : N` and `uint16 : N` in the same run is a
  parse error; start a new run by ending the previous parent integer
  exactly and declaring the next bitfield after it.
- `offset=` is allowed only on the first member of a bitfield run and
  pins the byte offset of the parent integer. Applying `offset=` to a
  later member in the same run is a parse error.
- The sum of bit widths in a run must equal the parent integer width.
  Gaps are represented explicitly with named or anonymous padding
  bitfields.
- `endian` applies to the parent integer's byte order for parent widths
  greater than one byte. Single-byte bitfield runs (`uint8 : N` /
  `int8 : N`) ignore any `endian=` setting since there is no byte-order
  ambiguity. `bitorder` then defines how bit numbers are assigned within
  the endian-decoded parent integer: `msb-first` consumes bits from
  most-significant to least-significant; `lsb-first` consumes bits from
  least-significant to most-significant.
- Bitfield signedness follows the declared parent integer type. Signed
  bitfield reads sign-extend from the declared bit width; unsigned reads
  zero-extend.

Worked example for `endian=native`: with parent `uint16`,
`endian=native, bitorder=msb-first`, and a leading 4-bit field, on a
little-endian host the field occupies bits 15..12 of the decoded
`uint16` — which is byte 1 bits 7..4 in memory. On a big-endian host
the same declaration places the same logical bits at byte 0 bits 7..4.
Cross-platform-stable code uses `endian=big` or `endian=little`, not
`native`; `native` is for host-local ABI layouts only.

### 2.5 Lifecycle

A `bindecl` instance is **a typed window into a binary**, not a typed
copy of one. Concretely, the runtime representation is a logical span
`{BinaryNode*, byte_offset, byte_length}` plus a `const BindeclDecl*`.
Member reads and writes resolve as offset-relative reads and writes
into the backing `BinaryNode`. Implicit conversion to `binary` returns
exactly the bytes covered by the span, not the whole backing buffer.
Mutation honours the existing `binary` COW invariant — see §10.4. For
all v1 records `byte_length == Name::size()`; the v2 variable-tail
extension is the only place this invariant relaxes.

Construction:

```qore
# Fresh, zero-initialized buffer (offset 0, length = Name::size())
IcmpEchoHeader hdr1();

# Zero-copy view over the first record-sized span of an existing binary.
IcmpEchoHeader hdr2 = IcmpEchoHeader::cast(packet);

# Zero-copy view over a record-sized span starting at byte offset 14.
IcmpEchoHeader hdr3 = IcmpEchoHeader::cast(packet, 14);
```

The `cast` form is a static factory on the bindecl type itself —
analogous to a class-level static method. The two-step
`IcmpEchoHeader hdr; hdr = IcmpEchoHeader::cast(packet);` is also
supported.

**Factory semantics:**

- `Name::cast(binary b, int offset = 0)` accepts any `b` with
  `b.size() - offset >= Name::size()` and `offset >= 0`. This is the
  normal protocol-parser path: a header is viewed over a larger packet
  without copying the payload. Trailing bytes after the record are
  permitted and remain in the backing binary but are not part of the
  span.
- `Name::castExact(binary b, int offset = 0)` accepts any `b` with
  `b.size() - offset == Name::size()` and `offset >= 0`. Trailing
  bytes are rejected. Used by file-format / validation code that wants
  no slack at the tail.
- Both factories raise `BINDECL-SIZE-MISMATCH` on size violations and
  on `offset < 0` or `offset > b.size()`. The same exception class
  covers all sizing and offset failures from cast.

**Conversion back to `binary` is implicit and zero-copy:**

```qore
binary out = hdr1;          # zero-copy share of the logical record span
```

There is no `.toBinary()` method — the implicit assignment is the
public surface. If the instance was cast over a larger packet, the
assigned `binary` contains only the logical record span (`Name::size()`
bytes starting at `byteOffset()`), not the whole source packet.
Programmers who want a private copy use `clone()`:

```qore
IcmpEchoHeader copy = hdr1.clone();   # private deep-copy of the record span
binary copied_bytes = copy;            # independent storage from hdr1
```

The `<bindecl>` pseudo-class (added once in libqore, applies to every
`bindecl` instance) exposes:

```qore
int                 <bindecl>::size()            # always equal to the record size
int                 <bindecl>::byteOffset()      # span offset within the backing binary
bool                <bindecl>::isShared()        # true if mutation would trigger COW
<bindecl-instance>  <bindecl>::clone()           # explicit deep copy
hash<auto>          <bindecl>::toHash()          # one-shot conversion to a plain hash for debugging / serialisation
BindeclDecl*        <bindecl>::getDecl()         # the decl the instance was constructed against
string              <bindecl>::toString()        # human-readable form for debug printing (member names + values)
```

`byteOffset()` returns the span offset within the *current* backing
binary. Concrete values:

| Construction                          | `byteOffset()` |
|---|---|
| Fresh `Name()`                        | `0` (private buffer)                                    |
| `Name::cast(b)`                       | `0`                                                     |
| `Name::cast(b, k)`                    | `k`                                                     |
| `Name::castExact(b)` / `(b, k)`       | `0` / `k`                                               |
| `clone()` of any of the above         | `0` (clone always allocates a private buffer of `size()` bytes) |
| After a mutation that triggered COW   | `0` (COW splits storage and the new buffer covers only the span) |

`toHash()` returns a hash mapping member name to value, with these
recursion rules:

- Scalar primitives → their natural Qore type (`int`, `float`,
  `bool`, `binary`, `string`).
- Bitfields → `int`, sign-extended where the declared type is signed.
- `string[N, encoding=...]` → `string` decoded in the declared
  encoding (or default if unspecified).
- Fixed scalar arrays (`<scalar>[N]`) → `list<...>` of the element
  type.
- Nested decl members → recursive `toHash()` of the nested instance.
- Fixed nested-decl arrays → `list<hash<auto>>`.

`toHash()` is lossy by design (encoding info, bit positions, and
signed/unsigned attributes are dropped); for round-trip use
`Serializable::serializeToData()` instead.

The `cast(...)` and `castExact(...)` factories are **not**
pseudo-methods on `<bindecl>` because there is no instance to dispatch
on — they are static methods on each individual `bindecl` type,
parallel to how class static methods work. The implementation
registers them once per decl at decl-creation time. Reflection over
the decl exposes them through the `BindeclDecl` reflection surface
the same way class static methods are exposed via `Class` reflection
today (see §5.6).

### 2.6 Type system

Every `bindecl` introduces a nominal type at the same parse-time tier
as `hashdecl`. `IcmpEchoHeader` is the type, `*IcmpEchoHeader` the
or-nothing form, `<bindecl>` the universal pseudo-class. Member access
is type-checked at parse time; member writes are type-checked against
the declared field type.

`bindecl` types are **not** structurally compatible with each other —
two records with identical layouts are still distinct types. This is
intentional: a `TcpHeader` and an `UdpHeader` may share field shapes
in some respects but mean different things. v1 does not add a
`reinterpret<T>()` operator; type-punning between declarations is a
future extension and should be spelled through an explicit binary
conversion and a second `OtherDecl::cast(...)` in v1 code.

`bindecl` types **are** assignment-compatible with `binary` in these
specific write contexts, all with zero-copy span semantics:

- assigning to a statically `binary`-typed local/global/member variable;
- returning from a function whose declared return type is `binary`;
- passing an argument to a parameter whose selected variant requires
  `binary`;
- calling binary-consuming stream APIs such as
  `BinaryOutputStream::write(...)`;
- explicit `binary` cast/conversion syntax, if the implementation
  exposes one consistently with existing cast rules.

They are **not** implicit-readable as `binary` for arbitrary expression
contexts. Overload resolution prefers an exact `bindecl` match over the
`binary` conversion; the conversion is not considered for untyped
arithmetic, concatenation, equality, or generic `auto` contexts unless an
explicit cast or binary-typed target is present. This avoids losing the
layout invariant silently while still making I/O paths ergonomic.

**Implicit conversion is one-way.** `bindecl → binary` is implicit in
the contexts above; `binary → bindecl` is *never* implicit. A `binary`
value reaching a `bindecl`-typed sink (parameter, return, lvalue) is a
parse error; the call site must use `Name::cast(b)` /
`Name::castExact(b)` explicitly. This keeps layout-fingerprint and
size validation visible at every binary-to-decl boundary.

### 2.7 Field read/write semantics

Field reads and writes are intentionally strict:

- Unsigned and signed integer writes are range-checked before encoding;
  out-of-range values raise `BINDECL-RANGE-ERROR`.
- Signed integer and signed bitfield reads sign-extend to Qore `int`.
- `bool8` writes normalize to `0` or `1`; reads treat any non-zero byte
  as `True`.
- `float32` / `float64` preserve the IEEE-754 bit pattern, including NaN
  payloads where the host conversion path preserves them.
- `byte[N]` assignment requires a `binary` of exactly `N` bytes.
- `string[N, encoding=...]` assignment encodes the string, errors if the
  encoded byte length exceeds `N`, and zero-pads the remaining bytes.
- `zstring[N]` assignment requires an encoded length of at most `N - 1`
  bytes, writes a terminating NUL, and zero-pads the remainder. Reads stop
  at the first NUL or at `N` bytes if no NUL is present.
- Fixed scalar arrays expose checked per-element load/store; assigning
  the whole field requires exactly `N` values.

### 2.8 AOT/IR

A `bindecl` definition serialises into the AOT QORD format as a new
section type carrying:

- The declaration name and namespace path.
- Per-member: name, type code, width/bitfield params, endian, offset,
  any nested decl reference.
- Computed total size and alignment.

Member access lowers to one of two IR opcodes per type/width
combination:

| Opcode form           | Lowering                                         |
|---|---|
| `LoadBindeclField`    | Load typed value from buffer at offset (with bswap if needed) |
| `StoreBindeclField`   | Store typed value to buffer at offset (with bswap if needed)  |

For bitfields, the load is a load+shift+mask; the store is a
load-modify-write. A future optimization can fuse adjacent bitfield
reads/writes into a single load-modify-write where the verifier can
prove the parent integer is not mutated between accesses.

The verifier and JIT lowering treat `LoadBindeclField` as a bounded,
side-effect-free read of a logical binary span plus an immediate
offset/width. `StoreBindeclField` is a mutating operation: it may trigger
binary COW, it writes bytes in the span, and bitfield stores perform a
read-modify-write of the parent integer. Optimisation passes must not
move stores across overlapping loads/stores unless alias analysis proves
the byte ranges cannot overlap.

---

## 3. Integration note: typed FFI / syscall gateway

The user's original framing paired binary structures with a "syscall
gateway." The right shape for that capability is a separate
**libffi-based typed FFI module**, with raw `syscall(2)` as a thin
specialization on top — not a peer to `bindecl`, but a consumer of it.
That module is **out of scope for this design** and gets its own
design-pending document. This section records only the integration
constraints `bindecl` v1 must satisfy so the FFI follow-up can land
without a second redesign of `bindecl`.

**Why FFI is the right primitive (not raw syscall).** Direct `syscall(2)`
is non-portable (numbers differ across kernels and architectures),
ABI-fragile (libc's argument marshalling is bypassed), and a giant
sandbox hole. libffi-based FFI is portable, calling-convention-aware,
and covers the common case (libc functions like `mlock`,
`posix_madvise`, `setrlimit`) plus the rare case (newer Linux syscalls
without libc wrappers — `io_uring_*`, `pidfd_*`, `landlock_*`) as a
thin specialization.

**What `bindecl` must give the FFI layer:**

- A stable C-pointer view into a record's backing buffer, exposed via a
  `BindeclInstance` API the FFI layer can call. The API must be split
  by mutability:
  - read-only access returns `const void*`, byte length, and a
    refcount-keepalive without forcing COW;
  - mutable access first materializes a unique buffer if the backing
    `binary` is shared, then returns `void*`, byte length, and a
    keepalive for the unique buffer. The materialized buffer covers
    exactly `Name::size()` bytes; the rest of the original source
    binary is not copied. After materialization the instance has
    `byteOffset() == 0` and `isShared() == false`.
  The returned pointer is scoped to the FFI call; C code must not
  retain it after the keepalive is released. Any pinning surface for
  longer-lived pointer lifetimes is owned by the FFI design, not by
  this proposal.
- A way to construct a fresh zero-initialized instance for "out"
  arguments (covered by the no-arg constructor in §2.5).
- Stable `byte_offset` semantics so the FFI layer can pass either a
  whole-binary span or a sub-record without copying.
- The `endian=native` and `align=` attributes for cases where the
  programmer asserts host-ABI layout. Note: these alone are not
  sufficient to *prove* C-struct compatibility; see §10.5.

**What FFI owns, not `bindecl`:**

- C ABI type aliases (`c_long`, `size_t`, `ssize_t`, `ptrdiff_t`,
  `time_t`, pointer-sized integer types).
- Platform-specific layout validation (`sizeof`, `_Alignof`, `offsetof`
  cross-checks against the target platform).
- Pointer-bearing C structs (`struct iovec`, `struct sigaction`,
  callback tables) — `bindecl` describes byte layout, not pointer
  semantics; the FFI layer represents pointer fields in its own
  signature/type system and lets them point at `bindecl` spans,
  `binary` buffers, or callback trampolines with explicit lifetime
  rules.
- Calling conventions, varargs, struct-by-value rules, callback
  trampolines from Qore to C, asynchronous completion integration.
- Per-import sandboxing — arbitrary user-declared FFI imports run as
  `QDOM_UNCONTROLLED_API`; trusted wrapper modules with audited fixed
  imports may declare narrower domains
  (see [`design/module-sandboxing-audit-guide.md`](../design/module-sandboxing-audit-guide.md)).

**Effort accounting.** This document estimates only the
`bindecl`-side work. Effort for the FFI module itself belongs in the
FFI design and is not counted in §11.

---

## 4. Use cases

The combination of `bindecl` + FFI displaces a class of work that
currently requires writing C++ binary modules. Examples, organized by
domain:

**Network protocols, pure-Qore implementations.** ICMPv4/v6, MQTT
(fixed and variable headers), AMQP framing, BACnet, IPMI, DNS, DHCP,
NTP, DTLS records, GTP-U, custom UDP/TCP framings. Today these either
live in C++ modules or are not implemented; with `bindecl` they're a
few hundred lines of Qore each.

**File format parsers and writers.** ELF/PE/COFF/Mach-O headers and
program/section tables; ZIP/GZIP/tar; PNG/TIFF/MP4 (box headers);
PCAP/PCAP-NG; ASN.1 DER outer framing; ID3v2; FAT/exFAT directory
entries; PDF cross-reference tables. Forensics and asset-pipeline tools
move into Qore.

**Database wire protocols.** PostgreSQL frontend message framing,
MySQL packet headers, MongoDB BSON, Redis RESP2/RESP3 (the
length-prefixed parts), Cassandra CQL framing. Some of the existing
Qore DB modules could move large portions of their parsing into pure
Qore.

**Hardware / embedded.** Memory-mapped device registers via `mmap`
(GPIO, V4L2, DRM, PCI config space); packed frames over serial / USB /
SPI / I2C; CAN bus frames; Modbus PDUs. With FFI for the `mmap` /
`ioctl` / `read`/`write` calls plus `bindecl` for the register and
frame layouts, a substantial slice of embedded Linux work becomes
expressible in Qore.

**IPC payloads.** Shared-memory queues (header + ring buffer + entry
records); Unix-socket ancillary data — `SCM_RIGHTS` for fd passing,
`SCM_CREDENTIALS` for peer auth — both of which require carefully laid
out `cmsghdr` records.

**`ioctl` argument structures.** `termios`, network interface info
(`SIOCGIFCONF` / `ifreq`), V4L2 controls, DRM modesetting, KVM VM
configuration. Today these all need a C++ shim per consumer.

**Wire-compatible RPC with non-Qore peers.** Custom binary RPC
protocols where the peer is C, Rust, or Go — `bindecl` is the schema
language, no external IDL needed.

**Cryptography wire formats.** TLS record framing, X.509 DER outer
structure, PKCS#7 / CMS, Kerberos AS-REP, SSH binary protocol. ASN.1
contents themselves stay out of scope (BER/DER is a different
abstraction), but the framing around them fits `bindecl` naturally.

**Forensics and binary inspection tooling.** A pure-Qore equivalent of
small `pwntools`-style scripts becomes practical when laying out
exploit payloads is just a `bindecl` declaration.

---

## 5. Compiler implementation

### 5.1 Lexer (`lib/scanner.lpp`)

- New keyword: `bindecl`.
- New context-sensitive keywords for primitive layout types: `uint8`/
  `uint16`/`uint32`/`uint64`/`int8`/`int16`/`int32`/`int64`/`float32`/
  `float64`/`bool8`/`byte`/`zstring`. These are *only* recognized
  inside `bindecl` member declarations to avoid colliding with user
  identifiers — see §10.6 for the mechanism.
- The existing global `string` keyword gains a new use inside `bindecl`
  member position (the `string[N, encoding=...]` form). It is **not**
  context-sensitive — `string` is already a Qore type keyword
  everywhere — so the lexer requires no new state for it.
- No `view` or `reinterpret` keyword is introduced in v1. Zero-copy
  views are expressed by `Name::cast(binary, offset)`, and type-punning
  remains a future extension.

### 5.2 Parser (`lib/parser.ypp`)

- New top-level production `bindecl_declaration` parallel to
  `hashdecl_declaration`.
- Member-list grammar with optional attribute lists and bitfield
  widths.
- Compile-time layout pass after parsing: walks the member list, computes
  offsets and sizes, validates `offset=` / `size=` assertions, validates
  fixed arrays and bitfield runs, validates explicit endian requirements,
  and validates that nested decls are fully defined.
- AST node `BindeclDeclNode` carrying the resolved layout.

### 5.3 Type checker

- New `bindeclTypeInfo` per-decl, parallel to the per-class typeinfos.
- Member access (`hdr.checksum`) parses to a typed access node that
  carries the resolved offset, width, endian, and bitfield params.
- Constructor, `cast(binary, offset = 0)`, and `castExact(binary, offset =
  0)` factory calls type-check against the declared layout. Field writes
  type-check against the declared field type and emit range/length checks
  where the value cannot be proven valid at parse time.

### 5.4 Runtime

- New `BindeclDecl` core type registered through the existing
  hashdecl-style registration path
  ([`design/hashdecls-cpp.md`](../design/hashdecls-cpp.md) describes
  the symmetric pattern). System `bindecl`s (used by core libqore)
  declare via the same `qpp` machinery.
- New `BindeclInstance` value class, refcounted, holding a
  `SimpleRefHolder<BinaryNode>` for storage, a byte offset, a byte length,
  and a `const BindeclDecl*` for layout. The logical span is shared with
  the underlying `BinaryNode` via the existing refcount, so implicit
  assignment to `binary` is zero-copy.
- New `QoreValue` tag — `NT_BINDECL_INSTANCE` — that points at the
  `BindeclInstance`. Existing object-shaped paths handle ref/deref
  uniformly.

### 5.5 IR / JIT lowering

The wire-format and on-disk concerns of AOT/IR are split into §6
("Serialization"); this subsection covers only the *lowering* shape.

- New IR opcode pair:
  - `LoadBindeclField(instance, decl_id, member_id) -> typed value`
  - `StoreBindeclField(instance, decl_id, member_id, value)`
- The verifier validates `(decl_id, member_id)` against the AOT decl
  table, ensuring offset/width/endian have been resolved.
- LLVM lowering: emit `getelementptr` against `buffer_base + span_offset`,
  `load`/`store` of the natural integer type, optional `bswap`, optional
  shift+mask for bitfields, and runtime range/length checks required by
  §2.7. This is the same broad shape as the existing typed
  `LoadHashKey`/`StoreHashKey` opcodes but cheaper.
- Bitfield store is read-modify-write of the parent integer; the verifier
  records this and the byte range touched by every store so optimisation
  passes don't reorder it across reads or writes of overlapping fields.
- `BindeclInstance` values do not flow through SSA value slots as
  inline bytes — the SSA value carries the boxed instance pointer; the
  Load/Store opcodes reach into the underlying buffer. This keeps
  `BindeclInstance` interoperable with all existing instruction shapes
  (return, `foreach` element, closure capture, etc.) without per-shape
  changes.

### 5.6 Pseudo-class and reflection

- `<bindecl>` pseudo-class registered once, dispatching to the
  per-instance decl pointer. Methods include `getDecl()`, `clone()`,
  `size()`, `byteOffset()`, `isShared()`, `toHash()`, and `toString()`
  (human readable). `cast(...)` and `castExact(...)` are static methods
  registered on each concrete bindecl type, not pseudo-class methods.
- Reflection: `BindeclDecl` exposed via the reflection module so tools
  can iterate members and offsets, parallel to `TypedHashDecl`
  reflection. Member iteration is always-available (no domain gate);
  full reflection across the program's decl table goes through the
  `reflection` module under `QDOM_REFLECTION` as today.
- The `cast(...)` / `castExact(...)` static factories appear in the
  `BindeclDecl` reflection surface as static methods — `getStaticMethod()`
  / `getStaticMethods()` analogue — the same way `Class` reflection
  exposes class static methods today. This lets QLS / qore-doc / hover
  tooling discover and document them without hard-coding the names.

### 5.7 AST parser module mirror

Per the project guideline ("When editing the core parser
(`lib/parser.ypp`, `lib/scanner.lpp`), mirror changes in
`modules/astparser/src/`"), the astparser module needs a parallel set
of changes so that QLS, qore-doc, and other tooling can parse, search,
and document `bindecl` declarations.

**Scanner — `modules/astparser/src/ast_scanner.lpp`:**

- Recognise the `bindecl` keyword (mirror of `TOK_BINDECL`).
- Recognise the context-sensitive primitive type keywords (`uint8`,
  `int8`, `uint16`, `int16`, `uint32`, `int32`, `uint64`, `int64`,
  `float32`, `float64`, `bool8`, `byte`, `zstring`) inside
  `bindecl { ... }` blocks; outside, they remain identifiers. The
  existing global `string` keyword is reused unchanged inside bindecl
  member position; no new lexer state is needed for it.
- Doc-comment claim/attach handling for `bindecl` (mirror of the
  `ATTACH_HASHDECL_DOC_COMMENT` macro at `ast_parser.ypp:204`).

**Grammar — `modules/astparser/src/ast_parser.ypp`:**

- New tokens: `TOK_BINDECL`, `BINDECL_IDENTIFIER` (and any
  `BINDECL_IDENTIFIER_OPENCURLY` form if literal-construction syntax is
  added in v2).
- New non-terminals: `bindecl_def`, `bindecl_attributes`,
  `bindecl_member`, `bindecl_attribute_list`, `bindecl_layout_attr`,
  `bindecl_member_attr_list`, `fixed_array_count`, `bitfield_width`.
- Production placement: `bindecl_def` slots into the same grammar
  positions as `hashdecl_def` (top-level decl, namespace decl,
  member-list-of-namespace) — see `ast_parser.ypp:573`, `:921`,
  `:1359` for the existing template.
- Destructor declarations for new non-terminals follow the
  `hashdecl_*` pattern at `:533–:535`.

**AST nodes — `modules/astparser/src/ast/declarations/`:**

- `ASTBindeclDeclaration.h` — mirror of `ASTHashDeclaration.h`. Holds
  the decl name, attribute list, member list, and source location.
  Inherits from `ASTDeclaration`.
- `ASTBindeclMemberDeclaration.h` — mirror of
  `ASTHashMemberDeclaration.h`. Holds member name, type, optional
  bitfield width, optional per-field attribute list, optional default
  value, doc comment.
- `ASTDeclarationKind.h` — new enum value `ADK_Bindecl`.
- `ASTNodeType.h` — new enum value `ANT_BindeclDeclaration` (and
  `ANT_BindeclMemberDeclaration` if separate visitor support is
  needed).

**Visitors / printers / searchers:**

- `AstPrinter.cpp` / `.h` — new `printBindeclDeclaration` and
  `printBindeclMemberDeclaration` mirroring the hashdecl printers.
- `AstTreeSearcher.cpp` / `.h` — visit hooks for the new node kinds so
  symbol search / hover / go-to-definition work.
- `CSTSearcher.cpp` / `.h` — concrete-syntax-tree searcher updates for
  the new tokens and node types.
- `AstTreePrinter.cpp` / `.h` — pretty-printer for the new nodes.

**Module exports — `modules/astparser/src/ql_ast.qpp`:**

- New constants for the bindecl kind (`ADK_Bindecl`) so Qore-side
  consumers (QLS) can dispatch on declaration kind.
- The `AstParser`/`AstTree`/`AstTreeSearcher` qclass APIs already
  accept abstract declaration nodes; no surface-API change.

**Test coverage:**

- `examples/test/qore/astparser/` (or wherever the astparser tests
  live) gets bindecl-specific cases: parse a simple bindecl, verify
  the AST structure, verify the printer roundtrip, verify symbol
  search finds the decl name and member names, verify hover info
  reports the resolved offset/width.

The astparser work is **not** optional — QLS and qore-doc are the user-
facing surfaces for new language constructs, and shipping `bindecl`
without astparser support means the IDE experience degrades silently
for any file using the new keyword.

---

## 6. Serialization

`bindecl` introduces three orthogonal serialization concerns. Each is
handled by an existing Qore mechanism extended for the new type, with
no new framework needed.

### 6.1 Data serialization — `Serializable` integration

A `bindecl` instance must round-trip through `Serializable` so that
programs can persist, transmit, or queue records the same way they do
hashes, lists, objects, and hashdecl instances today.

**`hash<SerializationInfo>` form** (the structured, self-describing
representation used by `Serializable::serializeToData()`):

```qore
{
    "_index": {
        "0": <hash<BindeclSerializationInfo>>{
            "_decl":   "Net::Icmp::IcmpEchoHeader",   # full namespace path
            "_module": "icmp",                         # module supplying the decl
            "_data":   <binary, 8 bytes>,              # the logical record span
        },
    },
    "_data": "0",
}
```

A new `BindeclSerializationInfo` hashdecl is added alongside the
existing `SerializationInfo`, `ObjectSerializationInfo`,
`HashSerializationInfo`, `IndexedObjectSerializationInfo` family
declared in `lib/QC_Serializable.qpp` and used by
`lib/QoreSerializable.cpp`. Members:

| Member       | Type     | Purpose                                                |
|---|---|---|
| `_decl`      | `string` | Fully-qualified namespace path of the `BindeclDecl`    |
| `_module`    | `*string` | Module that supplied the decl (NOTHING for in-program decls) |
| `_data`      | `binary` | Logical record-span bytes (exactly `<bindecl>::size()` bytes) |
| `_layout_hash` | `*int` | 64-bit layout fingerprint for drift detection (low 32 bits = computed hash, high 32 bits = 0; `0` is a legal hash value; `NOTHING` is allowed only when an explicit serializer/deserializer compatibility flag opts out) |

`QoreSerializable::serializeValue` gains a new dispatch arm for
`NT_BINDECL_INSTANCE` that calls a new
`serializeBindeclToData()` mirroring the existing
`serializeHashToData()` shape (`lib/QoreSerializable.cpp:639`).

`QoreSerializable::deserializeValue` gains a matching arm that:

1. Looks up the decl by `_decl` path in the program's namespace.
2. If the decl isn't found and `_module` is set, attempts to load that
   module first (same fallback the hashdecl deserializer already
   uses).
3. Verifies `_data.size() == decl->size()`.
4. Verifies `_layout_hash` matches the decl's computed layout
   fingerprint. If `_layout_hash` is `NOTHING` or absent, raises
   `BINDECL-LAYOUT-MISMATCH` unless the caller passed
   `SF_ACCEPT_UNFINGERPRINTED_BINDECL` (a new bit in the existing
   `Serializable` flag space — claim from the live `SF_*` set in
   `lib/QC_Serializable.qpp` at implementation time; do not pre-assign
   a value here).
5. Constructs the instance via the same `castExact(binary)` zero-copy
   path used at runtime.

**Binary stream form** (`Serializable::serialize(OutputStream)` /
`deserialize(InputStream)`): the bindecl is serialized as a tagged
record `(NT_BINDECL_INSTANCE, decl_path_str, layout_hash, byte_count,
bytes)`. The format is the same shape the existing serializer uses for
hashdecl instances, with the on-the-wire `bytes` being the bindecl
buffer verbatim — no per-field re-encoding.

**Compact-binary form** (new): for cases where the consumer already
knows the decl, implicit assignment to `binary` produces exactly the
record-span bytes. This is *not* `Serializable` — there's no
self-description — but it's the form most protocol code wants. The two
forms coexist; programmers pick based on whether the consumer is
type-aware.

**Layout fingerprint.** A 32-bit hash over `(decl_name, record_endian,
align, bitorder, total_size, ordered list of (member_name, type_code,
offset, width, endian, array_count, encoding, nested_decl_path,
nested_decl_fingerprint, bitfield_params))` catches accidental decl drift
between serializer
and deserializer. The record-level `endian` and `bitorder` axes are
included independently of per-field overrides because changing the
record-level default flips the bytes-on-disk meaning for any
non-overridden field.

The `nested_decl_fingerprint` axis is **load-bearing for nested-decl
drift detection.** Per §2.2, nested records are layout-stable by
declaration: a member of type `OtherDecl` embeds bytes per `OtherDecl`'s
own resolved layout. If a different version of `OtherDecl` (same FQ
path, different bytes-on-disk) is in scope at deserialization time,
the parent's `nested_decl_path` axis alone wouldn't change — only the
nested fingerprint would. Hashing the nested decl's fingerprint into
the parent's fingerprint propagates nested drift to every dependent
parent, so `BINDECL-LAYOUT-MISMATCH` fires at the outermost mismatch.
Nested-fingerprint resolution is a fixed point: a decl's fingerprint
depends on the fingerprints of every nested decl it transitively
embeds, so the compiler computes them in topological order at decl
finalization (the same order the AOT writer emits decls in §6.2).

Stored as a 64-bit signed Qore `int` whose low 32 bits hold the
computed hash; the high 32 bits are zero in v1, reserved for a future
widening to 64-bit. Numeric `0` is a legal hash value. `NOTHING` means
"fingerprint intentionally omitted" and is accepted only under an
explicit compatibility flag for trusted legacy/unfingerprinted payloads;
normal serialization always emits `_layout_hash`.

Computed once
at decl finalization, stored on `BindeclDecl`, included in `_layout_hash`
by default. Disabling it requires an explicit `Serializable` flag and
should be reserved for trusted compatibility paths, not normal use.

### 6.2 AOT serialization — QORD format extensions

The QORD wire format gets the additions documented below. All
extensions are gated by a single new feature flag bit; readers without
the flag refuse to load blobs that advertise it (existing behaviour
for forward-incompatible features).

**Feature flag bit.** `QORE_AOT_FEAT_BINDECL = 1ULL << 21` (verified
against `include/qore/intern/QoreAOTBinary.h:90–112`; bits 0–20 taken,
21 is the next free slot). `QORE_AOT_SUPPORTED_FEATURES` extends to
`0x3FFFFFULL`.

**New QORD section type — `BINDECL_DECL`.** The section id must be
claimed at implementation time from `QoreAOTSectionType`; in the current
tree (`include/qore/intern/QoreAOTBinary.h:115–137`), ids 1–21 are
taken (21 = `BUILD_INFO`), so the next candidate is **22**. Section
payload per declaration:

```
u32  decl_id_in_blob
str  fully_qualified_path                   # "Net::Icmp::IcmpEchoHeader"
str  module_name                            # NOTHING-marker if in-program
u32  total_size_bytes                       # static size
u8   layout_kind                            # 0 = static; v1 readers MUST reject any non-zero value
u8   record_endian                          # 0 = big, 1 = little, 2 = native, 3 = unset
u8   bitorder                               # 0 = msb-first, 1 = lsb-first, 2 = unset
u8   align                                  # 0 = packed, log2 byte alignment otherwise
u32  layout_hash                            # 32-bit fingerprint (see §6.1)
u32  member_count
member[member_count]:
    u32  member_id_in_decl                  # positional, matches declaration order
    str  member_name
    u16  type_code                          # see member-type table below
    u32  byte_offset
    u16  width_bits                         # for bitfields; 0xFFFF for non-bitfield
    u16  bit_offset_in_parent                # for bitfields; 0xFFFF otherwise
    u8   endian                             # 0 = big, 1 = little, 2 = native, 3 = inherit-from-record
    str  encoding                           # for string/zstring members; "" otherwise
    u32  nested_decl_ref                    # decl_id of nested bindecl, or 0xFFFFFFFF
    u32  array_count                        # 0 for scalar, >0 for fixed array
    u8   reserved_member_flags              # 0 in v1
```

**Forward-compatibility discipline for `layout_kind`.** v1 readers
reject any blob whose `layout_kind` is non-zero. Any v2 expansion
(variable-tail records, etc.) **must** be paired with a new feature
flag bit so v1 readers reject the whole blob via the existing
flag-mask check before they attempt per-section validation. This
guarantees that a v2 producer cannot feed garbage to a v1 reader by
exploiting the reserved field.

**Bitfield-run encoding.** Per §2.4, every member of a bitfield run
shares the same parent integer type (and therefore the same
`type_code` and computed parent-integer byte offset). v1 keeps the
per-member encoding above unchanged — the parent integer's width is
recoverable from each member's `type_code`, and the member's position
within the run is recoverable from `bit_offset_in_parent`. The
redundancy is a small constant per bitfield (≈ 20 bytes per member
record), and bitfield runs in real protocols rarely exceed 8 members
per parent integer. v2 may switch to a grouped encoding if profile
data justifies it; v1 does not.

**Member-type code table.** Stable numeric IDs assigned at language spec
time; never reordered. `0x01..0x10` cover scalar primitives
(`uint8`/`int8`/.../`float64`/`bool8`); `0x20..0x2F` cover composite
forms (`byte[N]`, `string[N,enc]`, `zstring[N]`, scalar arrays,
nested-decl, nested-decl-array). The table itself is part of the language
spec, not the QORD format.

A future member-type code whose on-disk member tuple shape is unchanged
can remain under `QORE_AOT_FEAT_BINDECL`, but older readers must reject
the declaration during `BINDECL_DECL` section loading before registering
the decl in any namespace or exposing it to the type system. The diagnostic
should be a clean "unknown bindecl member type code N" load error. Any
change that alters the per-member tuple shape, member-count semantics, or
section payload framing **must** claim a new feature flag bit.

**Decl ID stability across blobs.** `decl_id_in_blob` is per-blob and
is resolved at load time to a `const BindeclDecl*` via the same
namespace-path lookup the hashdecl loader uses
(`lib/QoreSerializable.cpp:653-666` shows the equivalent for
hashdecl). Cross-blob references use `(module_name, decl_path)` pairs,
not raw IDs.

**Member ID stability within a decl.** `member_id_in_decl` is
positional. **Reordering members is a breaking change** to the on-disk
format and the AOT QORD format both — same discipline as
`QoreIROpcode` ordering in
[`design/qore-ir-spec.md`](../design/qore-ir-spec.md). Adding a member
at the end is forward-compatible if the resulting size is legal under
the consumer's expectations; otherwise it's breaking. The layout
fingerprint catches accidental violations.

**Constants of bindecl type.** Compile-time constant bindecl values
(e.g. `const TCP_ACK_FLAG = TcpFlags::cast(<0x10>);` if v2 supports
literal binary syntax — out of scope for v1) serialize into the QORD
`CONSTANTS` section as `(decl_path, layout_hash, raw_bytes)` and
follow the same `aot_shell_pending` discipline that hashdecl constants
use. Specifically: the `ConstantEntry` is constructed with
`builtin=false, init=true`, and the runtime value is installed via
`ConstantEntry::setRuntimeValue()` so that `RuntimeConstantRefNode`
holders see a non-NOTHING `saved_val`. This is the same fix pattern
documented in memory entry
`session_2026_04_23_aot_logo_resource_fix.md` — bindecl constants
will hit the same hazard if implemented naively.

**IR opcode encoding.** `LoadBindeclField` and `StoreBindeclField`
serialize as:

```
opcode_id  u16
decl_id    u32                              # blob-local decl_id_in_blob
member_id  u32                              # member_id_in_decl
operand    u32                              # SSA id of bindecl-instance value
[value]    u32                              # SSA id of value (Store only)
result     u32                              # SSA id of result (Load only)
```

The verifier resolves `(decl_id, member_id)` against the loaded decl
table and refuses to execute the IR if any reference is unresolved.
The verifier extension is gated on `QORE_AOT_FEAT_BINDECL`.

**AOT writer side (`lib/QoreAOTBinary.cpp`).** A new
`writeBindeclDecl()` function follows the existing `writeHashDecl()`
pattern. The AOT writer must emit decls in dependency order — a decl
that contains a nested decl as a member must follow that nested decl
in the section payload — to match the loader's single-pass resolution.
Same constraint hashdecl already obeys.

**AOT reader side (`lib/QoreAOTRuntime.cpp` / `QoreAOTBinary.cpp`).**
A new `readBindeclDecl()` function constructs `BindeclDecl` objects in
the same order, populates the decl table, and registers each decl
into the program's namespace. Cross-blob nested-decl references are
resolved during a fix-up pass after all decls in the blob have been
constructed.

**Cross-version compatibility.** Old runtimes loading new blobs:
fail-clean via the feature flag check (this is the existing forward-
incompatibility behaviour). New runtimes loading old blobs: the new
`BINDECL_DECL` section is simply absent and the feature flag is
unset; no behaviour change. The QORD blob version number does *not*
change; only the feature flag mask widens.

### 6.3 AST persistence — astparser tree serialization

The astparser module produces an in-memory AST that QLS, qore-doc, and
diagnostic tools traverse. There's no on-disk AST persistence today
beyond the source itself, so "AST persistence" is really "the AST node
classes round-trip through the printer and re-parse cleanly":

- `AstPrinter::printBindeclDeclaration` (added per §5.7) must emit
  source that re-parses to a structurally-identical AST.
- `AstTreePrinter` (the node-shape printer used for diagnostics) must
  cover the new node types so issue reports are not silent.
- The CST (concrete-syntax-tree) printer used by reformatting tooling
  must preserve attribute ordering — `[endian=big, align=packed]` and
  `[align=packed, endian=big]` are semantically identical but produce
  different bytes; the reformatter should normalise to attribute
  alphabetical order.

No new persistent AST file format and no serialized AST ID assignment.
The in-memory astparser enums still gain ordinary source-level values
such as `ADK_Bindecl` and `ANT_BindeclDeclaration` per §5.7.

**Enum-stability discipline.** QLS and qore-doc consume the astparser
enum values across protocol boundaries (Language Server JSON, hover
APIs). New enum values must be **appended at the end** of their parent
enum — never inserted mid-list, never reordered — so existing
serialized values keep their meaning. Same discipline as
`QoreIROpcode` ordering in
[`design/qore-ir-spec.md`](../design/qore-ir-spec.md). `ADK_Bindecl`
and `ANT_BindeclDeclaration` therefore append after the current last
member of `ASTDeclarationKind` and `ASTNodeType` respectively.

### 6.4 Cross-format consistency invariant

The decl identity used across all three serialization axes (data,
AOT, AST) is **the fully-qualified namespace path** plus the
**layout fingerprint**. A bindecl referenced from any of the three
formats resolves to the same `BindeclDecl*` at runtime, and a layout
mismatch on any axis raises a uniformly-named exception
(`BINDECL-LAYOUT-MISMATCH`) with format-specific context attached.

This is the same identity model `TypedHashDecl` already uses; no new
abstraction.

---

## 7. Binary compatibility and AOT discipline

`bindecl` is a new declaration kind, not a modification to existing
types, so all changes are additive:

- New AOT QORD section type — old loaders reject the section as
  unknown, which is exactly the existing forward-compat behaviour
  (see §6.2).
- New IR opcodes — existing modules don't emit them, so old binaries
  and new binaries coexist.
- New pseudo-class — fresh namespace, no collisions.
- New keywords — context-sensitive per §5.7 / §10.6; gated by
  `PO_NO_BINDECL` / `%no-bindecl` for older sources (see §7.1).

The decl serialisation in QORD is deterministic: a `bindecl` parsed
from identical source text produces an identical byte representation
(modulo the `layout_hash`, which is also deterministic), so
cross-module dependency tracking works the same way as for
`hashdecl`.

### 7.1 Parse option for backward compatibility

Every new keyword and lexer construct introduced by this proposal gets
a matching parse option that disables it for older sources. This
follows the same pattern documented in the `char` design
(`char-type.md` §7.2) and is required because user code may have
identifiers named `bindecl` or any of the primitive type keywords.

| Parse directive | Parse option flag (C++) | Disables                                        |
|---|---|---|
| `%no-bindecl`   | `PO_NO_BINDECL`         | The `bindecl` keyword, `bindecl` declarations, the context-sensitive primitive type keywords (`uint8`/`int8`/.../`zstring`), the `<bindecl>` pseudo-class lookup, and the `Name::cast(...)` / `Name::castExact(...)` factory forms for bindecl types |

When `PO_NO_BINDECL` is set on a Program:

- The lexer emits `IDENTIFIER` tokens for `bindecl` and the primitive
  type names — they are never elevated to keywords, even inside
  contexts where they would otherwise become context-sensitive.
- Parsing a `bindecl` declaration is a parse error
  (`PARSE-ERROR: 'bindecl' declarations are disabled by
  PO_NO_BINDECL`).
- Loading an AOT module that advertises `QORE_AOT_FEAT_BINDECL` into a
  Program with `PO_NO_BINDECL` is a load error
  (`MODULE-LOAD-ERROR: module requires bindecl support disabled by
  PO_NO_BINDECL`). The load fails before any global state is touched.
- A bindecl instance arriving via `Serializable::deserialize` from an
  outside source raises `DESERIALIZATION-ERROR` rather than silently
  materialising; the program has explicitly opted out of the type
  system support.

The parse-option flag value comes from the parse-option bit space
documented in `include/qore/Restrictions.h` and represented by
`include/qore/QoreParseOptions.h`. A free bit must be claimed at
implementation time; do not pre-assign a value here.

In the common case the opt-out is not needed — the additive nature of
the change plus context-sensitive lexing means existing code keeps
working unchanged. The flag exists for the rare program that uses one
of the new identifiers as an existing variable or function name.

`%enable-bindecl` is **not** introduced; the feature is on by default
(decision #1 in §12 deferred this and recommended no gate, and
the parse option provides the necessary opt-out for compatibility).

---

## 8. Sandboxing

`bindecl` itself is a pure-language feature — no domain. Member access
is no more privileged than hash access.

The interesting interactions are:

- **`cast(binary, offset)` from an attacker-controlled `binary`** could
  allow a sandboxed program to interpret hostile bytes as a record with
  declared offsets. This is fine — the logical span is bounded by the
  source `binary` size, and out-of-range casts fail before member access.
  Layout fingerprint mismatch (per §6.1) catches drift between serializer
  and deserializer at the point of construction.
- **FFI** is `QDOM_UNCONTROLLED_API` per §3 for arbitrary user-declared
  imports. Narrower domains are only appropriate for audited wrapper
  modules with fixed imports; the layered syscall/library access is what
  gates dangerous behaviour, not the layout descriptor.
- **`bindecl` for shared-memory IPC** with another process must
  account for the peer being able to mutate the buffer concurrently;
  this is the same hazard as today's shared-memory APIs and is
  addressed at the IPC API level, not at the language level.

---

## 9. Performance projections

Numbers in this section are projections, not measurements; concrete
benchmarks must come from the implementation.

For a typical packet-parsing loop reading 8 fields out of a 40-byte
header:

| Path                                              | Approx ops per field |
|---|---|
| Today, `<binary>::getByte` + manual shift in Qore | ~6 boxed ops, ~2 allocs per field         |
| Today, hashdecl via C++ module                    | 1 boxed hash lookup + 1 unbox per field   |
| Proposed `bindecl`, IR-interpreted                | 1 typed load (no box, no alloc) per field |
| Proposed `bindecl`, JIT-compiled                  | 1 LLVM `load i32` + optional `bswap`      |

The expected ratio against today's pure-Qore path is one order of
magnitude on hot loops; against the C++-module path, the gain is
smaller (we save the boxing and the dispatch into the module) but the
*development cost* is the place this design pays off, not the runtime
cost.

Memory overhead per instance is the wrapper only: `BinaryNode*` (8) +
byte offset (4) + byte length (4) + `const BindeclDecl*` (8) +
refcount (8) ≈ 32 bytes plus allocator overhead. The record bytes
themselves are not copied.

For high-frequency parsing (e.g. one cast per packet on a hot
network connection) the per-cast wrapper allocation is the real
cost, not the wrapper bytes. A future optimization can pool
`BindeclInstance` wrappers per-thread; v1 does not need it.

---

## 10. Risks and tradeoffs

### 10.1 Decl scope creep

`bindecl` overlaps superficially with `hashdecl` and could grow toward
"another data type with members". The design guardrail is that
**every v1 member of a `bindecl` has a concrete bit position and the
record has a constant size**. Anything that breaks that invariant —
pointers, references, `auto` members, unbounded strings, and
runtime-sized fields — does not belong in v1 `bindecl`. Programmers
needing semantic records should use `hashdecl` or a class; programmers
needing C pointer arguments should use the future FFI type layer.

### 10.2 Endianness as a footgun

There is no `host` endian — only `big`, `little`, and `native`.
`native` is a runtime-resolved marker: a record declared
`endian=native` records "follows host byte order at instance time" in
the QORD blob, so an AOT artifact compiled on x86 and loaded on a
big-endian host produces the right bytes-on-disk layout for *that*
host. `big` and `little` are baked, deterministic, and portable.

v1 requires **explicit `endian=`** on either the record or every
multi-byte field — there is no implicit default. This forces the
programmer to think about it. The cost is verbosity for the rare
native-layout case (FFI to C structs), which is exactly when the
programmer *should* be confronting the question anyway.

### 10.3 Bitfield ordering

Bitfield ordering varies between platforms and protocols. Network specs
often use MSB-first field diagrams, while C bitfields on common x86 ABIs
are typically LSB-first. v1 therefore requires `bitorder=` whenever any
bitfields are present; omitting it is a parse error, not a warning.

### 10.4 Mutation through aliasing

`Name::cast(binary, offset)` shares storage with its source binary, the
same way `binary` itself shares storage when copied. The instance views a
logical span inside that binary. Mutation through a member store triggers
the underlying binary's COW path: if the source binary is shared
(refcount > 1), the COW splits storage before the write lands. Other
holders of the source binary see no change. This is the same hazard model
as `<binary>::splice` today; only the span offset is new.

The two cases worth attention:

- The source binary is held only by the bindecl instance (refcount = 1).
  Mutation lands in place inside the logical span — zero copy, fast path,
  the dominant workload.
- The source binary is shared with a non-bindecl holder. First
  mutation triggers COW; subsequent mutations stay in place because
  the bindecl now owns the only reference. The first-mutation cost is
  one buffer copy.

The pseudo-class exposes `<bindecl>::isShared()` so library code can
defensively check before bulk mutations where a one-shot upfront
`clone()` is cheaper than per-write COW.

### 10.5 Native ABI layout is not just alignment

FFI use cases are the most likely place for users to over-trust
`bindecl`. `align=8` and `endian=native` are not enough to prove C layout
compatibility: C field sizes, padding, bitfield allocation, pointer
widths, and typedefs such as `long`, `time_t`, and `socklen_t` are ABI-
specific. The follow-up FFI module must provide ABI aliases and layout
validation helpers; this design should not claim that a hand-written
`bindecl` is automatically a correct C struct.

### 10.6 Keyword collisions

Adding `bindecl`, `uint8`/`int8`/etc. as keywords would break user
code that has identifiers with those names. Two-layer mitigation:

1. **Context-sensitive lexing.** The primitive type names (`uint8`,
   `int8`, ..., `zstring`) are recognized as keywords *only* inside
   `bindecl { ... }` blocks; outside that context they are normal
   identifiers. This requires a lexer state flag set on `{` after
   `TOK_BINDECL <name> <attrs>` and cleared on the matching `}`. Same
   class of trick used elsewhere in the lexer for limited contextual
   recognition.
2. **`PO_NO_BINDECL` parse option (§7.1).** Programs that have
   `bindecl` itself as an identifier (the only keyword that *isn't*
   context-sensitive) opt out of the entire feature. The flag is
   checked at the lexer level so the keyword is never elevated.

The verification step before merging the parser changes:

1. Re-run the full Qore test suite.
2. Concrete keyword-collision scan:

   ```
   git grep -nwE 'bindecl|uint8|int8|uint16|int16|uint32|int32|uint64|int64|float32|float64|bool8|byte|zstring' \
       -- '*.q' '*.qm' '*.qc' '*.qpp' '*.qtest' \
       | grep -vE '^[^:]+:[0-9]+:\s*#'
   ```

   The output is finite and reviewable. Any user-module identifier
   that collides becomes a release-note migration item with the
   `%no-bindecl` workaround. (`bindecl` itself is not
   context-sensitive; the primitive type keywords are, so collisions
   outside `bindecl { ... }` blocks are harmless.)

### 10.7 AOT decl dependency surface

A `bindecl` referenced from another module participates in AOT
dependency tracking the same way a `hashdecl` does today. The known
hazards from the current AOT machinery (constant pending, transplant,
fallback) also apply to `BindeclDecl` and are deferred to
implementation review against the relevant memory entries
(`session_2026_04_23_aot_logo_resource_fix.md`,
`session_2026_04_23_aot_test_migration.md`,
`session_2026_04_23_hashdecl_nested_retype.md`).

---

## 11. Effort estimate

| Component                                                                 | Estimate (sequential) |
|---|---|
| Lexer changes (keyword + context-sensitive primitives + `PO_NO_BINDECL`)  | 4-5 days               |
| Parser + layout pass (offsets, bitfields, fixed arrays, attribute validation, fingerprint) | 1-2 weeks    |
| Type checker (bindeclTypeInfo, member access, constructor + cast forms)   | 1-2 weeks              |
| Runtime (BindeclDecl, span-aware BindeclInstance, NT_BINDECL_INSTANCE node type, COW mutation checks) | 1-2 weeks  |
| IR opcodes (Load/StoreBindeclField + verifier)                            | 1 week                 |
| JIT / AOT lowering (LLVM emit, QORD section, feature flag bit 21 / next section id) | 2 weeks                |
| **Serialization** — `Serializable` integration + `BindeclSerializationInfo` hashdecl + writer/reader hooks | 1 week        |
| **AOT serialization** — QORD `BINDECL_DECL` section writer/reader, layout fingerprint, decl path resolution, constant-pending discipline | 1-2 weeks |
| **astparser module mirror** — scanner/grammar/AST nodes/printers/searchers | 1-2 weeks              |
| `<bindecl>` pseudo-class + reflection module                              | 4-5 days               |
| Test coverage (qtest suite — layout, fixed arrays, write semantics, span casts, bitfields, endian, AOT, Serializable, astparser, parse-option) | 2-3 weeks    |
| Doc updates (Doxygen, language guide, release notes)                      | 4-5 days               |
| **Total (sequential)**                                                    | **~12-15 weeks**       |
| **Total (parallel, 2 developers)**                                        | **~8-10 weeks**        |

The FFI module is a **separate design** with its own effort estimate;
it is not counted here. The bindecl-side integration shim it consumes
(stable C-pointer view + keepalive, sub-record span exposure) is
already within this estimate as part of the runtime line item. The
full FFI module — libffi integration, ABI aliases and layout
validation, calling convention testing across x86_64 / aarch64 /
arm32, callback trampolines, pointer lifetime rules, sandbox
plumbing — and the `linux-syscall` submodule both belong in their
own design-pending file.

This estimate is independent of, and complementary to,
[`plugin-types-and-dense-data.md`](plugin-types-and-dense-data.md).

The estimate is lower than the broader draft because v1 now excludes
variable-length tails and the full FFI module. It is still above the
earliest sketch because the review pass surfaced three areas that were
under-scoped: full `Serializable` round-trip (including the
layout-fingerprint discipline and cross-module decl resolution); QORD
serialization in the level of detail the AOT loader actually needs; and
the astparser module mirror, which is a hard requirement per the project
guideline. Variable-length tails are a follow-up effort estimated at
**2-4 weeks** once the static-layout machinery has landed.

### 11.1 Acceptance test matrix

The v1 implementation is not complete without targeted qtests covering:

- Static layout: computed offsets, `offset=` / `size=` assertions,
  namespace lookup, nested records whose own attributes are not inherited
  from the parent, parent-`align`-driven nested span placement,
  fixed scalar/nested arrays, and direct/indirect self-reference
  rejection (`BINDECL-SELF-REFERENCE`).
- Cast spans: `cast(binary)`, `cast(binary, offset)`, `castExact(...)`,
  undersized input, oversized input, negative/out-of-range offsets, and
  implicit binary conversion returning only the logical span.
- Mutation and COW: writes through a unique span, writes through a shared
  source binary, mutable FFI-pointer preparation materializing a unique
  span before exposing `void*`, read-only pointer access avoiding COW,
  `clone()`, and `isShared()`.
- Endian and bitfields: explicit endian enforcement, `native` round-trip,
  MSB/LSB bitfield reads, mixed-width bitfield run rejection,
  mid-run `offset=` rejection, first-member `offset=` acceptance pinning
  the parent integer's byte offset, signed bitfield sign-extension,
  single-byte run ignoring `endian=`, and bitfield read-modify-write
  ordering.
- Field writes: integer range failures, bool normalization, exact
  `byte[N]` length, string encoding/truncation errors, `zstring[N]`
  termination, and fixed-array whole-field/per-element writes.
- Conversion and I/O contexts: assignment/return/argument conversion to
  `binary`, exact bindecl overload preference, and no implicit conversion
  in generic `auto`, concatenation, arithmetic, or equality contexts.
- Serialization: `Serializable::serializeToData()`, binary stream
  serialization, layout fingerprint mismatch, **nested-decl drift
  detection via `nested_decl_fingerprint`** (parent fingerprint
  changes when a transitively-nested decl changes layout, even when
  the parent's own fields are untouched), missing/`NOTHING`
  `_layout_hash` rejection unless `SF_ACCEPT_UNFINGERPRINTED_BINDECL`
  is set, missing decl/module resolution, one-way conversion
  enforcement (`binary → bindecl` parameters parse-error without
  explicit `cast`), and `PO_NO_BINDECL` rejection.
- AOT: QORD decl emission/load, feature-flag rejection on old runtimes,
  unknown member-type-code rejection during decl-section loading,
  constants of bindecl type, nested-decl dependency order, and IR
  Load/Store opcode verification.
- Tooling: astparser parse tree, printer round-trip, search/hover for
  decls and members, parse-option opt-out, and docs generation.

---

## 12. Decisions log and remaining open questions

### Decisions made during review

The following design questions were posed during review and have been
decided. They are recorded here as a quick reference; the substantive
text lives in the indicated sections.

| # | Question                                                           | Decision                                                  | Section |
|---|---|---|---|
| 1 | Initial parse-option gate (`%enable-bindecl`)?                     | No gate; `%no-bindecl` provides opt-out                  | §7.1     |
| 2 | Variable-length tail records in v1?                                | Defer to v2; sketched in §12.9                            | §2.2     |
| 3 | Unbounded `zstring`?                                               | Defer to v2 with variable tails                           | §2.2     |
| 4 | `reinterpret<T>` operator between same-size decls?                 | Defer to v2; workaround uses binary→cast                  | §12.7    |
| 5 | JSON-style debug printing                                          | Include `<bindecl>::toHash()` and `toString()` in v1      | §2.5     |
| 6 | `Serializable` / `BinaryInputStream` integration                   | Explicit `Serializable` support in v1                     | §6.1     |
| 7 | Endian-default policy                                              | Explicit-required, no implicit default                    | §10.2    |

### 12.7 `reinterpret<T>` workaround performance

The v1 workaround for type-punning between same-size decls is a binary
round-trip:

```qore
binary tmp = a;                   # zero-copy span share
B b = B::cast(tmp);               # one new BindeclInstance wrapper
```

This costs one extra `BindeclInstance` allocation and one extra span
resolution per type-pun (the underlying `BinaryNode` is shared
throughout — no buffer copy). On hot paths a future
`reinterpret<T>` would replace this with a single in-place
re-tagging; until then, code that type-puns frequently (e.g.
multi-version protocol parsers) should hoist the `cast` out of the
inner loop where possible.

### 12.8 Open question — full FFI ABI type mapping

Deferred to the FFI design. `bindecl` must stay compatible with native
ABI validation, but it should not itself define C typedef aliases,
pointer lifetimes, callback trampolines, varargs, or struct-by-value
calling rules.

### 12.9 v2 sketch — variable-length tail records

Documented here so the v2 work has a starting point and the v1 design
constraints (`byte_length == size()` invariant, `layout_kind = 0`,
forward-compat feature flag) are the right shape to extend.

Likely future syntax — a member whose length is determined by another
in-record integer member:

```qore
bindecl ZipLocalFileHeader [endian=little, bitorder=lsb-first] {
    uint32 signature;
    # ...
    uint16 filename_length;
    uint16 extra_field_length;
    byte   [length=filename_length]      filename;
    byte   [length=extra_field_length]   extra_field;
}
```

Constraints:

- The length-source member must precede the length-driven member in
  declaration order, and must be of an unsigned integer type.
- Length-driven members must form a contiguous tail; a fixed-position
  member after a length-driven one is a parse error.
- The compiler emits a runtime size formula and runtime offset
  formulas for any subsequent length-driven members.
- `<bindecl>::size()` becomes a small calculation rather than a
  constant; the verifier marks it side-effect-free but not constant.
- Construction must define whether length-source fields are immutable,
  whether changing them resizes the underlying buffer, and whether
  variable-tail records can be default-constructed without explicit
  tail sizes.
- `Name::cast(binary, offset)` evaluates the size formulas against
  the actual bytes; mismatch raises `BINDECL-SIZE-MISMATCH` with the
  computed expected size.
- AOT QORD `layout_kind = 1` plus a new `QORE_AOT_FEAT_BINDECL_VARTAIL`
  feature flag bit so v1 readers reject v2 blobs cleanly via the
  flag-mask check (per §6.2).

---

## 13. References

- [`plugin-types-and-dense-data.md`](plugin-types-and-dense-data.md) —
  the dense-buffer plugin-type proposal whose substrate this design
  reuses
- [`design/hashdecls-cpp.md`](../design/hashdecls-cpp.md) — symmetric
  pattern for typed records on the boxed-hash path
- [`design/qore-jit-aot-current-state.md`](../design/qore-jit-aot-current-state.md)
  — IR / JIT / AOT pipeline this design lowers into
- [`design/qore-ir-spec.md`](../design/qore-ir-spec.md) — IR semantic
  contract that the new opcodes must honour
- [`design/module-sandboxing-audit-guide.md`](../design/module-sandboxing-audit-guide.md)
  — sandboxing model the FFI module follows
- `lib/Pseudo_QC_Binary.qpp` — existing `<binary>` pseudo-class used by
  implicit bindecl-to-binary span assignment
- `lib/parser.ypp`, `lib/scanner.lpp` — parser/lexer touch points
