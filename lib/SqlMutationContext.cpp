/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SqlMutationContext.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Datasource/pool-scoped structured mutation observer support; see
    design/datasource-mutation-observer.md

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

#include "qore/intern/SqlMutationContext.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/typed_hash_decl_private.h"

#include <cstdio>

int64 SqlMutationEvent::getMaskBit() const {
    switch (event) {
        case SQL_MUTATION_EVENT_TX_BEGIN:
        case SQL_MUTATION_EVENT_OUTCOME:
            return SQL_MUTATION_MASK_TX;

        case SQL_MUTATION_EVENT_STREAM_BEGIN:
        case SQL_MUTATION_EVENT_STREAM_PROGRESS:
        case SQL_MUTATION_EVENT_STREAM_END:
            return SQL_MUTATION_MASK_STREAM;

        case SQL_MUTATION_EVENT_PRE_EXEC:
        case SQL_MUTATION_EVENT_POST_EXEC:
            return stmt_class == SQL_STMT_CLASS_READ ? SQL_MUTATION_MASK_READ : SQL_MUTATION_MASK_EXEC;

        default:
            break;
    }
    assert(false);
    return 0;
}

SqlMutationContext::~SqlMutationContext() {
    assert(!observer);
    assert(!observer_arg);
#ifdef DEBUG
    for (auto& i : tmap) {
        assert(i.second.decls.empty());
        assert(!i.second.depth);
    }
#endif
}

void SqlMutationContext::deref(ExceptionSink* xsink) {
    if (refs.ROdereference()) {
        // clear any observer and any remaining declarations before deleting
        clearObserver(xsink);
        {
            AutoLocker al(m);
            for (auto& i : tmap) {
                for (auto& d : i.second.decls) {
                    d->deref(xsink);
                }
                i.second.decls.clear();
                i.second.depth = 0;
            }
            tmap.clear();
            decl_active.store(false, std::memory_order_relaxed);
        }
        delete this;
    }
}

void SqlMutationContext::setObserver(ResolvedCallReferenceNode* cb, int64 mask, QoreValue arg,
        ExceptionSink* xsink) {
    assert(cb);
    ReferenceHolder<ResolvedCallReferenceNode> old_cb(xsink);
    ValueHolder old_arg(xsink);
    {
        AutoLocker al(m);
        old_cb = observer;
        old_arg = observer_arg;
        observer = cb;
        observer_arg = arg;
        event_mask.store(mask, std::memory_order_relaxed);
        observer_active.store(true, std::memory_order_relaxed);
    }
}

void SqlMutationContext::clearObserver(ExceptionSink* xsink) {
    ReferenceHolder<ResolvedCallReferenceNode> old_cb(xsink);
    ValueHolder old_arg(xsink);
    {
        AutoLocker al(m);
        if (!observer) {
            return;
        }
        old_cb = observer;
        old_arg = observer_arg;
        observer = nullptr;
        observer_arg = QoreValue();
        event_mask.store(0, std::memory_order_relaxed);
        observer_active.store(false, std::memory_order_relaxed);
    }
}

