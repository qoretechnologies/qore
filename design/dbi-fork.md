# Database connections in a child created with fork()

Copyright (C) 2026 Qore Technologies, s.r.o.

A child process created with `fork()` has a copy of every `Datasource`, `DatasourcePool` and `SQLStatement` of
its parent, with the driver's private data of each open connection and prepared statement. The database session
behind that data belongs to the parent: the child shares its socket (or the client library's handle), and any
transaction in progress is the parent's. Before this policy, a child that closed or destroyed an inherited object
(also at the end of its program) called the driver's rollback, statement close and logoff on the parent's session:
PostgreSQL, MySQL and ODBC sessions of the parent were ended or broken, an Oracle parent lost its cursors or crashed,
and a child that used an inherited `Datasource` whose transaction lock another thread of the parent held waited for
the whole transaction lock timeout (120 s by default) for a thread that does not exist in the child.

The library therefore never calls a driver with the connection or statement data of an ancestor process. Drivers
need no logic of their own for it (see the DBI driver guide, `dbi_fork`).

## Fork generation

`qore_fork_get_generation()` (`include/qore/intern/QoreForkHandlers.h`) is incremented in the child by the fork
registry's child handler. The DBI objects record the generation they belong to:

- `qore_ds_private::open_generation`: set by `Datasource::open()` when the driver opens the connection.
  `isInherited()` is true for an open connection of another generation.
- `ManagedDatasource::lock_generation`: set with the TID of the transaction lock (`setLockTid()`).
  `lockTidInProcess()` is -1 for a lock of another generation: its thread does not exist in this process, unless
  it is the forking thread, whose TID is the same in the child.
- `DatasourcePool::fork_generation`: the generation that the pool's allocations (`tmap`, `free_list`, `tid_list`)
  belong to.
- `QoreSQLStatement::data_generation`: the generation that the statement's driver data belongs to.

## Giving a connection up

`qore_ds_private::abandon()` drops an inherited connection without calling the driver: the private data, the
statement set and the placeholders of statements destroyed in other threads are handed to
`qore_dbi_keep_abandoned()`, which keeps them reachable (as memory that the process still refers to, not as
leaks) until the process ends, and the datasource is left closed with no transaction. The containers are swapped
out without being traversed, since a thread of the parent can have been changing them when the process was copied.
A statement drops its inherited driver data itself (`QoreSQLStatement::dropInheritedData()`), at the start of every
action, of its destruction and of the transaction callbacks. The check reads only the statement's atomic generation,
which the thread that holds the statement's connection sets when it prepares the statement: the other threads that
ask about a statement do not read its driver data. In a child, the first thread that sees the parent's generation
drops the data under the statement's lock and sets the generation of the child before any thread of the child uses
the statement.

The memory given up is a fixed amount for each inherited connection and statement; connections that the child opens
itself are freed normally.

## When the child gives up a connection

Each check names what the caller does with the connection (`QoreDbiForkCheck`, `include/qore/intern/DbiForkRegistry.h`):

| Mode | Callers | Inherited connection | Transaction of the parent |
|---|---|---|---|
| `Raise` | actions that use a connection: `startDBAction()` with a connection, `open()`, `qore_ds_private::forkCheck()` in every `Datasource` action, `DatasourcePool::getDSIntern()` | given up; the action opens a new one | `DATASOURCE-FORK-ERROR` |
| `Defer` | actions that need no connection: options and configuration (`acquireLock()`), `setAutoCommit()`, closing a statement, the pool's `inTransaction()` and `toString()` | given up | recorded; raised by the next `Raise` check |
| `Discard` | `close()`, `reset()`, the destructors, the thread resource cleanup | given up | forgotten without an error |

A transaction of the parent is one of:

- the inherited connection was in a transaction (`qore_ds_private::inTransaction()`);
- another thread of the parent held the transaction lock of a `ManagedDatasource` (also while it was opening the
  connection, which it does with the lock held);
- for a pool, the forking thread held a connection in a transaction (`tid_list`).

`Defer` actions also give the connection up, because the driver would otherwise be called with it: the option and
configuration hashes read the connection's current values from the driver, and the thread that releases the
transaction lock closes the orphaned statements on the connection. `ManagedDatasource::getOption()` reads the
connection's value, so it replaces a connection that a `Defer` check gave up, unless it was in a transaction.

The recorded transaction is kept in `qore_ds_private::fork_lost_tx` (the reason, raised once) and in
`DatasourcePool::fork_lost_tx_tid` (the thread whose next action raises it). A grandchild inherits it.

The thread resource that the forking thread registered in the parent for its transaction is removed by the first
check; when the forking thread exits in the child, the cleanup gives the connection up without a rollback and
without `DATASOURCE-TRANSACTION-EXCEPTION`.

`Datasource::inTransaction()` and `currentThreadInTransaction()` and the pool's equivalents are false in the child
for a transaction of the parent.

## Locks held by threads of the parent

The per-object locks are held across driver calls (opening and closing connections, closing statements) and while
a thread waits for the transaction lock, so they are not acquired before `fork()`: a parent thread can hold one when
the process is copied. Every DBI object registers itself with the DBI fork registry (`QoreDbiForkObject`,
`lib/Datasource.cpp`), a fork handler of rank `QFLR_LEAF` and mode `QFLM_ACQUIRE` for the registry's own lock (it is
held only to link and unlink objects). Its child handler calls `forkChild()` on every registered object, which
initializes the object's locks and conditions again (as for a `QFLM_RESET` lock):

- `qore_ds_private::m` (the statement set);
- `ManagedDatasource::ds_lock` and its condition (the waiter count is reset);
- the `DatasourcePool` lock and condition (the waiter count is reset);
- `QoreSQLStatement::stmt_lock`.

The data that a parent thread could have been changing under these locks is not used in the child: the inherited
connection is given up without traversing its containers, the pool rebuilds its free list from scratch and finds
the forking thread's connection in `tid_list` (an array of integers, written with the containers), and the
statement drops its data.

## What is not covered

- Configuration strings that a parent thread was changing when the process was copied (the pending user name,
  password, database, character set and host of a `Datasource`, `setOption()` values): they can be inconsistent in
  the child. Changing a datasource's configuration while another thread forks is not supported.
- The forking thread itself holding one of these locks while it calls `fork()`: no user code runs while they are
  held, so this can only happen from native code, which must not do it.
- A driver's own connections that the library does not know about (a thread of the driver that holds a connection,
  such as an event dispatcher): the driver must give them up in the child without using them.
- A child that uses an inherited connection through native code that bypasses the library (calling the driver's
  methods directly).
