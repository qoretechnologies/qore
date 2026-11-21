/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_ds_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    The Datasource class provides the low-level interface to Qore DBI drivers.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_DS_PRIVATE_H

#define _QORE_DS_PRIVATE_H

#include "qore/intern/qore_dbi_private.h"
#include "qore/intern/QoreSQLStatement.h"
#include "qore/intern/DatasourceStatementHelper.h"
#include "qore/intern/SqlMutationContext.h"
#include "qore/intern/DbiForkRegistry.h"
#include "qore/intern/QoreForkHandlers.h"

#include <qore/ModuleManager.h>

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

class QoreSandboxManager;

#ifndef _Q_WINDOWS
//! applies the rules of a sandbox for connecting to (or binding) a UNIX socket path (see QoreSocket.cpp)
/** @return 0 if allowed, -1 if denied (an exception is raised)
*/
DLLLOCAL int qore_socket_check_unix_sandbox_rules(QoreSandboxManager& sm, const char* path, bool bind,
        ExceptionSink* xsink);
#endif

//! the sandboxes that restrict a connection, each once, with a reference to each (qore#5747)
class QoreDsRestrictingManagers {
public:
    DLLLOCAL QoreDsRestrictingManagers() = default;

    DLLLOCAL ~QoreDsRestrictingManagers();

    //! adds a manager; takes the reference given, which is released at once if the manager is already in the list
    DLLLOCAL void add(QoreSandboxManager* sm);

    DLLLOCAL bool empty() const {
        return managers.empty();
    }

    DLLLOCAL const std::vector<QoreSandboxManager*>& get() const {
        return managers;
    }

    QoreDsRestrictingManagers(const QoreDsRestrictingManagers&) = delete;
    QoreDsRestrictingManagers& operator=(const QoreDsRestrictingManagers&) = delete;

private:
    std::vector<QoreSandboxManager*> managers;
};
class HashMember;

typedef std::set<QoreSQLStatement*> stmt_set_t;
typedef std::vector<std::pair<std::string, const QoreTypeInfo*>> qore_dbi_typed_result_members_t;

//! The code that set an option of a datasource: whoever configures a datasource decides what its options may do
/** A driver applies the options of a datasource when a connection is opened (Datasource::getConnectOptions()), which
    can happen much later than the options were set, on another thread and for other code: a DatasourcePool opens a
    connection on the thread that first needs it, and trusted code can open a datasource that sandboxed code built.
    The restrictions and the filesystem policy of the code that set an option are therefore recorded with the option
    when it is set (Datasource::setOption(), the options of a Datasource constructor or of a connection string), and
    a DatasourcePool records those of the code that created it for every connection that it creates.  Copies of a
    datasource keep the records of the options that they copy.  A driver reads them with
    Datasource::getOptionRestrictions() and Datasource::getOptionPolicyManagerRef() (qore#5741).

    The same record is kept for the code that chose the connection of a datasource (qore#6142): its creators and the
    setter of each connection value (the user name, password, database name, host name and port); a driver reads them
    with Datasource::getConnectChoosers() and Datasource::addOptionChooser().  For that, it also records whether the
    code could use uncontrolled APIs (allowsUncontrolledApis()).

    The object is immutable after construction, so it is shared by the copies of a datasource and by the connections
    of a pool, also across threads.
*/
class qore_ds_option_setter {
public:
    //! records the code running on the current thread
    /** The restrictions are those that QoreModuleCommandRestrictions::getCallerRestrictions() gives the calling code,
        and the policy manager is the one that a filesystem policy check of the calling code uses
        (QoreSandboxManagerHelper(QoreSandboxManagerHelper::Policy)).  On a thread without Qore thread data (C++ code
        that creates or configures a datasource there), no Qore code is calling: the record is unrestricted, with no
        policy manager
    */
    DLLLOCAL qore_ds_option_setter();

    DLLLOCAL ~qore_ds_option_setter();

    //! returns the restrictions of the code that set the option
    DLLLOCAL const QoreModuleCommandRestrictions& getRestrictions() const {
        return restrictions;
    }

    //! returns a new reference to the filesystem policy manager of the code that set the option, or nullptr
    DLLLOCAL QoreSandboxManager* getPolicyManagerRef() const;

    //! returns true if the code may use uncontrolled APIs now (qore#6142)
    /** With a policy manager, its current QoreSandboxManager::getUncontrolledApiPolicy(), so that a host that changes
        the policy later is honored in both directions; without one (no sandbox policy governed the code, or it ran
        under a trusted barrier), the value captured when the record was made, which is always true then
    */
    DLLLOCAL bool allowsUncontrolledApis() const;

    //! returns qore_sandbox_allows_uncontrolled_apis() with the \c Policy purpose as it was when the record was made
    DLLLOCAL bool getCapturedAllowsUncontrolledApis() const {
        return allows_uncontrolled;
    }

    //! returns true if this record and \a o always give the same results (qore#6142)
    /** The same policy manager (the policy is read from it when a connection is made), the same captured bit and the
        same restrictions: one of them can stand for both, so that the creators of copies of copies stay bounded by
        the distinct code involved
    */
    DLLLOCAL bool isEquivalent(const qore_ds_option_setter& o) const {
        return this == &o || (policy == o.policy && allows_uncontrolled == o.allows_uncontrolled
            && restrictions.isSandboxed() == o.restrictions.isSandboxed()
            && restrictions.getParseOptions() == o.restrictions.getParseOptions());
    }

    //! returns a record of the code running on the current thread
    DLLLOCAL static std::shared_ptr<const qore_ds_option_setter> getCaller() {
        return std::make_shared<const qore_ds_option_setter>();
    }

    qore_ds_option_setter(const qore_ds_option_setter&) = delete;
    qore_ds_option_setter& operator=(const qore_ds_option_setter&) = delete;

private:
    //! the restrictions of the code that set the option
    const QoreModuleCommandRestrictions restrictions;
    //! qore_sandbox_allows_uncontrolled_apis() with the \c Policy purpose of the code when the record was made
    /** Declared before \c policy: \c policy is a raw reference that only the destructor releases, which does not run
        if a later member initializer throws, so no initializer that can fail follows it
    */
    const bool allows_uncontrolled;
    //! the policy manager (referenced) of the code that set the option, or nullptr if no sandbox policy governed it
    QoreSandboxManager* const policy;
};

//! a record of the code that chose a part of the connection of a datasource; see qore_ds_option_setter
typedef std::shared_ptr<const qore_ds_option_setter> qore_ds_setter_t;

//! the creators of a datasource (qore#6142)
/** Copy-on-write: a vector is never changed once it is shared, so that a reader copies the pointer only; a change
    builds a new vector and replaces the pointer.  It has at least one entry and no two equivalent entries
    (qore_ds_option_setter::isEquivalent())
*/
typedef std::shared_ptr<const std::vector<qore_ds_setter_t>> qore_ds_creators_t;

//! returns the creators with the single entry \a creator
DLLLOCAL qore_ds_creators_t qore_ds_make_creators(const qore_ds_setter_t& creator);

//! returns the union of \a a and the creators \a b, \a a first, without a record equivalent to one before it
//! (qore_ds_option_setter::isEquivalent())
DLLLOCAL qore_ds_creators_t qore_ds_merge_creators(const qore_ds_setter_t& a, const qore_ds_creators_t& b);

//! returns the union of the creators \a a and \a b, \a a first, without a record equivalent to one before it; \a a
//! itself if each entry of \a b is equivalent to one of \a a
DLLLOCAL qore_ds_creators_t qore_ds_merge_creators(const qore_ds_creators_t& a, const qore_ds_creators_t& b);

//! the connection parameters of a datasource, which choose the destination and the credentials of a connection
/** The encoding is not one: it does not change the destination or the credentials
*/
enum qore_ds_conn_param {
    QDCP_USER,
    QDCP_PASS,
    QDCP_DB,
    QDCP_HOST,
    QDCP_PORT,
    QDCP_COUNT
};