int SqlMutationContext::checkDeclaration(const QoreHashNode* info, ExceptionSink* xsink) {
    assert(info);

    bool exists = false;
    QoreValue v = info->getKeyValueExistence("op_id", exists);
    if (!exists || v.getType() != NT_STRING || v.get<const QoreStringNode>()->empty()) {
        xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"op_id\" key must be present and must be a "
            "non-empty string; it provides the stable operation identity that must survive a restartable "
            "transaction replay");
        return -1;
    }

    v = info->getKeyValueExistence("path_id", exists);
    if (!exists || v.getType() != NT_STRING || v.get<const QoreStringNode>()->empty()) {
        xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"path_id\" key must be present and must be a "
            "non-empty string; it identifies the reviewed managed path issuing the operation");
        return -1;
    }

    int64 effect = info->getKeyValue("effect").getAsBigInt();
    if (effect != SQL_MUTATION_EFFECT_NONE && effect != SQL_MUTATION_EFFECT_MAY_GROW
        && effect != SQL_MUTATION_EFFECT_RECLAIM_ONLY) {
        xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"effect\" key has invalid value " QLLD "; expecting "
            "SQL_MUTATION_EFFECT_NONE (%d), SQL_MUTATION_EFFECT_MAY_GROW (%d), or SQL_MUTATION_EFFECT_RECLAIM_ONLY "
            "(%d)", effect, SQL_MUTATION_EFFECT_NONE, SQL_MUTATION_EFFECT_MAY_GROW,
            SQL_MUTATION_EFFECT_RECLAIM_ONLY);
        return -1;
    }

    v = info->getKeyValueExistence("max_growth_bytes", exists);
    if (exists && !v.isNothing()) {
        int64 mgb = v.getAsBigInt();
        if (mgb <= 0) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"max_growth_bytes\" key has invalid value "
                QLLD "; when present it must be a positive number of bytes", mgb);
            return -1;
        }
        if (effect != SQL_MUTATION_EFFECT_MAY_GROW) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"max_growth_bytes\" key can only be used with "
                "\"effect\" = SQL_MUTATION_EFFECT_MAY_GROW (%d); got %d", SQL_MUTATION_EFFECT_MAY_GROW, (int)effect);
            return -1;
        }
    }

    v = info->getKeyValueExistence("reclaim_audit_id", exists);
    if (exists && !v.isNothing()) {
        if (v.getType() != NT_STRING || v.get<const QoreStringNode>()->empty()) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"reclaim_audit_id\" key must be a non-empty "
                "string when present");
            return -1;
        }
        if (effect != SQL_MUTATION_EFFECT_RECLAIM_ONLY) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"reclaim_audit_id\" key can only be used with "
                "\"effect\" = SQL_MUTATION_EFFECT_RECLAIM_ONLY (%d); got %d", SQL_MUTATION_EFFECT_RECLAIM_ONLY,
                (int)effect);
            return -1;
        }
    }

    v = info->getKeyValueExistence("attempt", exists);
    if (exists && !v.isNothing()) {
        int64 attempt = v.getAsBigInt();
        if (attempt < 1) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "the \"attempt\" key has invalid value " QLLD "; when "
                "present it must be >= 1", attempt);
            return -1;
        }
    }

    return 0;
}

int SqlMutationContext::pushDeclaration(const QoreHashNode* info, ExceptionSink* xsink) {
    assert(info);
    assert(xsink);

    if (checkDeclaration(info, xsink)) {
        return -1;
    }

    // store the declaration as a SqlMutationInfo-typed hash so that observers always see the
    // declared type, even when the caller passed a plain hash
    ReferenceHolder<QoreHashNode> decl(xsink);
    if (info->getHashDecl() == hashdeclSqlMutationInfo) {
        decl = info->hashRefSelf();
    } else {
        decl = typed_hash_decl_private::get(*hashdeclSqlMutationInfo)->newHash(info, true, xsink);
        if (*xsink) {
            return -1;
        }
    }

    int tid = q_gettid();
    AutoLocker al(m);
    tmap[tid].decls.push_back(decl.release());
    decl_active.store(true, std::memory_order_relaxed);
    return 0;
}

int SqlMutationContext::popDeclaration(ExceptionSink* xsink) {
    assert(xsink);

    QoreHashNode* decl = nullptr;
    {
        int tid = q_gettid();
        AutoLocker al(m);
        tmap_t::iterator i = tmap.find(tid);
        if (i == tmap.end() || i->second.decls.empty()) {
            xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "there is no active mutation declaration in this "
                "thread to remove");
            return -1;
        }
        decl = i->second.decls.back();
        i->second.decls.pop_back();
        pruneThreadIntern(i);
    }
    decl->deref(xsink);
    return 0;
}

