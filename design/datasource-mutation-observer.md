# Datasource Mutation Observer Design

## Overview

This document describes the opt-in, structured mutation observer API on `Qore::SQL::Datasource` and
`Qore::SQL::DatasourcePool`.  The API provides a datasource/pool-scoped boundary at which a consumer
can observe — and, at defined admission points, reject — storage mutations issued through a
datasource, without the core performing any classification of SQL text, database error text, or
command tags.

The motivating consumer is a managed multi-tenant storage admission policy (Qorus), but the
mechanism is policy-neutral: the core supplies identity, boundaries, and outcomes only.  No quota,
pricing, threshold, or retention semantics exist anywhere in this design.

Tracking issue: qoretechnologies/qore#5384.

## Design constraints

These are contract requirements, not preferences.  Each is testable.

1. **No classification in core.**  The core never inspects SQL text, never matches error strings,
   and never reads command tags to decide what an operation does.  All semantic metadata is
   producer-supplied.
2. **No process-global singleton.**  Observer registration and all associated state are scoped to a
   `Datasource` or `DatasourcePool` object.
3. **Zero-observer fast path.**  A datasource with no observer and no declaration retains today's
   behavior exactly and costs one predictable null pointer test per operation.
4. **Deterministic boundaries.**  Every operation produces a defined sequence of events for both
   autocommit and explicit transactions, covering known commit, known rollback, driver exception,
   restartable failover, and commit ambiguity.
5. **No silent outcome conversion.**  An unknown outcome is never reported as a rollback.  An
   observer exception never changes the database outcome or the transaction state.
6. **Structural read/mutation separation.**  Reads are distinguishable from mutations, and
   no-growth/reclaim declarations from growth declarations, before any enforcing consumer attaches.
7. **Replay safety.**  Operation identity survives a restartable transaction replay, and the core
   reports whether it can structurally prove that no commit was attempted.
8. **Streaming without buffering.**  Bounded streams expose declared and consumed byte counts; the
   core never buffers a payload to measure it.

## Model

The API has two independent halves, both scoped to a datasource or pool:

- the **producer**, which declares metadata describing what an operation is about to do;
- the **consumer** (the observer), which receives boundary events and returns admission decisions.

Neither half requires the other.  A declaration with no observer is inert.  An observer with no
declaration still receives every boundary, with `declared` set to `False` — which is what lets a
consumer reject undeclared mutations rather than silently permitting them.

### SqlMutationContext

Both halves share one refcounted `SqlMutationContext` per datasource or pool
(`include/qore/intern/SqlMutationContext.h`).  It holds:

- the observer call reference, its event mask, and its registration argument;
- a per-thread declaration stack;
- a per-thread re-entrancy flag;
- the transaction identity counter for the datasource or pool.

`qore_ds_private::mutation_ctx` is `nullptr` until something is registered; the context is created
lazily.  This is the fast path: an unmonitored datasource performs one null test per operation and
does no further work.

A `DatasourcePool` owns one context and hands a reference to every pooled `Datasource`, and stores it
in `DatasourceConfig` so that lazily-created connections inherit it.  This mirrors the existing
`DatasourcePool::setEventQueue()` fan-out.  Sharing a single context across the pool is what makes a
per-thread declaration survive the `getDS()`/`freeDS()` churn that occurs inside a transaction, and
it is what keeps two pools in the same process completely independent.

A `Datasource` copy shares the context by reference, consistent with how the DBI event queue is
copied.

### Producer: declarations

A producer declares metadata with an RAII scope object.  The declaration is pushed onto the
per-`(context, thread)` stack on construction and popped deterministically on scope exit, so
statements issued far below the declaration site — inside `SqlUtil`, `BulkSqlUtil`, or any other
layer — inherit it with no call-site changes anywhere in between.