//! the record of a DatasourcePool for a connection that it creates (qore#6142)
struct qore_ds_pool_record {
    //! the creators of the pool, which are the creators of each of its connections
    qore_ds_creators_t creators;
    //! the code that created the pool: the setter of every value and option of its connections
    qore_ds_setter_t setter;
};

//! the setters of the options of a datasource: option name -> setter
/** the comparator is transparent, so that an option is found by its C string without a copy
*/
typedef std::map<std::string, std::shared_ptr<const qore_ds_option_setter>, std::less<>>
    qore_ds_option_setter_map_t;

//! a consistent copy of the options of a datasource and of the code that set them, for a copy of the datasource
/** Each value is the one that its recorded setter set (see qore_ds_private::getOptionSnapshot()); the object owns the
    reference to the option hash until it is released to the copy
*/
class qore_ds_option_snapshot {
public:
    //! takes ownership of the reference to \a opt
    DLLLOCAL qore_ds_option_snapshot(QoreHashNode* opt, qore_ds_option_setter_map_t&& setters) noexcept
            : opt(opt), setters(std::move(setters)) {
        assert(opt);
    }

    DLLLOCAL qore_ds_option_snapshot(qore_ds_option_snapshot&& old) noexcept
            : opt(old.opt), setters(std::move(old.setters)) {
        old.opt = nullptr;
    }

    DLLLOCAL ~qore_ds_option_snapshot();

    //! releases the reference to the options to the caller
    DLLLOCAL QoreHashNode* releaseOptions() noexcept {
        QoreHashNode* rv = opt;
        opt = nullptr;
        return rv;
    }

    //! returns the setters of the options, for the caller to take
    DLLLOCAL qore_ds_option_setter_map_t& getSetters() noexcept {
        return setters;
    }

    qore_ds_option_snapshot(const qore_ds_option_snapshot&) = delete;
    qore_ds_option_snapshot& operator=(const qore_ds_option_snapshot&) = delete;
    qore_ds_option_snapshot& operator=(qore_ds_option_snapshot&&) = delete;

private:
    //! the options (referenced), or nullptr once released
    QoreHashNode* opt;
    //! the code that set each option in \c opt, if it is known
    qore_ds_option_setter_map_t setters;
};

//! a lock of qore_ds_private::m; debug builds count the sections of the thread that hold one (qore#6142)
/** \c m is not a leaf lock: the statement close and free paths (qore_ds_private::transactionDone(),
    qore_ds_private::closeOrphans()) call the driver with it held.  The sections that guard the pending connection
    values (qore_ds_pending_params) are leaves, and their entry points take \c m with the \c Leaf tag, which asserts in
    a debug build that the thread holds no \c m already: \c QoreThreadLock is not recursive, so such a call would
    deadlock (see the DBI driver rules in 70_qore_modules.doxygen)
*/
class QoreDsLock : public AutoLocker {
public:
    //! the tag of a leaf section of an entry point that must not be called with an \c m held
    enum LeafTag { Leaf };

    //! takes the lock
    DLLLOCAL explicit QoreDsLock(QoreThreadLock& l) : AutoLocker(l) {
        dbgEnter();
    }

    //! takes the lock for a leaf section; asserts in a debug build that the thread holds no qore_ds_private::m
    DLLLOCAL QoreDsLock(QoreThreadLock& l, LeafTag) : AutoLocker(checkLeaf(l)) {
        dbgEnter();
    }

    DLLLOCAL ~QoreDsLock() {
        dbgLeave();
    }

    //! returns true if this object holds \a l
    DLLLOCAL bool holds(const QoreThreadLock& l) const {
        return lck == &l;
    }

#ifdef DEBUG
    //! returns the number of sections on qore_ds_private::m that the current thread is in (debug builds only)
    DLLLOCAL static int dbgDepth();
#endif

    QoreDsLock(const QoreDsLock&) = delete;
    QoreDsLock& operator=(const QoreDsLock&) = delete;

private:
#ifdef DEBUG
    DLLLOCAL static QoreThreadLock& checkLeaf(QoreThreadLock& l);
    DLLLOCAL static void dbgEnter();
    DLLLOCAL static void dbgLeave();
#else
    DLLLOCAL static QoreThreadLock& checkLeaf(QoreThreadLock& l) {
        return l;
    }
    DLLLOCAL static void dbgEnter() {
    }
    DLLLOCAL static void dbgLeave() {
    }
#endif
};

//! the pending connection values of a datasource: the values that the next connection is opened with (qore#6142)
/** They are set by any thread that configures the datasource and read by any thread that copies it or reports its
    configuration, so they are guarded by qore_ds_private::m, which their owner passes to the constructor.  The values
    are private and reachable only through a View or a ConstView, which is constructed from a QoreDsLock that holds
    that lock, so a read or write without the lock does not compile, and a debug build asserts that the lock is the
    right one.

    Every critical section on them is a leaf: it only copies, moves or swaps values (see qore_ds_private).  In a debug
    build, the constructor of a view calls the hook set by setDbgViewHook() with the lock held, so that a test can
    observe every section that reads or writes the values.

    The values in use, which the connection was opened with, are not kept here: they belong to the thread that holds
    the connection (see qore_ds_private::setConnectionValues())
*/
class qore_ds_pending_params {
public:
    //! the pending connection values
    struct Values {
        std::string username,
            password,
            dbname,
            //! the database-specific name of the encoding of the connection
            db_encoding,
            hostname;
        //! the port number (0 = the default port)
        int port = 0;
        //! the code that set each connection parameter (see qore_ds_conn_param), if it is known (qore#6142)
        /** A value and its setter are changed together, so that a reader pairs each value with the code that set it
        */
        qore_ds_setter_t setter[QDCP_COUNT];
    };

    //! creates the object with no values and the creators \a creators, which it takes, guarded by \a m
    DLLLOCAL qore_ds_pending_params(QoreThreadLock& m, qore_ds_creators_t&& creators) noexcept : m(m),
            creators(std::move(creators)) {
    }

    //! creates the object with the values \a v and the creators \a creators, which it takes, guarded by \a m
    DLLLOCAL qore_ds_pending_params(QoreThreadLock& m, Values&& v, qore_ds_creators_t&& creators) noexcept : m(m),
            v(std::move(v)), creators(std::move(creators)) {
    }

    //! read and write access to the values, while \a l holds the lock of the values
    class View {
    public:
        DLLLOCAL View(qore_ds_pending_params& p, const QoreDsLock& l) : v(p.v), c(p.creators) {
            p.dbgCheck(l);
        }

        DLLLOCAL Values& get() const {
            return v;
        }

        //! the creators of the datasource
        DLLLOCAL qore_ds_creators_t& creators() const {
            return c;
        }

        View(const View&) = delete;
        View& operator=(const View&) = delete;

    private:
        Values& v;
        qore_ds_creators_t& c;
    };

    //! read access to the values, while \a l holds the lock of the values
    class ConstView {
    public:
        DLLLOCAL ConstView(const qore_ds_pending_params& p, const QoreDsLock& l) : v(p.v), c(p.creators) {
            p.dbgCheck(l);
        }

        DLLLOCAL const Values& get() const {
            return v;
        }

        //! the creators of the datasource
        DLLLOCAL const qore_ds_creators_t& creators() const {
            return c;
        }

        ConstView(const ConstView&) = delete;
        ConstView& operator=(const ConstView&) = delete;

    private:
        const Values& v;
        const qore_ds_creators_t& c;
    };

#ifdef DEBUG
    //! the hook that the constructor of a view calls with the lock held (unit tests)
    typedef void (*dbg_view_hook_t)(QoreThreadLock& m, void* arg);

    //! sets the hook for the calling thread (DEBUG builds only); nullptr removes it
    DLLLOCAL static void setDbgViewHook(dbg_view_hook_t hook, void* arg);
#endif

