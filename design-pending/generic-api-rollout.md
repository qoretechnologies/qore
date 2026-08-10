# Generic API Rollout — Remaining Work

**Status:** Pending. The generic class type feature itself is implemented and shipped; see
[`design/generic-class-types.md`](../design/generic-class-types.md) for the design, the runtime model,
and the conversions already made across core, qlib, and the bundled `modules/*` tree.

Two pieces of work remain. Neither blocks the feature.

---

## 1. External module repositories

Core, qlib and every bundled binary module have been reviewed and converted or explicitly deferred, with
the decisions recorded in `design/generic-class-types.md`. What has not been reviewed are the
`~/src/qore/git/module-*` repositories delivered alongside Qore.

Prioritize iterator, cursor and message APIs — those are where one stable logical value type actually
flows through an object:

- `json`/`ndjson`, `yaml`, `xml`/`sax`
- `geos`, `openldap`, `mongodb`
- `zip`/`tar` streams, `grpc` streams
- `nats`/`amqp`/`zmq` messaging
- database driver cursors and result records

Per module family, in its own commit:

- [ ] Convert one family at a time, guarded by `QORE_HAVE_GENERIC_CLASSES` or an equivalent configure
      check wherever the module must still build against plain `develop`.
- [ ] Update the module's doxygen examples, release notes, generated `.meta.json`, and focused tests.
- [ ] Apply `audit-changes` before committing.

The conservative migration procedure in the *Binary Module Rollout Guidance* section of
`design/generic-class-types.md` applies unchanged — in particular, prefer `auto` over a type parameter
that would misrepresent a heterogeneous payload, and use `[legacy_raw]` only for public classes that
already existed as raw APIs.

## 2. Source-stripped AOT native generic specialization — deferred by decision

Source-stripped AOT preserves generic metadata and resolves it at run time rather than emitting a
separate native entry point per concrete type-argument tuple.

**This was measured and declined on 2026-05-19**, and the reasoning, the benchmark result and the
threshold that would reopen it are recorded in the *Known Limits* section of
`design/generic-class-types.md`. Source-stripped AOT is currently at or faster than IR/JIT/tiered on
every focused kernel, so there is nothing to buy.

The design work below is kept only so that it does not have to be re-derived if future benchmark data
ever crosses that threshold. **Do not implement it without new measurements that do.**

Design questions to settle first:

- [ ] Specialization keys for AOT native entry points: source body id, receiver class type arguments,
      method/function type arguments, and any parameterized hashdecl paths the lowered body needs.
- [ ] Whether specialized entry points are emitted eagerly from known source call sites, lazily from a
      load-time/runtime cache, or both.
- [ ] Code-size controls: per-body specialization limits, fallback to the generic metadata path, and
      diagnostics or counters for skipped specializations.
- [ ] Source observability: stack traces, reflection, profiling names and error locations must continue
      to identify the *source* method or function, not the specialization.

Implementation:

- [ ] Extend AOT metadata to record specialization descriptors without changing source-visible type
      identity.
- [ ] Teach qcc/AOT lowering to substitute concrete type arguments into native bodies using the same
      rules as IR/JIT specialization.
- [ ] Add a dispatch path that selects a matching specialized native entry point and falls back to the
      generic runtime metadata path when no match exists.
- [ ] Preserve compatibility with old AOT artifacts and with modules built without generic support.
- [ ] Correctness tests for source-stripped modules with mixed instantiations, nested generic hashdecls,
      static generic methods, and the fallback path.