```qore
{
    SqlMutationDeclaration decl(dsp, <SqlMutationInfo>{
        "op_id": op_id,
        "path_id": "orders.insert",
        "op_code": OP_INSERT,
        "effect": SQL_MUTATION_EFFECT_MAY_GROW,
        "max_growth_bytes": 1048576,
    });
    table.insert(row);
    dsp.commit();
}   # popped here
```

Declarations nest; the innermost declaration in the calling thread is the one reported.

#### `hashdecl Qore::SQL::SqlMutationInfo`

| key | type | meaning |
|---|---|---|
| `op_id` | `string` | stable operation/reservation identity; preserved across replay |
| `path_id` | `string` | reviewed managed-path identity |
| `op_code` | `int` | producer's closed operation code |
| `effect` | `int` | `SQL_MUTATION_EFFECT_NONE`, `_MAY_GROW`, or `_RECLAIM_ONLY` |
| `max_growth_bytes` | `*int` | positive upper bound on growth; only with `MAY_GROW` |
| `reclaim_audit_id` | `*string` | reclaim audit identity; only with `RECLAIM_ONLY` |
| `attempt_id` | `*string` | producer attempt identity for replay correlation |
| `attempt` | `*int` | attempt number, >= 1 |
| `tags` | `*hash<auto>` | opaque producer extension data, propagated unchanged |

The core validates only structure, and raises `SQL-MUTATION-DECLARATION-ERROR` on violation:

- `op_id` and `path_id` must be present and non-empty;
- `effect` must be one of the three closed values;
- `max_growth_bytes`, if present, must be > 0 and requires `effect == SQL_MUTATION_EFFECT_MAY_GROW`;
- `reclaim_audit_id`, if present, must be non-empty and requires
  `effect == SQL_MUTATION_EFFECT_RECLAIM_ONLY`;
- `attempt`, if present, must be >= 1.

The core assigns no meaning to `op_code`, `path_id`, or `tags`; it does not interpret
`max_growth_bytes` beyond reporting it.

### Consumer: observers

```qore
ds.setMutationObserver(\obs.event(), SQL_MUTATION_MASK_DEFAULT, arg);
ds.clearMutationObserver();
bool active = ds.hasMutationObserver();
```

The observer parameter is strongly typed as
`code<*hash<SqlMutationDecisionInfo>(hash<SqlMutationEventInfo>)>`, so a callback with an
incompatible signature is rejected at parse time.

#### Event masks

| constant | selects |
|---|---|
| `SQL_MUTATION_MASK_TX` | transaction begin and transaction outcome events |
| `SQL_MUTATION_MASK_EXEC` | pre/post execution of mutating statements |
| `SQL_MUTATION_MASK_READ` | pre/post execution of select and describe operations |
| `SQL_MUTATION_MASK_STREAM` | bounded stream begin/progress/end |
| `SQL_MUTATION_MASK_DEFAULT` | `TX | EXEC | STREAM` |
| `SQL_MUTATION_MASK_ALL` | `TX | EXEC | READ | STREAM` |

Reads are excluded by default because a storage admission consumer does not need them; they remain
available so that reads are *distinguishable* from mutations rather than invisible.

## Boundaries

### Statement classes

The statement class is derived structurally from which core entry point was used, never from the
SQL:

| constant | entry points |
|---|---|
| `SQL_STMT_CLASS_READ` | `Datasource::select()`, `selectRow()`, `selectRows()`, the typed and columnar variants, `describe()` |
| `SQL_STMT_CLASS_EXEC` | `Datasource::exec()`, `execRaw()` |
| `SQL_STMT_CLASS_STMT_EXEC` | `SQLStatement::exec()`, `SQLStatement::describe()` |
| `SQL_STMT_CLASS_BEGIN` | explicit and implicit transaction start |
| `SQL_STMT_CLASS_COMMIT` | commit, including autocommit |
| `SQL_STMT_CLASS_ROLLBACK` | rollback |
| `SQL_STMT_CLASS_STREAM` | bounded stream operations |