    qore_ds_pending_params(const qore_ds_pending_params&) = delete;
    qore_ds_pending_params& operator=(const qore_ds_pending_params&) = delete;

private:
    //! the lock that guards the values
    QoreThreadLock& m;
    //! the values; only reachable through a view
    Values v;
    //! the creators of the datasource (qore#6142); only reachable through a view
    qore_ds_creators_t creators;

#ifdef DEBUG
    //! asserts that \a l holds \c m and calls the hook of the thread, if any
    DLLLOCAL void dbgCheck(const QoreDsLock& l) const;
#else
    DLLLOCAL void dbgCheck(const QoreDsLock& l) const {
    }
#endif
};

//! what a copy of a datasource takes from the original: its options with their setters, and its pending values
/** Read from the original in one critical section on its qore_ds_private::m (see qore_ds_private::getCopySnapshot())
*/
struct qore_ds_copy_snapshot {
    //! the options and the code that set them
    qore_ds_option_snapshot opts;
    //! the pending connection values and their setters
    qore_ds_pending_params::Values pending;
    //! the creators of the copy: the code that copies the datasource and the creators of the original (qore#6142)
    qore_ds_creators_t creators;
};

struct qore_ds_private : public QoreDbiForkObject {
    // mutex
    mutable QoreThreadLock m;

    Datasource* ds;

    // written only by the thread that holds the connection (the transaction lock of a Datasource, or the connection
    // of a DatasourcePool), and read by other threads as a snapshot (Datasource::isInTransaction(), the destruction
    // of a pool), so it is atomic; relaxed ordering is enough, as the holder always reads its own last write
    std::atomic<bool> in_transaction{false};
    bool active_transaction = false;
    bool isopen = false;
    // written with the Datasource's locks held (see ManagedDatasource::setAutoCommit()), but read by any thread: by
    // Datasource::getAutoCommit() and before an action takes the transaction lock, so it is atomic; relaxed ordering
    // is enough, as the value is an independent snapshot
    std::atomic<bool> autocommit{false};
    bool connection_aborted = false;
    bool keep_lock = false;
    //! true while a driver commit call is in progress; used to derive commit ambiguity structurally
    bool commit_in_progress = false;
    //! set when the driver reports a lost or aborted connection; flushed at the next core boundary
    bool mutation_conn_lost = false;
    //! true while the driver owns an active native bulk-load protocol session
    bool bulk_load_active = false;
    //! the fork generation of the process that opened the connection (see qore_fork_get_generation()); see
    //! isInherited()
    unsigned open_generation = 0;
    //! why the next action that uses the connection raises \c DATASOURCE-FORK-ERROR, or nullptr
    /** In a child process created with fork(): a transaction of the parent that was given up by an action that did
        not use the connection (see QoreDbiForkCheck::Defer); see raiseForkLostTransaction()
    */
    std::atomic<const char*> fork_lost_tx{nullptr};

    mutable DBIDriver* dsl;
    const QoreEncoding* qorecharset = QCS_DEFAULT;
    void* private_data = nullptr;               // driver private data per connection

    //! the pending connection values, guarded by \c m (qore#6142)
    /** Declared before \c opt: the copy constructor moves them out of its snapshot, which cannot fail, before the raw
        reference to the options is taken
    */
    qore_ds_pending_params pending;

    // the connection values in use, copied from the pending values when the connection is opened
    // (setConnectionValues()); written and read without a lock by the thread that holds the connection, which is the
    // only writer; another thread reads them only with the lock that the writer holds (the ds_lock of a
    // ManagedDatasource)
    std::string username,
        password,
        db_encoding,   // database-specific name for the encoding for the connection
        dbname,
        hostname;
    int port = 0; // port number (0 = default port)
    //! the code that set each connection value in use (see qore_ds_conn_param), if it is known (qore#6142)
    /** Copied from the pending setters with the values (setConnectionValues()), and written and read like the values
        in use: by the thread that holds the connection
    */
    qore_ds_setter_t param_setter[QDCP_COUNT];
    //! the creators of the datasource as they were when the values in use were copied (qore#5747)
    /** Copied from the pending values with the values in use and their setters, in the same section
        (setConnectionValues()), so that the code that chose a connection is read from the snapshot that the
        connection is opened with; written and read like the values in use: by the thread that holds the connection
    */
    qore_ds_creators_t conn_creators;

    //! the destinations of the open connection as they were checked against the sandbox, or nullptr (qore#5747)
    /** Set by checkConnectTargets() before the driver connects, for a connection chosen by code that a sandbox
        restricting network or filesystem access governed, and kept while the connection is open, so that the driver
        also reconnects to the checked destinations only; written and read by the thread that holds the connection
    */
    QoreListNode* checked_targets = nullptr;

    // options per connection
    QoreHashNode* opt;
    //! the code that set each option in \c opt, if it is known (qore#5741); see qore_ds_option_setter
    /** Protected by \c m; an option's value in \c opt and its setter here are changed together with \c m held
        (commitOption()), so that a copy of the datasource made by another thread pairs each value with the code that
        set it (getOptionSnapshot())
    */
    qore_ds_option_setter_map_t opt_setters;
    //! the option that setOption() is giving to the driver of an open connection, while the driver is called
    /** Protected by \c m; meaningful only while \c pending_opt_setter is set.  An option is set by one thread at a
        time (the thread that configures the datasource, or holds its locks)
    */
    std::string pending_opt_name;
    //! the code that is setting \c pending_opt_name: the driver judges the value by it (getOptionSetter())
    /** Protected by \c m.  It is not the setter of a value that the datasource keeps until the driver accepts the
        value, so a copy of the datasource made meanwhile does not see it (getOptionSnapshot())
    */
    std::shared_ptr<const qore_ds_option_setter> pending_opt_setter;
    //! the option whose entry in \c opt prepareOption() created and that has no value yet, while it is being set
    /** Protected by \c m; meaningful only while \c placeholder_opt is set.  The entry is not an option value: a
        connection opened meanwhile does not apply it (qore_ds_is_option_placeholder())
    */
    std::string placeholder_opt_name;
    //! true while \c placeholder_opt_name names an entry that prepareOption() created
    bool placeholder_opt = false;
    //! the number of connections being opened with the stored options (QoreDsApplyStoredOptionsHelper)
    /** Protected by \c m.  While it is not 0, getOptionSetter() reports the stored setters: the values given to the
        connection are the stored ones, not the value that is being set
    */
    int applying_stored_options = 0;
    // DBI event queue
    Queue* event_queue = nullptr;
    // DBI Event queue argument
    QoreValue event_arg{};

    // mutation observer context, shared with the owning pool if any; nullptr = unmonitored
    /** Atomic because an observer can be registered on a DatasourcePool or a ManagedDatasource
        while other threads are executing statements on connections that already exist: the
        registration runs under the pool or datasource lock, but statement execution does not hold
        that lock, so readers are not serialized against the store.  See
        design/datasource-mutation-observer.md.

        The pointer is only ever replaced while it is null (lazy creation, and inheritance by a
        connection that has not yet been handed out), so a reader that observes a non-null value
        always observes a live, fully-constructed, already-referenced context.
    */
    std::atomic<SqlMutationContext*> mutation_ctx{nullptr};
    // transaction identity assigned when the first observer event of a transaction is delivered
    std::string tx_id;
    // event sequence within the current transaction
    int64 tx_seq = 0;
    // declared size in bytes of the bounded stream in progress; -1 = no stream in progress
    int64 stream_declared_bytes = -1;
    //! set when a bounded stream admission point rejected the stream in progress
    /** it distinguishes a stream that the consumer stopped from one that the driver failed, so that
        the terminal stream event can report the exact outcome; see
        design/datasource-mutation-observer.md
    */
    bool stream_rejected = false;

