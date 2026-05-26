/*
    DatasourcePool.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/DatasourcePool.h"
#include "qore/intern/qore_ds_private.h"
#include <memory>

DatasourcePoolActionHelper::~DatasourcePoolActionHelper() {
    if (!ds)
        return;

    bool keep_lock = qore_ds_private::get(*ds)->keepLock();

    if (cmd == DAH_RELEASE
        || ds->wasConnectionAborted()
        || (new_ds && (cmd == DAH_NOCHANGE) && !keep_lock))
        dsp.freeDS(xsink);
}

// the first connection (opened in the DatasourcePool constructor) is passed with an xsink obj
// because invalid options can cause an exception to be thrown
Datasource* DatasourceConfig::get(DatasourceStatementHelper* dsh, ExceptionSink* xsink) const {
    Datasource* ds = new Datasource(driver, dsh);

    if (!user.empty())
        ds->setPendingUsername(user.c_str());
    if (!pass.empty())
        ds->setPendingPassword(pass.c_str());
    if (!db.empty())
        ds->setPendingDBName(db.c_str());
    if (!encoding.empty())
        ds->setPendingDBEncoding(encoding.c_str());
    if (!host.empty())
        ds->setPendingHostName(host.c_str());

    if (port)
        ds->setPendingPort(port);

    if (q) {
        q->ref();
        ds->setEventQueue(q, arg.refSelf(), nullptr);
    }

    // set options
    ConstHashIterator hi(opts);
    while (hi.next()) {
        // skip "min" and "max" options
        if (!strcmp(hi.getKey(), "min") || !strcmp(hi.getKey(), "max"))
            continue;

        if (ds->setOption(hi.getKey(), hi.get(), xsink))
            break;
    }

    // turn off autocommit
    ds->setAutoCommit(false);

    return ds;
}

DatasourcePool::DatasourcePool(ExceptionSink* xsink, DBIDriver* ndsl, const char* user, const char* pass,
                               const char* db, const char* charset, const char* hostname, unsigned mn, unsigned mx, int port, const QoreHashNode* opts,
                               Queue* q, QoreValue a) :
    pool(new Datasource*[mx]),
    tid_list(new int[mx]),
    min(mn),
    max(mx),
    cmax(0),
    wait_count(0),
    wait_max(0),
    tl_warning_ms(0),
    tl_timeout_ms(120000),
    stats_reqs(0),
    stats_hits(0),
    warning_callback(nullptr),
    config(ndsl, user, pass, db, charset, hostname, port, opts, q, a),
    valid(false) {
    //assert(mn > 0);
    //assert(mx > min);
    //assert(db != 0 && db[0]);

    assert(!(a && !q));

    // create minimum datasources if possible
    printd(5, "DatasourcePool::DatasourcePool(driver: %p user: %s pass: %s db: %s charset: %s host: %s min: %d max: %d port: %d) pool: %p\n",
            ndsl, user ? user : "(null)", pass ? pass : "(null)", db ? db : "(null)", charset ? charset : "(null)",
            hostname ? hostname : "(null)", min, max, port, pool);

    init(xsink);
}

DatasourcePool::DatasourcePool(const DatasourcePool& old, ExceptionSink* xsink) :
    pool(new Datasource*[old.max]),
    tid_list(new int[old.max]),
    min(old.min),
    max(old.max),
    cmax(0),
    wait_count(0),
    wait_max(0),
    tl_warning_ms(old.tl_warning_ms),
    tl_timeout_ms(old.tl_timeout_ms),
    stats_reqs(0),
    stats_hits(0),
    warning_callback(old.warning_callback ? old.warning_callback->refRefSelf() : nullptr),
    callback_arg(old.callback_arg.refSelf()),
    config(old.config),
    valid(false) {
    init(xsink);
}

DatasourcePool::~DatasourcePool() {
    //printd(5, "DatasourcePool::~DatasourcePool() this: %p\n", this);
    for (unsigned i = 0; i < cmax; ++i)
        delete pool[i];
    delete [] tid_list;
    delete [] pool;
    assert(!warning_callback);
    assert(!callback_arg);
}

// common constructor code
void DatasourcePool::init(ExceptionSink* xsink) {
    assert(xsink);
    // ths initial Datasource creation could throw an exception if there is an error in a driver option, for example
    std::unique_ptr<Datasource> ds(config.get(this, xsink));
    if (*xsink)
        return;

    ds->open(xsink);
    if (*xsink)
        return;

    pool[0] = ds.release();
    //printd(5, "DP::init() open %s: %p (%d)\n", ndsl->getName(), pool[0], xsink->isEvent());
    // add to free list
    free_list.push_back(0);

    while (++cmax < min) {
        ds.reset(config.get(this, xsink));
        if (*xsink) {
            return;
        }
        ds->open(xsink);
        if (*xsink) {
            return;
        }
        pool[cmax] = ds.release();
        //printd(5, "DP::init() open %s: %p (%d)\n", ndsl->getName(), pool[cmax], xsink->isEvent());
        // add to free list
        free_list.push_back(cmax);
    }
    valid = true;
}

void DatasourcePool::cleanup(ExceptionSink* xsink) {
   int tid = q_gettid();

   // thread must have a Datasource allocated
   SafeLocker sl((QoreThreadLock *)this);
   thread_use_t::iterator i = tmap.find(tid);
   assert(i != tmap.end());
   sl.unlock();

#ifndef DEBUG_1
   xsink->raiseException("DATASOURCEPOOL-LOCK-EXCEPTION", "%s:%s@%s: TID %d terminated while in a transaction with connection %d; transaction will be automatically rolled back and the datasource returned to the pool", pool[0]->getDriverName(), pool[0]->getUsernameStr().c_str(), pool[0]->getDBNameStr().c_str(), tid, i->second);
#else
   QoreString* sql = getAndResetSQL();
   xsink->raiseException("DATASOURCEPOOL-LOCK-EXCEPTION", "%s:%s@%s: TID %d terminated while in a transaction; transaction will be automatically rolled back and the datasource returned to the pool\n%s", pool[0]->getDriverName(), pool[0]->getUsernameStr().c_str(), pool[0]->getDBNameStr().c_str(), tid, sql ? sql->getBuffer() : "<no data>");
   if (sql)
      delete sql;
#endif

   // execute rollback on Datasource before releasing to pool
   pool[i->second]->rollback(xsink);

   // grab lock to add to free list and erase thread map entry
   sl.lock();
   free_list.push_back(i->second);
   // erase thread map entry
   tmap.erase(i);
   // signal any waiting threads
   if (wait_count)
      signal();
}

void DatasourcePool::destructor(ExceptionSink* xsink) {
    //printd(5, "DatasourcePool::destructor() this: %p\n", this);
    SafeLocker sl((QoreThreadLock*)this);

    // mark object as invalid in case any threads are waiting on a free Datasource
    valid = false;

    int tid = q_gettid();
    thread_use_t::iterator i = tmap.find(tid);
    unsigned curr = i == tmap.end() ? (unsigned)-1 : i->second;

    for (unsigned j = 0; j < cmax; ++j) {
        if (j != curr && pool[j]->isInTransaction())
            xsink->raiseException("DATASOURCEPOOL-ERROR", "%s:%s@%s: TID %d deleted DatasourcePool while TID %d using connection %d/%d was in a transaction", pool[0]->getDriverName(), pool[0]->getUsernameStr().c_str(), pool[0]->getDBNameStr().c_str(), q_gettid(), tid_list[j], j + 1, cmax);
    }

    if (i != tmap.end() && pool[curr]->isInTransaction()) {
        xsink->raiseException("DATASOURCEPOOL-LOCK-EXCEPTION", "%s:%s@%s: TID %d deleted DatasourcePool while in a transaction; transaction will be automatically rolled back", pool[0]->getDriverName(), pool[0]->getUsernameStr().c_str(), pool[0]->getDBNameStr().c_str(), tid);
        sl.unlock();

        // execute rollback on Datasource before releasing to pool
        pool[curr]->rollback(xsink);

        freeDS(xsink);
    }

    if (warning_callback) {
        warning_callback->deref(xsink);
        callback_arg.discard(xsink);
#ifdef DEBUG
        warning_callback = nullptr;
        callback_arg = QoreValue();
#endif
    }

    config.del(xsink);
}

#if 0
void DatasourcePool::addSQL(const char* cmd, const QoreString* sql) {
   QoreString* str = thread_local_storage.get();
   if (!str)
      str = new QoreString;
   else
      str->concat('\n');
   str->sprintf("%s(): %s", cmd, sql->getBuffer());
   thread_local_storage.set(str);
}

void DatasourcePool::resetSQL() {
   QoreString* str = thread_local_storage.get();
   if (str) {
      delete str;
      thread_local_storage.set(nullptr);
   }
}

QoreString* DatasourcePool::getAndResetSQL() {
   QoreString* str = thread_local_storage.get();
   thread_local_storage.set(nullptr);
   return str;
}
#endif

void DatasourcePool::freeDS(ExceptionSink* xsink) {
   // remove from thread resource list
   //printd(5, "DatasourcePool::freeDS() remove_thread_resource(this: %p)\n", this);

   remove_thread_resource(this);

   int tid = q_gettid();

   AutoLocker al((QoreThreadLock*)this);

   thread_use_t::iterator i = tmap.find(tid);
   assert(!pool[i->second]->isInTransaction());
   free_list.push_back(i->second);

   // issue 1250: close any other statements created on this datasource
   qore_ds_private::get(*pool[i->second])->transactionDone(true, true, xsink);

   tmap.erase(i);

   if (wait_count)
      signal();
}

Datasource* DatasourcePool::getDS(bool &new_ds, ExceptionSink* xsink) {
   assert(xsink);
   assert(!*xsink);

   // total # of microseconds waiting for a new connection
   int64 wait_total = 0;
   Datasource* ds = getDSIntern(new_ds, wait_total, xsink);
   if (!ds) {
      assert(*xsink);
      return nullptr;
   }
   assert(ds);
   assert(!*xsink);

   assert(!(new_ds && ds->isInTransaction()));

   // only run the wait callback if we have a wait time and if no exception was raised acquiring a datasource
   if (wait_total && checkWait(wait_total, xsink)) {
      assert(new_ds);
      assert(!ds->isInTransaction());
      freeDS(xsink);
      return nullptr;
   }

   // try to open Datasource if it's not open already
   if (!ds->isOpen()) {
      assert(new_ds);
      if (ds->open(xsink)) {
         assert(!ds->isInTransaction());
         freeDS(xsink);
         return nullptr;
      }
   }

   assert(ds->isOpen());
   assert(!*xsink);

   return ds;
}

Datasource* DatasourcePool::getAllocatedDS() {
   SafeLocker sl((QoreThreadLock*)this);
   // see if thread already has a datasource allocated
   thread_use_t::iterator i = tmap.find(q_gettid());
   assert(i != tmap.end());
   return pool[i->second];
}

// must be called in the lock
int DatasourcePool::checkWait(int64 wait_total, ExceptionSink* xsink) {
    assert(wait_total);

    ReferenceHolder<ResolvedCallReferenceNode> wc(xsink);

    {
        // get reference to callback and check wait threshold only while holding lock
        AutoLocker al((QoreThreadLock*)this);
        if (!warning_callback || (wait_total / 1000) < tl_warning_ms) {
            return 0;
        }

        wc = warning_callback->refRefSelf();
    }

    // build argument list
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    // in case of failure to acquire a connection to get the config string, the warning callback is not called
    SimpleRefHolder<QoreStringNode> cstr(getConfigString(xsink));
    if (*xsink)
        return -1;
    args->push(cstr.release(), xsink);
    args->push(wait_total, xsink);
    args->push(tl_warning_ms, xsink);
    args->push(callback_arg.refSelf(), xsink);
    wc->execValue(*args, xsink).discard(xsink);
    return *xsink ? -1 : 0;
}

Datasource* DatasourcePool::getDSIntern(bool& new_ds, int64& wait_total, ExceptionSink* xsink) {
    assert(!new_ds);

    int tid = q_gettid();

    Datasource* ds;

    SafeLocker sl((QoreThreadLock*)this);

    // increase request counter
    ++stats_reqs;

    // see if thread already has a datasource allocated
    thread_use_t::iterator i = tmap.find(tid);
    if (i != tmap.end()) {
        ++stats_hits;
        //printd(5, "DatasourcePool::getDSIntern() this: %p returning already allocated ds: %p\n", this, pool[i->second]);
        return pool[i->second];
    }

    // will be a new allocation, not already in a transaction
    new_ds = true;

    // iteration flag
    bool iter = false;

    // see if there is a datasource free
    while (true) {
        if (!free_list.empty()) {
            int fi = free_list.front();
            free_list.pop_front();
            // DEBUG
            //printf("DSP::getDS() assigning tid %d index %d from free list (%N)\n", $tid, $i, $.p[$i]);

            tmap[tid] = fi;
            ds = pool[fi];
            tid_list[fi] = tid;

            // increase hit counter
            if (!iter)
                ++stats_hits;
            break;
        }

        // see if we can open a new connection
        if (cmax < max) {
            ds = pool[cmax] = config.get(this, xsink);
            assert(!*xsink);

            tmap[tid] = cmax;
            tid_list[cmax++] = tid;

            // increase hit counter
            if (!iter)
                ++stats_hits;

            break;
        }

        //printd(5, "DatasourcePool::getDSIntern() this: %p tl_timeout_ms: %d max: %d\n", this, tl_timeout_ms, max);
        // otherwise we sleep until a connection becomes available
        ++wait_count;
        int64 warn_start = q_clock_getmicros();
        int rc = tl_timeout_ms ? wait((QoreThreadLock*)this, tl_timeout_ms) : wait((QoreThreadLock*)this);
        wait_count--;

        // add waiting time to total time
        wait_total += (q_clock_getmicros() - warn_start);

        if (!valid) {
            xsink->raiseException("DATASOURCEPOOL-ERROR", "%s:%s@%s: DatasourcePool deleted while TID %d waiting " \
                "on a connection to become free", getDriverName(), pool[0]->getUsernameStr().c_str(),
                pool[0]->getDBNameStr().c_str(), tid);
            return nullptr;
        }

        if (rc && tl_timeout_ms) {
            xsink->raiseException("DATASOURCEPOOL-TIMEOUT", "%s:%s@%s: TID %d timed out on datasource pool after " \
                "waiting " QLLD " millisecond%s for a free connection (max %d connections in use)",
                                getDriverName(), pool[0]->getUsernameStr().c_str(),
                                pool[0]->getDBNameStr().c_str(), tid,
                                tl_timeout_ms, tl_timeout_ms == 1 ? "" : "s", max);
            return nullptr;
        }

        if (!iter)
            iter = true;
        continue;
    }

    if (wait_total > wait_max)
        wait_max = wait_total;

    sl.unlock();

    // add to thread resource list
    //printd(5, "DatasourcePool::getDSIntern() set_thread_resource(this: %p) ds: %p\n", this, ds);

    set_thread_resource(this);

    assert(ds);
    return ds;
}

QoreValue DatasourcePool::select(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink);
    if (!dpah)
        return QoreValue();

    return dpah->select(sql, args, xsink);
}

QoreValue DatasourcePool::selectTyped(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink);
    if (!dpah)
        return QoreValue();

    return dpah->selectTyped(sql, args, xsink);
}

QoreColumnarResult* DatasourcePool::selectColumnar(const QoreString* sql, const QoreListNode* args,
        ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink);
    if (!dpah)
        return nullptr;

    return dpah->selectColumnar(sql, args, xsink);
}

QoreHashNode* DatasourcePool::selectRow(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourcePoolActionHelper dpah(*this, xsink);
   if (!dpah)
      return nullptr;

   return dpah->selectRow(sql, args, xsink);
}

QoreValue DatasourcePool::selectRows(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourcePoolActionHelper dpah(*this, xsink);
   if (!dpah)
      return QoreValue();

   return dpah->selectRows(sql, args, xsink);
}

QoreValue DatasourcePool::selectRowsTyped(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourcePoolActionHelper dpah(*this, xsink);
   if (!dpah)
      return QoreValue();

   return dpah->selectRowsTyped(sql, args, xsink);
}

QoreHashNode* DatasourcePool::describe(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourcePoolActionHelper dpah(*this, xsink);
   if (!dpah)
      return nullptr;

   return dpah->describe(sql, args, xsink);
}

int DatasourcePool::beginTransaction(ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink, DAH_ACQUIRE);
    if (!dpah)
        return 0;

    int rc = dpah->beginTransaction(xsink);
    if (rc) {
        assert(*xsink);
        dpah.releaseNew();
    } else {
        assert(!*xsink);
    }
    return rc;
}

QoreValue DatasourcePool::exec_internal(bool doBind, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourcePoolActionHelper dpah(*this, xsink, DAH_ACQUIRE);
   if (!dpah)
      return QoreValue();

   return doBind ? dpah->exec(sql, args, xsink) : dpah->execRaw(sql, args, xsink);;
}

QoreValue DatasourcePool::exec(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   return exec_internal(true, sql, args, xsink);
}

QoreValue DatasourcePool::execRaw(const QoreString* sql, ExceptionSink* xsink) {
   return exec_internal(false, sql, nullptr, xsink);
}

int DatasourcePool::commit(ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink, DAH_RELEASE);
    if (!dpah)
        return -1;

    //printd(5, "DatasourcePool::commit() this: %p ds: %p: %s@%s\n", this, *dpah, dpah->getUsername(), dpah->getDBName());

    return dpah->commit(xsink);
}

int DatasourcePool::rollback(ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink, DAH_RELEASE);
    if (!dpah)
        return -1;

    //printd(5, "DatasourcePool::rollback() this: %p ds: %p\n", this, *dpah);

    return dpah->rollback(xsink);
}

QoreValue DatasourcePool::getServerVersion(ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink);
    if (!dpah) {
        return QoreValue();
    }

    return dpah->getServerVersion(xsink);
}

QoreStringNode* DatasourcePool::getDriverRealName(ExceptionSink* xsink) {
    DatasourcePoolActionHelper dpah(*this, xsink);
    if (!dpah) {
        return nullptr;
    }

    return dpah->getDriverRealName(xsink);
}

QoreStringNode* DatasourcePool::toString() {
    QoreStringNode* str = new QoreStringNode;

    SafeLocker sl((QoreThreadLock *)this);
    str->sprintf("this: %p, min: %d, max: %d, cmax: %d, wait_count: %d, thread_map = (", this, min, max, cmax, wait_count);
    thread_use_t::const_iterator ti = tmap.begin();
    while (ti != tmap.end()) {
        str->sprintf("tid %d: %d, ", ti->first, ti->second);
        ++ti;
    }
    if (!tmap.empty())
        str->terminate(str->strlen() - 2);

    str->sprintf("), free_list = (");
    free_list_t::const_iterator fi = free_list.begin();
    while (fi != free_list.end()) {
        str->sprintf("%d, ", *fi);
        ++fi;
    }
    if (!free_list.empty())
        str->terminate(str->strlen() - 2);
    sl.unlock();
    str->concat(')');
    return str;
}

unsigned DatasourcePool::getMin() const {
    return min;
}

unsigned DatasourcePool::getMax() const {
    return max;
}

QoreStringNode* DatasourcePool::getPendingUsername() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingUsername();
}

QoreStringNode* DatasourcePool::getPendingPassword() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingPassword();
}

QoreStringNode* DatasourcePool::getPendingDBName() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingDBName();
}

QoreStringNode* DatasourcePool::getPendingDBEncoding() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingDBEncoding();
}

QoreStringNode* DatasourcePool::getPendingHostName() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingHostName();
}

int DatasourcePool::getPendingPort() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getPendingPort();
}

const QoreEncoding* DatasourcePool::getQoreEncoding() const {
    // depends on static configuration, can be called disconnected
    return pool[0]->getQoreEncoding();
}

bool DatasourcePool::inTransaction() {
    int tid = q_gettid();
    AutoLocker al((QoreThreadLock*)this);
    return tmap.find(tid) != tmap.end();
}

QoreHashNode* DatasourcePool::getConfigHash(ExceptionSink* xsink) {
    QoreHashNode* h;
    {
        DatasourcePoolActionHelper dpah(*this, xsink);
        if (!dpah)
            return nullptr;

        h = dpah->getConfigHash();
    }

    // add min and max options
    QoreHashNode* opt = h->getKeyValue("options").get<QoreHashNode>();
    if (!opt) {
        opt = new QoreHashNode(autoTypeInfo);
        h->setKeyValue("options", opt, nullptr);
    }
    opt->setKeyValue("min", min, nullptr);
    opt->setKeyValue("max", max, nullptr);

    return h;
}

QoreStringNode* DatasourcePool::getConfigString(ExceptionSink* xsink) {
    QoreStringNode* str;
    {
        DatasourcePoolActionHelper dpah(*this, xsink);
        if (!dpah)
            return 0;

        str = dpah->getConfigString();
    }

    // add min and max options
    QoreStringMaker mm(",min=%d,max=%d", min, max);
    if ((*str)[str->size() - 1] == '}')
        str->splice(str->size() - 1, 0, mm, xsink);
    else
        str->sprintf("{%s}", mm.getBuffer() + 1);

    return str;
}

void DatasourcePool::clearWarningCallback(ExceptionSink* xsink) {
    AutoLocker al((QoreThreadLock*)this);
    if (warning_callback) {
        callback_arg.discard(xsink);
        warning_callback->deref(xsink);
        warning_callback = nullptr;
        tl_warning_ms = 0;
    }
}

void DatasourcePool::setWarningCallback(int64 warning_ms, ResolvedCallReferenceNode* cb, QoreValue arg, ExceptionSink* xsink) {
    if (warning_ms <= 0) {
        xsink->raiseException("DATASOURCEPOOL-SETWARNINGCALLBACK-ERROR", "DatasourcePool::setWarningCallback() warning ms argument: " QLLD " must be greater than zero; to clear, call DatasourcePool::clearWarningCallback() with no arguments", warning_ms);
        return;
    }
    AutoLocker al((QoreThreadLock*)this);
    if (warning_callback) {
        warning_callback->deref(xsink);
        callback_arg.discard(xsink);
    }
    warning_callback = cb;
    tl_warning_ms = warning_ms;
    callback_arg = arg;
}

QoreHashNode* DatasourcePool::getUsageInfo() const {
    AutoLocker al((QoreThreadLock*)this);
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    if (warning_callback) {
        h->setKeyValue("callback", warning_callback->refRefSelf(), nullptr);
        h->setKeyValue("arg", callback_arg.refSelf(), nullptr);
        h->setKeyValue("timeout", tl_warning_ms, nullptr);
    }
    h->setKeyValue("wait_max", wait_max, nullptr);
    h->setKeyValue("stats_reqs", stats_reqs, nullptr);
    h->setKeyValue("stats_hits", stats_hits, nullptr);
    return h;
}

void DatasourcePool::setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
    AutoLocker al((QoreThreadLock*)this);

    // assign to all datasources
    bool first = true;
    for (unsigned i = 0; i < cmax; ++i) {
        if (first) {
            pool[i]->setEventQueue(q, arg, xsink);
            first = false;
        }
        else
            pool[i]->setEventQueue(q ? q->queueRefSelf() : nullptr, arg.refSelf(), xsink);
    }

    config.setQueue(q, arg, xsink);
}

QoreListNode* DatasourcePool::getCapabilityList() const {
    // depends on the driver, can be called disconnected
    return pool[0]->getCapabilityList();
}

int DatasourcePool::getCapabilities() const {
    // depends on the driver, can be called disconnected
    return pool[0]->getCapabilities();
}