### Event sequence

Autocommit statement:

```
PRE_EXEC  -> POST_EXEC(OUTCOME_OK) -> OUTCOME(COMMIT)
```

Explicit transaction:

```
TX_BEGIN
PRE_EXEC  -> POST_EXEC(OUTCOME_OK)
PRE_EXEC  -> POST_EXEC(OUTCOME_OK)
OUTCOME(COMMIT | ROLLBACK | COMMIT_AMBIGUOUS | ...)
```

`PRE_EXEC` is emitted **before** any implicit transaction is started, so a rejection can never leave
an open transaction behind.

### Outcome classification

All outcomes are derived from control flow and from a `commit_in_progress` flag maintained around
the driver `commit` call.  No error text is examined.

| situation | outcome | `replay_safe` |
|---|---|---|
| statement returned, transaction still open | `SQL_MUTATION_OUTCOME_OK` | n/a |
| driver raised, connection intact | `SQL_MUTATION_OUTCOME_ERROR` | n/a |
| commit returned 0 with no exception | `SQL_MUTATION_OUTCOME_COMMIT` | `False` |
| commit raised, or the connection was lost during commit | `SQL_MUTATION_OUTCOME_COMMIT_AMBIGUOUS` | `False` |
| rollback returned 0 | `SQL_MUTATION_OUTCOME_ROLLBACK` | `True` |
| rollback raised | `SQL_MUTATION_OUTCOME_ROLLBACK_ERROR` | `True` |
| connection lost or aborted with no commit in flight | `SQL_MUTATION_OUTCOME_LOST_CONNECTION` | `True` |
| admission rejected the operation or the stream | `SQL_MUTATION_OUTCOME_NOT_EXECUTED` | `True` |

A commit that raises is **always** ambiguous.  It is never reported as a rollback, because the core
cannot know whether the server applied it.  This is the single most important rule in this design: a
consumer that released a reservation on a false rollback would undercount storage permanently.

`replay_safe` is `True` only when the core can structurally prove that no commit was attempted.  A
consumer replaying an operation may therefore reuse the reservation associated with `op_id` when
`replay_safe` is `True`, and must not create a second reservation when it is `False`.

### Transaction identity

The core assigns `tx_id`, unique within the process for each transaction on a datasource or pool, and
`tx_seq`, the sequence number of the event within that transaction.  `tx_id` is *not* stable across a
replay — a replay is a new transaction.  Stability across replay is carried by the producer's
`op_id`, which the core propagates unchanged.

## Observer semantics

### Admission events

`PRE_EXEC`, `STREAM_BEGIN`, and `STREAM_PROGRESS` are admission points.  Everything else is a
notification.

- Returning `NOTHING`, or a hash with `admit` `True`, admits the operation.
- Returning `{"admit": False, "err": ..., "desc": ...}` rejects it.  The core raises the given
  exception — defaulting to `SQL-MUTATION-REJECTED` — does not execute the operation, and does not
  change transaction state.
- An exception thrown by the observer propagates and the operation is not executed.  Admission is
  fail-closed by design: a consumer that cannot decide must not have its silence read as consent.

### Terminal events for a rejected operation

Every rejected admission point is followed by exactly one terminal event carrying
`OUTCOME_NOT_EXECUTED` and `replay_safe` `True`.  This holds for both kinds of rejection — an
explicit `admit` `False` decision and a fail-closed observer exception — because a consumer tracking
reservations by `op_id` must always see the operation's lifetime close.  Without it, a rejection
outside a transaction produces no terminal event at all: no rollback follows, so no `OUTCOME` event
is ever emitted, and the reservation has no boundary at which to be released.