    // interface for the parent class
    DatasourceStatementHelper* dsh;
    std::map<std::string, const TypedHashDecl*> typed_result_hashdecl_cache;

    //! returns the autocommit flag; see autocommit
    DLLLOCAL bool getAutoCommit() const {
        return autocommit.load(std::memory_order_relaxed);
    }

    //! sets the autocommit flag; see autocommit
    DLLLOCAL void setAutoCommit(bool b) {
        autocommit.store(b, std::memory_order_relaxed);
    }

    //! returns true if a transaction is in progress on the connection; see in_transaction
    DLLLOCAL bool inTransaction() const {
        return in_transaction.load(std::memory_order_relaxed);
    }

    //! sets the transaction status; called only by the thread that holds the connection
    DLLLOCAL void setInTransaction(bool b) {
        in_transaction.store(b, std::memory_order_relaxed);
    }

    //! creates a datasource with the creators \a creators, which it takes (qore#6142)
    /** \c pending is initialized before \c opt, and it cannot fail; \a creators is allocated by the caller
    */
    DLLLOCAL qore_ds_private(Datasource* n_ds, DBIDriver* ndsl, DatasourceStatementHelper* dsh,
            qore_ds_creators_t&& creators) : ds(n_ds), dsl(ndsl), pending(m, std::move(creators)),
            opt(new QoreHashNode(autoTypeInfo)), dsh(dsh) {
        forkRegister();
    }

    //! copies a datasource with the options, setters and pending values in \a snap, which it takes
    /** The pending values are moved out of \a snap, which cannot fail; the options are released from \a snap by a
        member initializer that cannot fail, and every later one cannot fail either, so that \a snap (a temporary of
        the delegating constructor) frees them if a member before them cannot be created
    */
    DLLLOCAL qore_ds_private(const qore_ds_private& old, Datasource* n_ds, DatasourceStatementHelper* dsh,
            qore_ds_copy_snapshot&& snap) :
            ds(n_ds), autocommit(old.getAutoCommit()), dsl(old.dsl),
            pending(m, std::move(snap.pending), std::move(snap.creators)),
            opt(snap.opts.releaseOptions()),
            opt_setters(std::move(snap.opts.getSetters())),
            event_queue(old.event_queue ? old.event_queue->queueRefSelf() : nullptr),
            event_arg(old.event_arg.refSelf()),
            mutation_ctx(old.getMutationCtx() ? old.getMutationCtx()->refSelf() : nullptr),
            dsh(dsh) {
    }

    //! copies a datasource; the copy keeps the setters of the options that it copies (qore#5741)
    /** The code that configured the original decides what its options may do, not the code that copies it.  The
        options and their setters are copied first, into an object that owns them until the copy takes them, and the
        copy is registered once it is constructed, so that nothing leaks if a member cannot be created.

        The copy also keeps the setters of the connection values that it copies, and its creators are the calling
        code (the copier) and the creators of the original (qore#6142): a copy is never less restricted than the
        original or the copier.  The copier is recorded before any lock is taken
    */
    DLLLOCAL qore_ds_private(const qore_ds_private& old, Datasource* n_ds, DatasourceStatementHelper* dsh) :
            qore_ds_private(old, n_ds, dsh, old.getCopySnapshot(qore_ds_option_setter::getCaller())) {
        // the object is constructed here, so the destructor (which unregisters it) runs if anything fails
        forkRegister();
    }

    DLLLOCAL ~qore_ds_private() {
        forkUnregister();
        assert(!pending_opt_setter);
        assert(!private_data);
        assert(stmt_set.empty());
        // the driver data of destroyed statements is closed with the connection
        assert(orphan_stmts.empty());
        clearTypedResultHashDeclCache();
        ExceptionSink xsink;
        clearCheckedTargets();
        if (opt) {
            opt->deref(&xsink);
        }
        event_arg.discard(&xsink);
        if (event_queue) {
            event_queue->deref(&xsink);
        }
        if (SqlMutationContext* ctx = getMutationCtx()) {
            ctx->deref(&xsink);
        }
    }

    //! initializes the lock again in a child process created with fork(); see QoreDbiForkObject
    DLLLOCAL void forkChild() noexcept override {
        m.reinitAfterFork();
    }

    //! returns true if the connection is open and was opened by an ancestor of this process
    /** This is a child process created with fork(): the connection's session belongs to the process that opened
        it, which shares its network connection (see abandon())
    */
    DLLLOCAL bool isInherited() const {
        return isopen && open_generation != qore_fork_get_generation();
    }

    //! gives up a connection that an ancestor of this process opened; see isInherited()
    /** The connection's session belongs to the process that opened it, which shares its network connection: the
        driver is not called (no rollback, no statement close, no logoff), so that the parent's session is not
        changed, and the driver data of the connection and of its statements is given up (it is freed when the
        process ends; see qore_dbi_keep_abandoned()).  The statement set and the placeholders of destroyed statements
        are replaced without being traversed, as a thread of the parent can have been changing them when the process
        was copied; the statements drop their inherited driver data themselves (see
        QoreSQLStatement::dropInheritedData()).

        The datasource is closed afterwards, with no transaction in progress.
    */
    DLLLOCAL void abandon();

    //! gives up an inherited connection; a transaction in progress is raised by the next action that uses the
    //! connection (see raiseForkLostTransaction())
    DLLLOCAL void abandonInherited() {
        assert(isInherited());
        if (inTransaction()) {
            setForkLostTransaction(QoreDbiForkReasonTransaction);
        }
        abandon();
    }

    //! records a transaction of the parent that was given up, for the next action that uses the connection
    /** The first reason is kept.
    */
    DLLLOCAL void setForkLostTransaction(const char* reason) {
        const char* none = nullptr;
        fork_lost_tx.compare_exchange_strong(none, reason, std::memory_order_relaxed);
    }

    //! forgets a transaction of the parent that was given up; the connection is closed or destroyed
    DLLLOCAL void clearForkLostTransaction() {
        fork_lost_tx.store(nullptr, std::memory_order_relaxed);
    }

    //! raises \c DATASOURCE-FORK-ERROR once for a transaction of the parent that was given up
    /** @return 0 if no transaction was given up, -1 if an exception was raised
    */
    DLLLOCAL int raiseForkLostTransaction(ExceptionSink* xsink) {
        if (!fork_lost_tx.load(std::memory_order_relaxed)) {
            return 0;
        }
        const char* reason = fork_lost_tx.exchange(nullptr, std::memory_order_relaxed);
        if (!reason) {
            return 0;
        }
        qore_dbi_raise_fork_error(xsink, *ds, reason);
        return -1;
    }

    //! gives up an inherited connection before it is used; see isInherited()
    /** For an action that uses the connection; a ManagedDatasource and a DatasourcePool give up their inherited
        connections before their actions themselves (except for actions that use no connection, which leave it to the
        Datasource method).  An inherited connection is given up, and a new one is opened for the action, unless the
        connection was in a transaction, which belongs to the process that opened it: then \c DATASOURCE-FORK-ERROR
        is raised, as it is for a transaction that an earlier action gave up (see raiseForkLostTransaction()); the
        datasource is closed then, so a later action opens a new connection

        @return 0 if the action can use the connection, -1 if an exception was raised
    */
    DLLLOCAL int forkCheck(ExceptionSink* xsink);

    //! returns the mutation observer context or nullptr; this is the fast-path accessor
    DLLLOCAL SqlMutationContext* getMutationCtx() const {
        return mutation_ctx.load(std::memory_order_acquire);
    }

    //! returns a copy of the pending connection values
    /** The values are copied in one leaf critical section on \c m: the one allocating step under it, bounded by the
        five strings; must not be called with a qore_ds_private::m held (see QoreDsLock)
    */
    DLLLOCAL qore_ds_pending_params::Values getPendingValues() const {
        qore_ds_pending_params::Values rv;
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        rv = v.get();
        return rv;
    }