QoreHashNode* SqlMutationContext::getDeclaration() const {
    if (!decl_active.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    int tid = q_gettid();
    AutoLocker al(m);
    tmap_t::const_iterator i = tmap.find(tid);
    if (i == tmap.end() || i->second.decls.empty()) {
        return nullptr;
    }
    return i->second.decls.back()->hashRefSelf();
}

//! process-wide counter used to give each context a stable, non-reused identity
/** the address of the context cannot be used for this, because addresses are reused after a
    context is destroyed, which would let two unrelated transactions share an identity
*/
static std::atomic<int64> sql_mutation_ctx_counter{0};

int64 SqlMutationContext::getContextId() const {
    int64 id = ctx_id.load(std::memory_order_relaxed);
    if (!id) {
        id = sql_mutation_ctx_counter.fetch_add(1, std::memory_order_relaxed) + 1;
        int64 expected = 0;
        if (!ctx_id.compare_exchange_strong(expected, id, std::memory_order_relaxed)) {
            id = expected;
        }
    }
    return id;
}

void SqlMutationContext::getNewTransactionId(std::string& tx_id) {
    int64 seq = tx_counter.fetch_add(1, std::memory_order_relaxed) + 1;
    char buf[64];
    // the context id is unique within the process and is never reused; the counter makes the
    // identity unique within the datasource or pool
    snprintf(buf, sizeof(buf), QLLD "." QLLD, getContextId(), seq);
    tx_id = buf;
}

#ifdef DEBUG
void SqlMutationContext::dbgArmFault(const char* op_id, int when) {
    AutoLocker al(m);
    if (when == SQL_MUTATION_FAULT_NONE || !op_id || !*op_id) {
        dbg_fault_op_id.clear();
        dbg_fault_when = SQL_MUTATION_FAULT_NONE;
        dbg_fault_armed.store(false, std::memory_order_relaxed);
        return;
    }
    dbg_fault_op_id = op_id;
    dbg_fault_when = when;
    dbg_fault_armed.store(true, std::memory_order_relaxed);
}

int SqlMutationContext::dbgTakeArmedFault(const char* op_id) {
    if (!op_id || !*op_id || !dbg_fault_armed.load(std::memory_order_relaxed)) {
        return SQL_MUTATION_FAULT_NONE;
    }
    AutoLocker al(m);
    if (dbg_fault_op_id != op_id) {
        // an operation the test did not arm must never be disturbed
        return SQL_MUTATION_FAULT_NONE;
    }
    const int when = dbg_fault_when;
    // one-shot: the arming is consumed by the operation it matched
    dbg_fault_op_id.clear();
    dbg_fault_when = SQL_MUTATION_FAULT_NONE;
    dbg_fault_armed.store(false, std::memory_order_relaxed);
    return when;
}
#endif

int SqlMutationContext::dispatch(const SqlMutationEvent& ev, const Datasource& ds, ExceptionSink* xsink) {
    assert(xsink);

    ReferenceHolder<ResolvedCallReferenceNode> cb(xsink);
    ReferenceHolder<QoreHashNode> decl(xsink);
    ValueHolder arg(xsink);
    const int tid = q_gettid();

    {
        AutoLocker al(m);
        if (!observer || !(event_mask.load(std::memory_order_relaxed) & ev.getMaskBit())) {
            return 0;
        }
        ThreadState& ts = tmap[tid];
        if (ts.depth) {
            // the operation was issued from inside an observer callback in this thread; suppress the
            // event so that an observer can query the same datasource without recursing.  The entry
            // cannot be pruned here: a non-zero depth is what keeps it alive.
            return 0;
        }
        if (!ts.decls.empty()) {
            decl = ts.decls.back()->hashRefSelf();
        }
        ++ts.depth;
        cb = observer->refRefSelf();
        arg = observer_arg.refSelf();
    }

    // ensure the re-entrancy depth is always restored
    ON_BLOCK_EXIT_OBJ(*this, &SqlMutationContext::endDispatch, tid);

    ExceptionSink cb_xsink;

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclSqlMutationEventInfo, &cb_xsink), &cb_xsink);
    if (cb_xsink) {
        xsink->assimilate(cb_xsink);
        return ev.isAdmission() ? -1 : 0;
    }

    qore_hash_private* ph = qore_hash_private::get(**h);
    ph->setKeyValueIntern("event", (int64)ev.event);
    ph->setKeyValueIntern("stmt_class", (int64)ev.stmt_class);
    ph->setKeyValueIntern("driver", new QoreStringNode(ds.getDriverName()));
    const char* str = ds.getDBName();
    if (str && *str) {
        ph->setKeyValueIntern("db", new QoreStringNode(str));
    }
    str = ds.getUsername();
    if (str && *str) {
        ph->setKeyValueIntern("user", new QoreStringNode(str));
    }
    ph->setKeyValueIntern("tid", (int64)tid);
    ph->setKeyValueIntern("tx_id", new QoreStringNode(ev.tx_id ? ev.tx_id : ""));
    ph->setKeyValueIntern("tx_seq", ev.tx_seq);
    ph->setKeyValueIntern("in_transaction", ev.in_transaction);
    ph->setKeyValueIntern("autocommit", ev.autocommit);
    ph->setKeyValueIntern("declared", (bool)decl);
    if (decl) {
        ph->setKeyValueIntern("info", decl.release());
    }
    if (ev.outcome) {
        ph->setKeyValueIntern("outcome", (int64)ev.outcome);
    }
    if (ev.replay_safe >= 0) {
        ph->setKeyValueIntern("replay_safe", (bool)ev.replay_safe);
    }
    if (ev.declared_bytes >= 0) {
        ph->setKeyValueIntern("declared_bytes", ev.declared_bytes);
    }
    if (ev.consumed_bytes >= 0) {
        ph->setKeyValueIntern("consumed_bytes", ev.consumed_bytes);
    }
    if (ev.driver_xsink && *ev.driver_xsink) {
        // the exception is read without being consumed; the driver outcome is not affected
        QoreException* e = ev.driver_xsink->getException();
        if (e) {
            ph->setKeyValueIntern("ex", e->makeExceptionObject());
        }
    }
    ph->setKeyValueIntern("arg", arg.release());

    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &cb_xsink);
    args->push(h.release(), &cb_xsink);

    // the observer is always called with a clean ExceptionSink so that it never runs with an
    // exception already active, and so that a driver exception is always reported first
    ValueHolder rv(cb->execValue(*args, &cb_xsink), &cb_xsink);

    if (cb_xsink) {
        xsink->assimilate(cb_xsink);
        // an observer exception is fail-closed at an admission point and inert everywhere else: an
        // outcome that has already happened can never be changed by the observer
        return ev.isAdmission() ? -1 : 0;
    }

    if (!ev.isAdmission() || rv->getType() != NT_HASH) {
        return 0;
    }

    const QoreHashNode* dh = rv->get<const QoreHashNode>();
    bool exists = false;
    QoreValue av = dh->getKeyValueExistence("admit", exists);
    if (!exists || av.isNothing() || av.getAsBool()) {
        return 0;
    }

    QoreStringNodeValueHelper err(dh->getKeyValue("err"));
    QoreStringNodeValueHelper desc(dh->getKeyValue("desc"));

    xsink->raiseException(err->empty() ? SQL_MUTATION_REJECTED_ERR : err->c_str(), "%s",
        desc->empty()
            ? "the operation was rejected by the datasource mutation observer and was not executed"
            : desc->c_str());
    return -1;
}

void SqlMutationContext::endDispatch(int tid) {
    AutoLocker al(m);
    tmap_t::iterator i = tmap.find(tid);
    assert(i != tmap.end());
    assert(i->second.depth > 0);
    --i->second.depth;
    pruneThreadIntern(i);
}