| rejected at | terminal event | emitted by |
|---|---|---|
| `PRE_EXEC` | `POST_EXEC` with `OUTCOME_NOT_EXECUTED` | the core; pairs the admission event |
| `STREAM_BEGIN` | `STREAM_END` with `OUTCOME_NOT_EXECUTED` | the core; the producer never started the stream, so it reports no end boundary of its own |
| `STREAM_PROGRESS` | `STREAM_END` with `OUTCOME_NOT_EXECUTED` | the producer, which is required to report the end boundary of a stream it started |

The terminal event is a notification: its return value is ignored, and it cannot re-enter the
callback that produced the rejection, because the re-entrancy depth is released before it is
delivered.  The rejection exception is reported on it through `ex` without being consumed.

For a stream stopped at `STREAM_PROGRESS` the core records the rejection and maps the producer's
`reportMutationStreamEnd(consumed, False)` to `OUTCOME_NOT_EXECUTED` instead of `OUTCOME_ERROR`.  The
distinction matters to a consumer: `OUTCOME_ERROR` on a stream means the database failed while the
payload was being sent, whereas `OUTCOME_NOT_EXECUTED` means the consumer itself stopped it.  In both
cases the bytes already sent are discarded by the surrounding rollback; `replay_safe` `True` states
only that no commit was attempted, not that nothing was sent.

### Notification events

- The return value is ignored.  A rejection decision on a notification event has no meaning and is
  discarded.
- An exception thrown by the observer is recorded on the `ExceptionSink` **after** the real outcome
  has been determined and applied.  The database outcome, the transaction state, and the value
  returned to the caller are unaffected.  In particular, an observer that throws while handling
  `OUTCOME_COMMIT_AMBIGUOUS` does not turn that outcome into a rollback.

### Re-entrancy

A per-`(context, thread)` flag suppresses event emission for operations issued from inside an
observer callback.  An observer may therefore query the same datasource — for example to read a
current size — without recursing.  Suppressed operations execute normally; only the events are
withheld.

### `hashdecl Qore::SQL::SqlMutationEventInfo`

| key | type | meaning |
|---|---|---|
| `event` | `int` | `SQL_MUTATION_EVENT_*` |
| `stmt_class` | `int` | `SQL_STMT_CLASS_*` |
| `driver` | `string` | Qore driver name |
| `db` | `*string` | database name |
| `user` | `*string` | connection user name |
| `tid` | `int` | thread ID issuing the operation |
| `tx_id` | `string` | core-assigned transaction identity |
| `tx_seq` | `int` | event sequence within the transaction |
| `in_transaction` | `bool` | transaction state at the boundary |
| `autocommit` | `bool` | whether the datasource is in autocommit mode |
| `declared` | `bool` | whether a declaration was in effect |
| `info` | `*hash<SqlMutationInfo>` | the innermost declaration, unchanged |
| `outcome` | `*int` | `SQL_MUTATION_OUTCOME_*`, on outcome events |
| `replay_safe` | `*bool` | on outcome events; see above |
| `ex` | `*hash<ExceptionInfo>` | driver exception, on error and ambiguous outcomes |
| `declared_bytes` | `*int` | on stream events |
| `consumed_bytes` | `*int` | on stream events |
| `arg` | `auto` | the observer registration argument |

### `hashdecl Qore::SQL::SqlMutationDecisionInfo`

| key | type | meaning |
|---|---|---|
| `admit` | `bool` | default `True` |
| `err` | `*string` | exception error code on rejection; defaults to `SQL-MUTATION-REJECTED` |
| `desc` | `*string` | exception description on rejection |

## Bounded streams

The core has no COPY or bulk-write API; bulk DML is driver-side.  The observer boundary for streams
is therefore a reporting API that the producer of the stream — a DBI driver performing COPY, or Qore
bulk code — calls as it streams.  The core never sees or buffers the payload; it propagates byte
counts only.

C++, for drivers (non-virtual `DLLEXPORT` methods on `Datasource`):