    //! returns a copy of one pending string value; see getPendingValues()
    DLLLOCAL std::string getPendingString(std::string qore_ds_pending_params::Values::* field) const {
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        return v.get().*field;
    }

    //! returns copies of the pending host name and port, read together in one leaf critical section on \c m
    /** So that a description of the datasource never pairs a host with the port of another (qore#6988); see
        getPendingValues()
    */
    DLLLOCAL void getPendingHostPort(std::string& host, int& port) const {
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        host = v.get().hostname;
        port = v.get().port;
    }

    //! returns the pending port; see getPendingValues()
    DLLLOCAL int getPendingPort() const {
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        return v.get().port;
    }

    //! sets one pending string value that has no setter (the encoding)
    /** The value is created before the lock is taken and swapped in with it held, and the previous value is freed
        after the lock is released, so nothing allocates or frees under \c m
    */
    DLLLOCAL void setPendingString(std::string qore_ds_pending_params::Values::* field, const char* val) {
        // declared before the lock, so that the previous value is freed after it is released
        std::string tmp(val);
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::View v(pending, al);
        (v.get().*field).swap(tmp);
    }

    //! sets a pending connection parameter and the code that set it, together (qore#6142)
    /** The value is created before the lock is taken and swapped in with it held.  \a setter is swapped with the
        previous setter of the parameter, which the caller releases after the lock, as it can hold the last reference
        to a policy manager; the previous value is freed after the lock as well

        @param p the parameter, not QDCP_PORT
        @param val the value
        @param setter the code that set it; receives the previous setter
    */
    DLLLOCAL void setPendingParam(qore_ds_conn_param p, const char* val, qore_ds_setter_t& setter) {
        assert(p != QDCP_PORT && p < QDCP_COUNT);
        // declared before the lock, so that the previous value is freed after it is released
        std::string tmp(val);
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::View v(pending, al);
        (v.get().*getParamField(p)).swap(tmp);
        v.get().setter[p].swap(setter);
    }

    //! sets the pending port and the code that set it, together; see setPendingParam()
    DLLLOCAL void setPendingPort(int port, qore_ds_setter_t& setter) {
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::View v(pending, al);
        v.get().port = port;
        v.get().setter[QDCP_PORT].swap(setter);
    }

    //! returns the member of the pending values that holds the string parameter \a p
    DLLLOCAL static std::string qore_ds_pending_params::Values::* getParamField(qore_ds_conn_param p) {
        switch (p) {
            case QDCP_USER: return &qore_ds_pending_params::Values::username;
            case QDCP_PASS: return &qore_ds_pending_params::Values::password;
            case QDCP_DB: return &qore_ds_pending_params::Values::dbname;
            case QDCP_HOST: return &qore_ds_pending_params::Values::hostname;
            default:
                break;
        }
        assert(false);
        return &qore_ds_pending_params::Values::dbname;
    }

    //! returns the creators of the datasource
    /** One leaf section that copies a pointer: no allocation, no deref
    */
    DLLLOCAL qore_ds_creators_t getCreators() const {
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        return v.creators();
    }

    //! fills \a c with the creators and the setters of the connection values in use (qore#6142)
    /** Called by the thread that holds the connection, which owns the values in use and their setters; see
        Datasource::getConnectChoosers()

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int getConnectChoosers(QoreDatasourceConnectChoosers& c, ExceptionSink* xsink) const;

    //! fills \a c with the creators and the setters of the connection values in use, as the connection uses them
    /** The creators copied with the values in use (\c conn_creators) are used, so that no other section on the
        pending values is taken and the code that chose the connection is read from the snapshot that the connection
        is opened with (qore#5747); before the values in use were first copied, the current creators are used

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int getInUseConnectChoosers(QoreDatasourceConnectChoosers& c, ExceptionSink* xsink) const;

    //! fills \a c with the given creators and the setters of the connection values in use
    DLLLOCAL int getConnectChoosersIntern(const qore_ds_creators_t& cr, QoreDatasourceConnectChoosers& c,
            ExceptionSink* xsink) const;

    //! copies the pending connection values of \a other and their setters to this datasource
    /** The values of \a other and its creators are copied with its lock held and then swapped into this datasource
        with this lock held, so the two locks are never held together; the previous values are freed after the lock is
        released.  The creators of this datasource become the union of its own and those of \a other, so that it is
        never less restricted than it was (qore#6142)
    */
    DLLLOCAL void setPendingConnectionValues(const qore_ds_private* other);

    //! copies the pending connection values to the values in use; called by the thread that holds the connection
    /** The pending values and their setters are copied to the values in use in one leaf section on \c m: the strings
        are assigned in place, which reuses their storage (the one bounded allocation that a section on \c m can make),
        and a setter is replaced only if it changed, with the previous one released after the lock, so no record is
        released under it.  Only the thread that holds the connection writes the values in use.  For a
        ManagedDatasource, its \c ds_lock is held by the caller, so that another thread that reads the values in use
        with \c ds_lock held does not race with the change (a debug build asserts it); this function does not take
        \c ds_lock itself, as its callers hold it already
    */
    DLLLOCAL void setConnectionValues() {
        assert(!dsh || dsh->helperHoldsConnectionValuesLock());
        // the previous setters of the values in use and the previous creators, released after the lock
        qore_ds_setter_t old_setters[QDCP_COUNT];
        qore_ds_creators_t old_creators;
        QoreDsLock al(m, QoreDsLock::Leaf);
        qore_ds_pending_params::ConstView v(pending, al);
        const qore_ds_pending_params::Values& p = v.get();
        // a string assignment can fail (bad_alloc) after others succeeded: the open fails then, and the next open
        // copies every value and setter again, so a value in use is never used with the setter of another value
        dbname = p.dbname;
        username = p.username;
        password = p.password;
        hostname = p.hostname;
        db_encoding = p.db_encoding;
        port = p.port;
        for (unsigned i = 0; i < QDCP_COUNT; ++i) {
            if (param_setter[i] != p.setter[i]) {
                old_setters[i] = std::move(param_setter[i]);
                param_setter[i] = p.setter[i];
            }
        }
        // the creators with the values that they chose, for the sandbox check of the connection (qore#5747): a
        // pointer copy, as getCreators() makes
        if (conn_creators != v.creators()) {
            old_creators = std::move(conn_creators);
            conn_creators = v.creators();
        }
    }

    DLLLOCAL void statementExecuted(int rc);

    DLLLOCAL void clearTypedResultHashDeclCache();

    DLLLOCAL const TypedHashDecl* getTypedResultHashDecl(const char* driver_name, const std::string& signature,
            const qore_dbi_typed_result_members_t& members, ExceptionSink* xsink);

    //! sets an option for the datasource and records the code that set it
    /** The option is validated, set on the connection if the datasource is open, and kept to be set on every new
        connection with \a setter as the code that set it.  A value that is refused is not kept, and the previous
        setter of the option stays recorded.

        @param name the option
        @param val the value
        @param setter the code that set the option: the calling code for Datasource::setOption(), the code that
        created a DatasourcePool for the connections of the pool
        @param xsink for the errors of the option

        @return 0 for OK, -1 for an error (exception raised)
    */
    DLLLOCAL int setOption(const char* name, const QoreValue val,
            const std::shared_ptr<const qore_ds_option_setter>& setter, ExceptionSink* xsink);

    //! returns true if \a name is the entry of a new option that is being set and has no value yet
    DLLLOCAL bool isOptionPlaceholder(const char* name) const {
        assert(name);
        QoreDsLock al(m);
        return placeholder_opt && placeholder_opt_name == name;
    }

    //! returns the setter of an option, or an empty pointer if it is not known
    /** While setOption() gives a value to the driver of an open connection, the driver judges it by the code that is
        setting it, so that code is returned for that option, except while a connection is opened with the stored
        options (\c applying_stored_options), which are judged by their stored setters
    */
    DLLLOCAL std::shared_ptr<const qore_ds_option_setter> getOptionSetter(const char* name) const {
        assert(name);
        QoreDsLock al(m);
        // the pending setter is the setter of the value that the driver is being given, not of the stored values that
        // a connection being opened meanwhile is given
        if (pending_opt_setter && !applying_stored_options && pending_opt_name == name) {
            return pending_opt_setter;
        }
        qore_ds_option_setter_map_t::const_iterator i = opt_setters.find(name);
        return i == opt_setters.end() ? std::shared_ptr<const qore_ds_option_setter>() : i->second;
    }

    //! returns a copy of the options and of their setters in which each value is the one that its setter set
    /** For a copy of the datasource, which can be made by another thread while an option is being set: the values
        that the datasource keeps and their setters are read together with \c m held, as commitOption() changes them,
        and they replace the values that the driver of an open connection reports for those options; an option that
        the driver is being given (setOption()) and that has no kept value is not copied.  Options that the driver
        reports and that were never set (its defaults) are copied without a setter.
    */
    DLLLOCAL qore_ds_option_snapshot getOptionSnapshot() const;

    //! returns what a copy of the datasource takes from it: getOptionSnapshot() and the pending connection values
    /** The pending values are copied in the same critical section on \c m as the options that the datasource keeps
        (qore#6142); must not be called with a qore_ds_private::m held (see QoreDsLock)
    */
    DLLLOCAL qore_ds_copy_snapshot getCopySnapshot(const qore_ds_setter_t& copier) const;

    //! returns getOptionSnapshot(); if \a pending is not nullptr, the pending connection values are copied into it in
    //! the same critical section as the options, and the creators into \a creators
    DLLLOCAL qore_ds_option_snapshot getOptionSnapshotIntern(qore_ds_pending_params::Values* pending,
            qore_ds_creators_t* creators) const;

    //! what keeping an option needs, allocated before the driver is given the value (see setOption())
    /** Everything that can fail is allocated here, so that keeping a value that the driver accepted cannot fail: the
        connection has the value from the moment that the driver accepts it, and it must then be kept with its setter
    */
    struct OptionCommitPrep {
        //! the record of the setter if the option has none yet
        qore_ds_option_setter_map_t::node_type node;
        //! the member of the option in \c opt, created for it if the option had no value; nullptr until prepared
        HashMember* member = nullptr;
        //! true if \c member was created for this change
        bool created_member = false;
    };

    //! allocates what keeping the option needs; can fail (bad_alloc) before anything is changed
    /** The member of a new option is created with no value, which the copies of the datasource treat as an option
        that is not set (getOptionSnapshot()); cancelOption() removes it again if the value is not kept
    */
    DLLLOCAL void prepareOption(const char* name, const std::shared_ptr<const qore_ds_option_setter>& setter,
            OptionCommitPrep& prep);

    //! undoes prepareOption() for a value that is not kept; allocates nothing
    DLLLOCAL void cancelOption(const char* name, OptionCommitPrep& prep) noexcept;

    //! keeps an option that the driver accepted, with the code that set it, as one change made with \c m held
    /** @param name the option
        @param val the value; the reference is taken
        @param setter the code that set it
        @param prep what prepareOption() allocated for the change
        @param xsink for the errors of releasing the previous value

        A setter that setOption() made pending for the driver (\c pending_opt_setter) is cleared in the same critical
        section.  Allocates nothing, so it cannot fail
    */
    DLLLOCAL void commitOption(const char* name, QoreValue val,
            const std::shared_ptr<const qore_ds_option_setter>& setter, OptionCommitPrep& prep, ExceptionSink* xsink);

#ifdef DEBUG
    //! a hook that setOption() calls after the driver accepted a value and before the value is kept (unit tests)
    typedef void (*dbg_option_commit_hook_t)(qore_ds_private& ds, const char* name, void* arg);

    //! sets the hook for the calling thread (DEBUG builds only); nullptr removes it
    DLLLOCAL static void setDbgOptionCommitHook(dbg_option_commit_hook_t hook, void* arg);
#endif

    DLLLOCAL QoreHashNode* getOptionHash() const {
        return private_data ? qore_dbi_private::get(*dsl)->getOptionHash(ds) : opt->hashRefSelf();
    }

    DLLLOCAL QoreHashNode* getCurrentOptionHash(bool ensure_hash = false) const;

    DLLLOCAL QoreHashNode* getConfigHash() const;

    DLLLOCAL QoreStringNode* getConfigString() const;

    DLLLOCAL void setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
        if (event_queue)
            event_queue->deref(xsink);
        event_arg.discard(xsink);
        event_queue = q;
        event_arg = arg;
    }

    DLLLOCAL QoreHashNode* getEventQueueHash(Queue*& q, int event_code) const {
        q = event_queue;
        if (!q)
            return nullptr;
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        if (!username.empty())
            h->setKeyValue("user", new QoreStringNode(username), 0);
        if (!dbname.empty())
            h->setKeyValue("db", new QoreStringNode(dbname), 0);
        h->setKeyValue("eventtype", event_code, 0);
        if (event_arg)
            h->setKeyValue("arg", event_arg.refSelf(), 0);
        return h;
    }

    //! returns true if an observer is registered and wants events for the given mask bit
    /** this is the fast path for all mutation observer hooks; when no context has been created it
        is a single predictable null pointer test
    */
    DLLLOCAL bool observes(int64 mask_bit) const {
        SqlMutationContext* ctx = getMutationCtx();
        return ctx && ctx->observes(mask_bit);
    }

    //! creates the mutation context if it does not yet exist; callers must serialize access
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext() {
        SqlMutationContext* ctx = getMutationCtx();
        if (!ctx) {
            ctx = new SqlMutationContext;
            mutation_ctx.store(ctx, std::memory_order_release);
        }
        return ctx;
    }

    //! replaces the mutation context; takes ownership of the reference passed
    DLLLOCAL void setMutationContext(SqlMutationContext* ctx, ExceptionSink* xsink) {
        SqlMutationContext* old = getMutationCtx();
        if (old == ctx) {
            if (ctx) {
                ctx->deref(xsink);
            }
            return;
        }
        mutation_ctx.store(ctx, std::memory_order_release);
        if (old) {
            old->deref(xsink);
        }
    }

    //! delivers an observer event; fills in the datasource-derived fields first
    /** @return 0 to continue, -1 if an admission event was rejected

        @note the transaction identity is assigned lazily here, so that a transaction that produced
        no observable event never consumes an identity and never reports a terminal outcome
    */
    DLLLOCAL int dispatchMutationEvent(SqlMutationEvent& ev, ExceptionSink* xsink) {
        SqlMutationContext* ctx = getMutationCtx();
        assert(ctx);
        if (!ctx->observes(ev.getMaskBit())) {
            return 0;
        }
        if (tx_id.empty()) {
            ctx->getNewTransactionId(tx_id);
            tx_seq = 0;
        }
        ev.tx_id = tx_id.c_str();
        ev.tx_seq = ++tx_seq;
        ev.in_transaction = inTransaction();
        ev.autocommit = getAutoCommit();
        int rc = ctx->dispatch(ev, *ds, xsink);
        if (rc && ev.isAdmission()) {
            dispatchMutationNotExecuted(ev, xsink);
        }
        return rc;
    }

    //! emits the terminal event for an operation that an admission point rejected
    /** The event closes the operation's lifetime for a consumer that tracks reservations by
        \c "op_id": without it a rejected operation would leave an unpaired admission event, and a
        rejection outside a transaction would produce no terminal event at all.

        It cannot be emitted from SqlMutationContext::dispatch() itself, because the re-entrancy
        depth that suppresses observer-issued operations is still held there; it is emitted here,
        after that depth has been released.

        The event is a notification: the operation has already been refused, so the return value is
        ignored.  The rejection exception is passed for reporting only and is not consumed.

        @param ev the rejected admission event
    */
    DLLLOCAL void dispatchMutationNotExecuted(const SqlMutationEvent& ev, ExceptionSink* xsink) {
        assert(ev.isAdmission());
        // a rejected stream in progress is closed by its producer's terminal event, which reports
        // the rejection through the "stream_rejected" flag; emitting here would duplicate it
        if (ev.event == SQL_MUTATION_EVENT_STREAM_PROGRESS) {
            stream_rejected = true;
            return;
        }
        if (ev.event == SQL_MUTATION_EVENT_STREAM_BEGIN) {
            // the producer never started the stream, so it emits no terminal event of its own
            stream_declared_bytes = -1;
            SqlMutationEvent term(SQL_MUTATION_EVENT_STREAM_END, SQL_STMT_CLASS_STREAM);
            term.declared_bytes = ev.declared_bytes;
            term.consumed_bytes = 0;
            term.outcome = SQL_MUTATION_OUTCOME_NOT_EXECUTED;
            // the operation was refused before execution, so no commit can have been attempted
            term.replay_safe = 1;
            term.driver_xsink = xsink;
            dispatchMutationEvent(term, xsink);
            return;
        }
        assert(ev.event == SQL_MUTATION_EVENT_PRE_EXEC);
        SqlMutationEvent term(SQL_MUTATION_EVENT_POST_EXEC, ev.stmt_class);
        term.outcome = SQL_MUTATION_OUTCOME_NOT_EXECUTED;
        term.replay_safe = 1;
        term.driver_xsink = xsink;
        dispatchMutationEvent(term, xsink);
    }