```cpp
bool sqlMutationObserverActive() const;
int reportMutationStreamBegin(int64 declared_bytes, ExceptionSink* xsink);
int reportMutationStreamProgress(int64 consumed_bytes, ExceptionSink* xsink);
int reportMutationStreamEnd(int64 consumed_bytes, bool ok, ExceptionSink* xsink);
```

Each returns 0 to continue and -1 when rejected or when the observer raised, in which case `xsink`
is set and the caller must abort the stream.  `sqlMutationObserverActive()` lets a driver skip the
bookkeeping entirely when nothing is observing.

Equivalent methods exist on `AbstractDatasource` for Qore-level producers.

A driver compiled against an older `libqore` must guard these calls; `QORE_HAVE_SQL_MUTATION_OBSERVER`
is defined in `qore-version.h` when they are available:

```cpp
#ifdef QORE_HAVE_SQL_MUTATION_OBSERVER
    if (ds->reportMutationStreamProgress(consumed, xsink)) {
        // the consumer stopped the stream; abort it
    }
#endif
```

`declared_bytes` defaults to the declaration's `max_growth_bytes` when
`reportMutationStreamBegin()` is passed 0.  `STREAM_PROGRESS` is an admission point precisely so that
a consumer can stop a stream that has exceeded what it declared, without the core knowing what a
limit means.

On a `DatasourcePool` the stream methods acquire and hold the connection, as `exec()` does, rather
than borrowing it temporarily.  The declared size of a stream is per-connection state, so a
temporary acquisition could report progress against a different connection than the one the stream
was started on.  The connection is released by the caller's commit or rollback.

### Native bulk-load ownership

`BulkInsertOperation` has two bounded-stream implementations, with exactly one event owner for any
operation:

- on the ordinary array-bind or row-loop path, BulkSqlUtil reports `STREAM_BEGIN`, cumulative
  `STREAM_PROGRESS`, and `STREAM_END` around its block flushes;
- on a driver-native path, the DBI driver reports the same boundaries around its native protocol,
  and BulkSqlUtil suppresses its own events.

Native loading is opt-in. `bulk_load: False` is the default, `bulk_load: True` requires an eligible
native mechanism, and `bulk_load: "auto"` falls back to the ordinary path when the capability or
dynamic server support is unavailable. Upserts, SQL value expressions, `returning`, and `rowcode`
callbacks are not native-eligible because the native mechanisms are insert-only and do not return
generated values.

The fallback path measures the serialized Qore values before each block executes. A native driver
reports the encoded bytes it actually passes to its database API, which can include escaping,
delimiters, or protocol framing. Both measures are cumulative payload bytes and can be checked
against the declaration bound, but callers must not assume byte-for-byte equality between paths.

The DBI contract consists of `QDBI_METHOD_BULK_LOAD_BEGIN`,
`QDBI_METHOD_BULK_LOAD_ROWS`, and `QDBI_METHOD_BULK_LOAD_END`. Registering the complete set
automatically adds `DBI_CAP_HAS_BULK_LOAD`; a partial set is invalid. Begin receives an SQL-rendered
table name, ordered SQL-rendered column names, and driver options. It returns 0 after starting a
session, 1 when the mechanism is dynamically unavailable and fallback remains safe, or -1 with an
exception for a hard error. Rows receives one hash-of-columns block. End receives `true` to finish
or `false` to abort and must close the driver protocol and any stream boundary it began. Once begin
returns 0, the caller invokes end exactly once even when row delivery fails. Closing a datasource
with a session still active forces an abort callback as a final safety net.

## API surface

`AbstractDatasource` gains these as **non-abstract** methods that throw
`ABSTRACT-DATASOURCE-ERROR`, following the precedent set by `getSQLStatement()` and
`getCapabilities()`, so that existing user subclasses of `AbstractDatasource` are not broken:

- `setMutationObserver(code<*hash<SqlMutationDecisionInfo>(hash<SqlMutationEventInfo>)> observer, int event_mask = SQL_MUTATION_MASK_DEFAULT, auto arg)`
- `clearMutationObserver()`
- `bool hasMutationObserver()`
- `pushMutationDeclaration(hash<SqlMutationInfo> info)`
- `popMutationDeclaration()`
- `*hash<SqlMutationInfo> getMutationDeclaration()`
- `bool reportMutationStreamBegin(int declared_bytes = 0)`
- `bool reportMutationStreamProgress(int consumed_bytes)`
- `bool reportMutationStreamEnd(int consumed_bytes, bool ok = True)`

`Datasource` and `DatasourcePool` override all of them.

The producer-facing scope class is `Qore::SQL::SqlMutationDeclaration`; its constructor takes the
datasource or pool object and the declaration, and its destructor removes the declaration.  It calls
`pushMutationDeclaration()` and `popMutationDeclaration()` through the object's own Qore-level API
rather than through a C++ downcast, so a user subclass of `AbstractDatasource` that implements those
methods works with it too.

### Incidental core fix

`QDBI_METHOD_DESCRIBE` could not be registered by any DBI driver in a debug build:
`q_dbi_describe_t` is the same function type as `q_dbi_select_row_t`, so a describe method always
resolves to the `add(int, q_dbi_select_row_t)` overload, whose assertion accepted only
`QDBI_METHOD_SELECT_ROW`.  In release builds the assertion is compiled out and registration worked,
which is why this went unnoticed.  The assertion now accepts `QDBI_METHOD_DESCRIBE` as well
(`lib/DBI.cpp`).

## Core hook points

| purpose | location |
|---|---|
| statement admission and result for `exec()`/`execRaw()` | `Datasource::exec_internal()` |
| read boundaries | the six `Datasource::select*()` plus `describe()` |
| transaction start | `Datasource::beginTransaction()`, `beginImplicitTransaction()` |
| commit/rollback outcome, including `SQLStatement::commit()` | `qore_ds_private::commitIntern()`, `rollbackIntern()` |
| autocommit outcome | `Datasource::autoCommit()` and the autocommit branch of `exec_internal()` |
| failover and ambiguity | `qore_ds_private::connectionAborted()`, `connectionLost()` |
| prepared statements | `QoreSQLStatement::execIntern()`, `execDescribeIntern()` |
| context storage and copy | `qore_ds_private` fields and copy constructor |
| pool fan-out and inheritance | `DatasourcePool::setMutationObserver()`, `DatasourceConfig::get()` |

Commit and rollback are hooked in `qore_ds_private` rather than in `Datasource::commit()` because
`QoreSQLStatement::commit()` and `rollback()` call `commitIntern()`/`rollbackIntern()` directly,
bypassing the public `Datasource` methods.

## Testing

Deterministic coverage of failover, commit ambiguity, driver exceptions, and stream boundaries
requires a driver that can be made to fail on command.  `modules/dbi_test` provides one: a mock DBI
driver registered as `dbitest`, with driver options selecting fault injection
(`fail-on-exec`, `fail-on-commit`, `lost-connection-during-commit`, `abort-on-exec`,
`abort-connection`) and a simulated bounded COPY stream.  It is a test artifact and is not installed
by default; `run_tests.sh` discovers `build/modules/*` automatically.

Test matrix:

- autocommit and explicit transaction event sequences;
- known commit, known rollback, driver exception with the transaction still open;
- commit ambiguity, asserted to never be reported as a rollback;
- restartable failover with `replay_safe` `True`, and a `SqlUtil::RestartableTransaction` replay
  presenting the same `op_id` with an incremented attempt;
- admission rejection leaving the transaction and the database untouched;
- observer exception on admission (fail-closed) and on an outcome event (outcome preserved);
- observer removal mid-transaction;
- reads distinguishable from mutations; undeclared mutations reported with `declared` `False`;
- declaration validation negative cases and nesting;
- re-entrancy;
- prepared statements carrying identical metadata, including `SQLStatement::commit()`;
- stream declared vs consumed bytes and mid-stream abort;
- concurrent pools and tenants, with no cross-talk between two pools in one process;
- PostgreSQL equivalents where `QORE_DB_CONNSTR_PGSQL` is set, guarded by `%try-module pgsql`.