#ifdef DEBUG
    //! triggers an armed debug connection abort when the current operation matches it
    /** Debug builds only.  The abort goes through the ordinary Datasource::connectionAborted()
        path, so the connection is really closed and the server really discards the transaction;
        nothing about the resulting outcome is synthesized.  The exception is required: the
        connection_aborted paths assert that one is active.

        @param when the boundary being passed; see @ref sql_mutation_debug_fault_codes

        @return true if the connection was aborted, in which case an exception has been raised
    */
    DLLLOCAL bool dbgCheckArmedFault(int when, ExceptionSink* xsink) {
        SqlMutationContext* ctx = getMutationCtx();
        if (!ctx || !ctx->dbgFaultArmed()) {
            return false;
        }
        ReferenceHolder<QoreHashNode> decl(ctx->getDeclaration(), xsink);
        if (!decl) {
            return false;
        }
        QoreStringNodeValueHelper op_id(decl->getKeyValue("op_id"));
        if (!ctx->dbgTakeArmedFault(op_id->c_str(), when)) {
            return false;
        }
        xsink->raiseException(SQL_MUTATION_DEBUG_ABORT_ERR, "debug connection abort armed for op_id %s "
            "triggered at boundary %d", op_id->c_str(), when);
        connectionAborted(xsink);
        return true;
    }