### Deterministic faults on a real driver

`dbitest` cannot cover an end-to-end test against a real database server, because it is not
installed and is not a real driver.  For that case a debug build registers one builtin:

```qore
dbg_ds_arm_connection_abort(AbstractDatasource ds, string op_id, int when);
```

It arms a one-shot connection abort for an exact declared `op_id`, at either
`SQL_MUTATION_FAULT_AFTER_EXEC` (after the statement has been executed and before any commit, giving
`LOST_CONNECTION` with `replay_safe` `True`) or `SQL_MUTATION_FAULT_ON_COMMIT` (with the
`commit_in_progress` flag set, giving `COMMIT_AMBIGUOUS`).  An arming that does not match the
current operation's `op_id` is left untouched, and a match consumes it.

Three properties make this a real outcome rather than a simulated one:

- the abort runs the ordinary `Datasource::connectionAborted()` path, so the connection is really
  closed and the server really discards the in-flight transaction;
- the resulting outcome is produced by the existing classification code, with no special casing for
  the fault;
- because the abort is applied at the core boundary rather than inside a driver, it behaves
  identically on every DBI driver.

The arming lives on the `SqlMutationContext`, which a `DatasourcePool` shares with every connection
it pools, so a test can arm the pool without knowing which connection its thread will be allocated.

Everything here is compiled only when `DEBUG` is defined: a release build registers no function and
carries no fault state, so there is no way to reach any of it in production.  Tests call the builtin
by name through `call_function()` and skip when it is absent, so a release-build test run skips
rather than failing to parse.

## Performance

`bench/cases/bench_datasource_mutation_observer.qr` measures the monitored and unmonitored statement
paths against `dbitest`, which does no I/O, so what is measured is core overhead only.

Measured on an Apple M-series macOS build (`-O3`, 2000 autocommit `Datasource::exec()` calls per
iteration, 8 iterations after 2 warmups):

| configuration | per iteration | per statement |
|---|---|---|
| unmonitored (no observer, no declaration) | 1.17 ms | 0.58 µs |
| declared, no observer registered | 1.16 ms | 0.58 µs |
| monitored (observer receiving every boundary) | 21.39 ms | 10.70 µs |

**Unmonitored overhead: none measurable.**  Comparing the same 2000-statement loop against a
`libqore` built from the same tree with this feature reverted, over three runs of 12 iterations
each:

| build | mean per statement |
|---|---|
| without this feature | 0.564 – 0.590 µs |
| with this feature, unmonitored | 0.550 – 0.556 µs |

The monitored path costs roughly 3.3 µs per delivered event, which is the cost of calling a Qore
closure and building the event hash; that is paid only by datasources that opted in.

To reproduce:

```sh
cmake --build build --target dbitest
DYLD_LIBRARY_PATH=build QORE_MODULE_DIR=build/modules/dbi_test:build/qlib-qmod:qlib \
    build/qore bench/cases/bench_datasource_mutation_observer.qr
```

## Out of scope

Quota, pricing, threshold, and retention policy; storage measurement; enforcement decisions.  Those
belong entirely to the consumer.  This design supplies mechanism only.

Also out of scope for this repository: wiring the bounded-stream API into a specific DBI driver.
`Datasource::reportMutationStreamBegin()` / `reportMutationStreamProgress()` /
`reportMutationStreamEnd()` are the contract a driver calls while streaming; the `pgsql` driver
lives in its own repository and must call them from its `COPY` implementation for PostgreSQL bulk
loads to be observable.  `modules/dbi_test` exercises exactly that call sequence so the core side is
verified independently of any real driver.