#endif

    //! emits a transaction start event
    DLLLOCAL void dispatchMutationTxBegin(ExceptionSink* xsink) {
        if (!observes(SQL_MUTATION_MASK_TX)) {
            return;
        }
        SqlMutationEvent ev(SQL_MUTATION_EVENT_TX_BEGIN, SQL_STMT_CLASS_BEGIN);
        dispatchMutationEvent(ev, xsink);
    }

    //! emits a terminal transaction outcome event and ends the transaction identity
    DLLLOCAL void dispatchMutationOutcome(int stmt_class, int outcome, int replay_safe,
            ExceptionSink* driver_xsink, ExceptionSink* xsink) {
        mutation_conn_lost = false;
        if (observes(SQL_MUTATION_MASK_TX)) {
            SqlMutationEvent ev(SQL_MUTATION_EVENT_OUTCOME, stmt_class);
            ev.outcome = outcome;
            ev.replay_safe = replay_safe;
            ev.driver_xsink = driver_xsink;
            dispatchMutationEvent(ev, xsink);
        }
        // the next operation starts a new transaction identity
        tx_id.clear();
        tx_seq = 0;
    }

    //! reports a connection loss recorded by the driver at the next core boundary
    /** The event is not emitted from Datasource::connectionAborted()/connectionLost() because
        those are called by the driver from inside a driver call; emitting there would run observer
        code with driver state in flux and would report the transaction outcome before the result of
        the statement that triggered it.
    */
    DLLLOCAL void flushMutationConnectionLost(ExceptionSink* xsink) {
        if (!mutation_conn_lost) {
            return;
        }
        // no commit was in flight, so no commit can have been applied: the operation can be replayed
        dispatchMutationOutcome(SQL_STMT_CLASS_ROLLBACK, SQL_MUTATION_OUTCOME_LOST_CONNECTION, 1, xsink, xsink);
    }

    DLLLOCAL void addStatement(QoreSQLStatement* stmt) {
        QoreDsLock al(m);
        assert(stmt_set.find(stmt) == stmt_set.end());
        stmt_set.insert(stmt);
    }

    DLLLOCAL void removeStatement(QoreSQLStatement* stmt) {
        QoreDsLock al(m);
        stmt_set_t::iterator i = stmt_set.find(stmt);
        if (i != stmt_set.end())
            stmt_set.erase(i);
    }

    //! takes the driver data of a statement that is destroyed in a thread that does not hold this connection
    /** The statement is removed from the connection, and its driver data, which belongs to the connection, is kept in
        a placeholder statement; the thread that holds the connection closes it when its transaction ends (see
        transactionDone()), when it releases the connection or the transaction lock (see closeOrphans()), or when the
        connection is closed, so that the driver is only ever called for the connection by that thread.
    */
    DLLLOCAL void adoptStatementData(QoreSQLStatement* stmt);

    //! closes the driver data of destroyed statements that the connection holds; called by the thread holding it
    DLLLOCAL void closeOrphans(ExceptionSink* xsink) {
        QoreDsLock al(m);
        orphansDone(true, xsink);
    }

    DLLLOCAL void connectionAborted(ExceptionSink* xsink) {
        assert(isopen);
        // close all statements and clear private data, leave datasource allocated
        transactionDone(false, true, xsink);
        // mark connection aborted
        connection_aborted = true;
        if (getMutationCtx()) {
            mutation_conn_lost = true;
        }
        // close the datasource; the connection is lost, and the driver reports that, so an error closing it is a
        // warning event
        ExceptionSink close_xsink;
        close(&close_xsink);
        qore_dbi_private::reportCloseWarnings(ds, close_xsink);
    }

    DLLLOCAL void connectionLost(ExceptionSink* xsink) {
#ifdef DEBUG
        // issue #4117: get backtrace if connectionLost() called while the connection is closed
        if (!isopen) {
            qore_machine_backtrace();
        }
#endif
        assert(isopen);
        // close statements but do not clear datasource or statements in the datasource
        transactionDone(false, false, xsink);
        if (getMutationCtx() && inTransaction()) {
            mutation_conn_lost = true;
        }
    }

    DLLLOCAL void connectionRecovered(ExceptionSink* xsink) {
        assert(isopen);
        // close all statements, clear private data, leave datasource allocation
        transactionDone(false, true, xsink);
    }

    // @param clear if true then clears the statements' datasource ptrs and the stmt_set, if false, does not
    DLLLOCAL void transactionDone(bool clear, bool close, ExceptionSink* xsink) {
        QoreDsLock al(m);
        for (stmt_set_t::iterator i = stmt_set.begin(), e = stmt_set.end(); i != e; ++i) {
            //printd(5, "qore_ds_private::transactionDone() this: %p stmt: %p clear: %d close: %d\n", this, *i, clear, close);
            (*i)->transactionDone(clear, close, xsink);
        }
        if (clear)
            stmt_set.clear();
        orphansDone(close, xsink);
    }

    DLLLOCAL int commitIntern(ExceptionSink* xsink) {
        //printd(5, "qore_ds_private::commitIntern() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
        assert(isopen);
        setInTransaction(false);
        active_transaction = false;
        if (!getMutationCtx()) {
            return qore_dbi_private::get(*dsl)->commit(ds, xsink);
        }
        // the flag is what makes commit ambiguity structural: any failure or connection loss while
        // it is set means the core cannot know whether the server applied the commit
        commit_in_progress = true;
#ifdef DEBUG
        // an abort armed for the commit boundary is triggered with the flag set, so that it is
        // classified as ambiguous exactly as a real connection loss during a commit would be
        int rc = dbgCheckArmedFault(SQL_MUTATION_FAULT_ON_COMMIT, xsink)
            ? -1
            : qore_dbi_private::get(*dsl)->commit(ds, xsink);
        // This second debug-only boundary runs only after the driver has
        // demonstrably committed.  Closing the connection while
        // commit_in_progress is still set gives recovery tests a
        // deterministic durable COMMIT_AMBIGUOUS branch.
        if (!rc && !*xsink
                && dbgCheckArmedFault(SQL_MUTATION_FAULT_AFTER_COMMIT, xsink)) {
            rc = -1;
        }
#else
        int rc = qore_dbi_private::get(*dsl)->commit(ds, xsink);
#endif
        commit_in_progress = false;
        // a commit that did not demonstrably succeed is always ambiguous and is never reported as a
        // rollback: the server may have applied it
        const bool known = !rc && !*xsink && !connection_aborted;
        dispatchMutationOutcome(SQL_STMT_CLASS_COMMIT,
            known ? SQL_MUTATION_OUTCOME_COMMIT : SQL_MUTATION_OUTCOME_COMMIT_AMBIGUOUS, 0, xsink, xsink);
        return rc;
    }

    DLLLOCAL int rollbackIntern(ExceptionSink* xsink) {
        //printd(5, "qore_ds_private::rollbackIntern() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
        assert(isopen);
        setInTransaction(false);
        active_transaction = false;
        if (!getMutationCtx()) {
            return qore_dbi_private::get(*dsl)->rollback(ds, xsink);
        }
        int rc = qore_dbi_private::get(*dsl)->rollback(ds, xsink);
        const bool known = !rc && !*xsink && !connection_aborted;
        // no commit was attempted either way, so the operation can always be replayed
        dispatchMutationOutcome(SQL_STMT_CLASS_ROLLBACK,
            known ? SQL_MUTATION_OUTCOME_ROLLBACK : SQL_MUTATION_OUTCOME_ROLLBACK_ERROR, 1, xsink, xsink);
        return rc;
    }

    DLLLOCAL int commit(ExceptionSink* xsink) {
        int rc = commitIntern(xsink);
        transactionDone(true, true, xsink);
        return rc;
    }

    DLLLOCAL int rollback(ExceptionSink* xsink) {
        int rc = rollbackIntern(xsink);
        transactionDone(true, true, xsink);
        return rc;
    }

    //! closes the connection and the statements prepared on it, if it is open
    /** the datasource is closed after the call in every case

        @return 0 if the connection was closed without an error or was not open, -1 if an error closing it or one of
        its statements was added to \a xsink
    */
    //! releases the checked destinations of the connection
    DLLLOCAL void clearCheckedTargets() {
        if (checked_targets) {
            // a list of strings, ints and hashes of them: no object can be released here
            checked_targets->deref(nullptr);
            checked_targets = nullptr;
        }
    }

    //! returns the sandboxes that restrict the connection: the policy managers of the code that chose it
    /** The policy managers of the creators of the datasource and of the setters of the connection values in use (see
        getConnectChoosers()) that restrict network or filesystem access, each once

        @return 0 for success, -1 if an exception was raised (the record cannot be read)
    */
    DLLLOCAL int getRestrictingManagers(QoreDsRestrictingManagers& managers, ExceptionSink* xsink) const;

    //! checks the destinations of a connection against the sandboxes of the code that chose it (qore#5747)
    /** Called before the driver opens the connection: for a connection that no sandbox restricting network or
        filesystem access chose, nothing is done; otherwise the driver must report its destinations
        (QDBI_METHOD_GET_CONNECT_TARGETS), which are checked against every such sandbox and kept for the driver
        (Datasource::getCheckedConnectTargets()), unless it declares DBI_CAP_LOCAL_ONLY, or the connection is refused

        @return 0 if the connection may be opened, -1 if an exception was raised
    */
    DLLLOCAL int checkConnectTargets(ExceptionSink* xsink);

    DLLLOCAL int close(ExceptionSink* xsink) {
        assert(xsink);
        // a transaction of the parent of a child process created with fork() ends with the connection without an
        // error
        clearForkLostTransaction();
        if (isInherited()) {
            // the connection belongs to the process that opened it: nothing is sent on it
            abandon();
            return 0;
        }
        if (isopen) {
            //printd(5, "qore_ds_private::close() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
            if (bulk_load_active && !connection_aborted) {
                // A native protocol session cannot survive a close/reopen cycle.  Give the driver a
                // final abort callback so it can close its wire-level state and stream boundary.
                ExceptionSink bulk_xsink;
                bulk_load_active = false;
                qore_dbi_private::get(*dsl)->bulkLoadEnd(ds, false, &bulk_xsink);
                bulk_xsink.clear();
            }
            bulk_load_active = false;
            // what is raised here is checked in a sink of its own, as the caller's can already hold an exception
            ExceptionSink close_xsink;
            // the statements prepared on the connection are closed while it is still open, as their driver data
            // belongs to it; they stay registered, with their datasource, so that an action in progress - one in
            // which the driver aborted the connection, for example - still releases the connection when it ends
            transactionDone(false, true, &close_xsink);
            qore_dbi_private::get(*dsl)->close(ds, &close_xsink);
            isopen = false;
            clearCheckedTargets();
            setInTransaction(false);
            active_transaction = false;
            commit_in_progress = false;
            // a pending connection-loss event must keep the transaction identity it belongs to;
            // it is cleared when the outcome is delivered at the next core boundary
            if (!mutation_conn_lost) {
                tx_id.clear();
                tx_seq = 0;
            }
            clearTypedResultHashDeclCache();
            if (!close_xsink) {
                return 0;
            }
            xsink->assimilate(close_xsink);
            return -1;
        }
        return 0;
    }

    DLLLOCAL void setStatementKeepLock(QoreSQLStatement* stmt) {
        assert(!keep_lock);
        keep_lock = true;
        if (!inTransaction())
            setInTransaction(true);
        if (!active_transaction)
            active_transaction = true;

        addStatement(stmt);
    }

    DLLLOCAL bool keepLock() {
        bool rv = keep_lock;
        if (keep_lock)
            keep_lock = false;
        return rv;
    }

    DLLLOCAL static qore_ds_private* get(Datasource& ds) {
        return ds.priv;
    }

    //! creates a connection of a DatasourcePool with the creators of the pool (qore#6142)
    /** The connection's creators are the creators of the pool, not the code that runs when the pool grows.

        @param driver the DBIDriver object to use for the connection
        @param dsh the interface of the real C++ parent object
        @param rec the creators of the pool

        @return the new object; the caller owns it
    */
    DLLLOCAL static Datasource* createPoolConnection(DBIDriver* driver, DatasourceStatementHelper* dsh,
            const qore_ds_pool_record& rec);

private:
    // set of active SQLStatements on this datasource
    stmt_set_t stmt_set;
    // placeholders holding the driver data of statements destroyed in another thread than the one holding this
    // connection; protected by m, and closed by the thread holding the connection when its transaction ends
    std::vector<SQLStatement*> orphan_stmts;

    //! closes the placeholders (or frees their handles if \a close is false); called with m held
    DLLLOCAL void orphansDone(bool close, ExceptionSink* xsink);
};

#endif
