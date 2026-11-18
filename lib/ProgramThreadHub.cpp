/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ProgramThreadHub.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/ProgramThreadHub.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/qore_thread_intern.h"

#include <algorithm>
#include <chrono>
#include <cstring>

const TypedHashDecl* hashdeclProgramThreadEventOptions = nullptr;
const TypedHashDecl* hashdeclProgramThreadInfo = nullptr;
const TypedHashDecl* hashdeclProgramThreadSubscription = nullptr;
const TypedHashDecl* hashdeclProgramThreadEvent = nullptr;

namespace {
//! the last background thread ID handed out
std::atomic<int64> program_thread_id_counter{0};

//! the last subscription ID handed out; subscription IDs are unique in the process
std::atomic<int64> program_thread_sub_id_counter{0};

//! returns the current wall-clock time in microseconds since the epoch
int64 pth_epoch_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

//! returns a date in the given time zone for a wall-clock time in microseconds since the epoch
DateTimeNode* pth_make_date(const AbstractQoreZoneInfo* zone, int64 us) {
    int64 secs = us / 1000000;
    int64 rem = us % 1000000;
    if (rem < 0) {
        rem += 1000000;
        --secs;
    }
    return DateTimeNode::makeAbsolute(zone, secs, static_cast<int>(rem));
}

//! returns the error string of an exception, or nullptr if it is not a string
const char* pth_get_err_string(const QoreValue& err) {
    if (err.getType() != NT_STRING) {
        return nullptr;
    }
    return err.get<const QoreStringNode>()->c_str();
}
}

const char* program_thread_outcome_name(ProgramThreadOutcome outcome) {
    switch (outcome) {
        case ProgramThreadOutcome::COMPLETED: return "COMPLETED";
        case ProgramThreadOutcome::EXCEPTION: return "EXCEPTION";
        case ProgramThreadOutcome::CANCELLED: return "CANCELLED";
        case ProgramThreadOutcome::INTERRUPTED: return "INTERRUPTED";
        case ProgramThreadOutcome::EXITED: return "EXITED";
        case ProgramThreadOutcome::START_FAILED: return "START_FAILED";
    }
    assert(false);
    return "EXCEPTION";
}

ProgramThreadOutcome program_thread_get_outcome(const ExceptionSink& xsink, bool entered) {
    if (!entered) {
        return ProgramThreadOutcome::START_FAILED;
    }
    // the first exception is the one that ended the thread's code; any others were raised while it propagated
    ExceptionSink& sink = const_cast<ExceptionSink&>(xsink);
    QoreException* e = sink.getException();
    if (!e) {
        return xsink.isThreadExit() ? ProgramThreadOutcome::EXITED : ProgramThreadOutcome::COMPLETED;
    }
    const char* err = pth_get_err_string(sink.getExceptionErr());
    if (err) {
        if (!strcmp(err, "THREAD-CANCELLED")) {
            return ProgramThreadOutcome::CANCELLED;
        }
        // the Program was interrupted, or a sandbox ended the thread for reaching a limit
        if (!strcmp(err, "PROGRAM-INTERRUPTED") || !strcmp(err, "SANDBOX-TIMEOUT")
                || !strcmp(err, "SANDBOX-MEMORY-LIMIT")) {
            return ProgramThreadOutcome::INTERRUPTED;
        }
    }
    return ProgramThreadOutcome::EXCEPTION;
}

int64 program_thread_new_id() {
    return program_thread_id_counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

int64 program_thread_count_exceptions(ExceptionSink& xsink) {
    int64 rv = 0;
    for (QoreException* e = xsink.getException(); e; e = e->next) {
        ++rv;
    }
    return rv;
}

QoreHashNode* program_thread_event_error(ExceptionSink& xsink, int64 index) {
    QoreException* e = xsink.getException();
    for (int64 i = 0; e && i < index; ++i) {
        e = e->next;
    }
    if (!e) {
        return nullptr;
    }
    QoreHashNode* h = e->makeExceptionObjectWithoutChain();
    qore_hash_private* ph = qore_hash_private::get(*h);
    for (const char* key : {"err", "desc", "arg"}) {
        // a key without a value needs no scan
        if (needs_scan(h->getKeyValue(key))) {
            // the exception still holds the value, so releasing this reference runs no destructor
            ph->setKeyValueIntern(key, QoreValue());
        }
    }
    return h;
}

ProgramThreadHub::ProgramThreadHub(qore_program_id_t pgm_id) : pgm_id(pgm_id) {
}

ProgramThreadHub::~ProgramThreadHub() {
    // a hub is deleted when the Program and every thread that published to it have released it; a Program that was
    // never closed (released without being torn down) still has its subscriptions
    if (!subs.empty()) {
        ExceptionSink xsink;
        releaseQueues(subs, &xsink);
        xsink.handleExceptions();
    }
}

void ProgramThreadHub::deref() {
    if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
    }
}

void ProgramThreadHub::releaseQueues(sub_list_t& released, ExceptionSink* xsink) {
    for (Subscription& sub : released) {
        sub.queue->deref(xsink);
    }
    released.clear();
}

QoreHashNode* ProgramThreadHub::makeThreadEvent(const char* event, const ThreadRecord& rec) const {
    QoreHashNode* h = new QoreHashNode(hashdeclProgramThreadEvent, nullptr);
    qore_hash_private* ph = qore_hash_private::get(*h);
    ph->setKeyValueIntern("program_id", static_cast<int64>(pgm_id));
    ph->setKeyValueIntern("event", new QoreStringNode(event));
    ph->setKeyValueIntern("tid", static_cast<int64>(rec.tid));
    ph->setKeyValueIntern("thread_id", rec.id.thread_id);
    ph->setKeyValueIntern("parent_tid", static_cast<int64>(rec.id.parent_tid));
    ph->setKeyValueIntern("parent_thread_id", rec.id.parent_thread_id);
    ph->setKeyValueIntern("name", new QoreStringNode(rec.name));
    ph->setKeyValueIntern("start", pth_make_date(rec.zone, rec.start_epoch_us));
    return h;
}

void ProgramThreadHub::publishIntern(int64 event_bit, QoreHashNode* event, uint64_t gen) {
    qore_hash_private* ph = qore_hash_private::get(*event);
    ph->setKeyValueIntern("seq", ++seq);
    for (Subscription& sub : subs) {
        if (!(sub.events & event_bit)) {
            continue;
        }
        if (event_bit == QORE_PTE_ACTIVITY) {
            // the subscription's snapshot already has this state
            if (gen <= sub.activity_gen) {
                continue;
            }
            sub.activity_gen = gen;
        }
        QoreHashNode* h = event->copy();
        qore_hash_private::get(*h)->setKeyValueIntern("subscription_id", sub.id);
        // the queue has no maximum size, so this never blocks; it takes the reference, and releases it if the queue
        // is in an error state, which runs no Qore code, as the event refers to no objects or code
        sub.queue->pushAndTakeRef(h);
    }
}

void ProgramThreadHub::publishClosedIntern(sub_list_t& released) {
    assert(closed && !close_published && threads.empty());
    close_published = true;
    if (!subs.empty()) {
        QoreHashNode* h = new QoreHashNode(hashdeclProgramThreadEvent, nullptr);
        ReferenceHolder<QoreHashNode> holder(h, nullptr);
        qore_hash_private* ph = qore_hash_private::get(*h);
        ph->setKeyValueIntern("program_id", static_cast<int64>(pgm_id));
        ph->setKeyValueIntern("event", new QoreStringNode("CLOSED"));
        ph->setKeyValueIntern("running", static_cast<int64>(0));
        publishIntern(QORE_PTE_CLOSED, h);
    }
    // only the interest of the subscriptions removed: a subscription being made has added its interest, and removes
    // it when it finds the hub closed
    int removed = 0;
    for (const Subscription& sub : subs) {
        if (sub.events & QORE_PTE_ACTIVITY) {
            ++removed;
        }
    }
    released.swap(subs);
    if (removed) {
        activity_subs.fetch_sub(removed, std::memory_order_seq_cst);
    }
}

void ProgramThreadHub::threadStarted(int tid, const ProgramThreadIdentity& id) {
    ThreadRecord rec;
    rec.tid = tid;
    rec.id = id;
#ifdef QORE_HAVE_THREAD_NAME
    {
        QoreString name;
        q_get_thread_name(name);
        rec.name = name.c_str();
    }
#endif
    // the time zone of the Program that the thread runs in, which the thread's dates use
    rec.zone = currentTZ();
    rec.start_epoch_us = pth_epoch_us();
    rec.start_mono_us = q_clock_getmicros_monotonic();

    // built before the lock and released after it
    ReferenceHolder<QoreHashNode> event(makeThreadEvent("STARTED", rec), nullptr);

    AutoLocker al(lck);
    assert(threads.find(id.thread_id) == threads.end());
    rec.start_order = ++start_counter;
    threads.insert(thread_map_t::value_type(id.thread_id, std::move(rec)));
    qore_hash_private::get(**event)->setKeyValueIntern("running", static_cast<int64>(threads.size()));
    publishIntern(QORE_PTE_STARTED, *event);
}

void ProgramThreadHub::cancelRequested(int64 thread_id, uint64_t gen, const char* reason) {
    ReferenceHolder<QoreHashNode> event(nullptr);
    {
        AutoLocker al(lck);
        thread_map_t::iterator i = threads.find(thread_id);
        // the thread has already published FINISHED
        if (i == threads.end()) {
            return;
        }
        // the claim has been published by the canceller or by the thread
        if (!i->second.cancel_gens.insert(gen).second) {
            return;
        }
        i->second.cancel_requested = true;
        event = makeThreadEvent("CANCEL_REQUESTED", i->second);
        qore_hash_private* ph = qore_hash_private::get(**event);
        ph->setKeyValueIntern("running", static_cast<int64>(threads.size()));
        if (reason) {
            ph->setKeyValueIntern("cancel_reason", new QoreStringNode(reason));
        }
        publishIntern(QORE_PTE_CANCEL_REQUESTED, *event);
    }
}

void ProgramThreadHub::cancelClaimReleased(int64 thread_id, uint64_t gen) {
    AutoLocker al(lck);
    thread_map_t::iterator i = threads.find(thread_id);
    if (i != threads.end()) {
        i->second.cancel_gens.erase(gen);
    }
}

#ifdef DEBUG
int64 ProgramThreadHub::getCancelGenerationCount(int64 thread_id) const {
    AutoLocker al(lck);
    thread_map_t::const_iterator i = threads.find(thread_id);
    return i == threads.end() ? -1 : static_cast<int64>(i->second.cancel_gens.size());
}
#endif

void ProgramThreadHub::threadFinished(int64 thread_id, ProgramThreadOutcome outcome, QoreHashNode* error,
        int64 exception_count, QoreHashNode* cleanup_error) {
    ReferenceHolder<QoreHashNode> error_holder(error, nullptr);
    ReferenceHolder<QoreHashNode> cleanup_error_holder(cleanup_error, nullptr);

    int64 end_epoch_us = pth_epoch_us();
    int64 end_mono_us = q_clock_getmicros_monotonic();
    QoreString name;
#ifdef QORE_HAVE_THREAD_NAME
    q_get_thread_name(name);
#endif

    ReferenceHolder<QoreHashNode> event(nullptr);
    sub_list_t released;
    {
        AutoLocker al(lck);
        thread_map_t::iterator i = threads.find(thread_id);
        assert(i != threads.end());
        if (i == threads.end()) {
            return;
        }
        if (name.empty()) {
            name.set(i->second.name.c_str());
        } else {
            i->second.name = name.c_str();
        }
        event = makeThreadEvent("FINISHED", i->second);
        const AbstractQoreZoneInfo* zone = i->second.zone;
        int64 duration_us = end_mono_us - i->second.start_mono_us;
        threads.erase(i);

        qore_hash_private* ph = qore_hash_private::get(**event);
        ph->setKeyValueIntern("running", static_cast<int64>(threads.size()));
        ph->setKeyValueIntern("end", pth_make_date(zone, end_epoch_us));
        ph->setKeyValueIntern("duration_us", duration_us < 0 ? 0 : duration_us);
        ph->setKeyValueIntern("outcome", new QoreStringNode(program_thread_outcome_name(outcome)));
        ph->setKeyValueIntern("exception_count", exception_count);
        if (error) {
            ph->setKeyValueIntern("error", error_holder.release());
        }
        if (cleanup_error) {
            ph->setKeyValueIntern("cleanup_error", cleanup_error_holder.release());
        }
        publishIntern(QORE_PTE_FINISHED, *event);

        // the last thread of a Program being destroyed closes the hub
        if (closed && !close_published && threads.empty()) {
            publishClosedIntern(released);
        }
    }
    if (!released.empty()) {
        ExceptionSink xsink;
        releaseQueues(released, &xsink);
        // releasing a queue can run the destructors of values that its consumer pushed to it; their exceptions are
        // reported as the thread's own exceptions are, and do not affect the Program
        xsink.handleExceptions();
    }
}

void ProgramThreadHub::activityChanged(uint64_t gen, bool idle) {
    ReferenceHolder<QoreHashNode> event(nullptr);
    {
        AutoLocker al(lck);
        // a later transition has already been published: this one arrived out of order
        if (gen <= activity_gen) {
            return;
        }
        activity_gen = gen;
        activity_idle = idle;
        if (subs.empty()) {
            return;
        }
        event = new QoreHashNode(hashdeclProgramThreadEvent, nullptr);
        qore_hash_private* ph = qore_hash_private::get(**event);
        ph->setKeyValueIntern("program_id", static_cast<int64>(pgm_id));
        ph->setKeyValueIntern("event", new QoreStringNode("ACTIVITY"));
        ph->setKeyValueIntern("running", static_cast<int64>(threads.size()));
        ph->setKeyValueIntern("idle", idle);
        publishIntern(QORE_PTE_ACTIVITY, *event, gen);
    }
}

void ProgramThreadHub::addActivityInterest() {
    activity_subs.fetch_add(1, std::memory_order_seq_cst);
}

void ProgramThreadHub::removeActivityInterest() {
    int prev = activity_subs.fetch_sub(1, std::memory_order_seq_cst);
    assert(prev > 0);
    (void)prev;
}

QoreHashNode* ProgramThreadHub::subscribe(Queue* queue, int64 events, uint64_t gen, bool idle,
        ExceptionSink* xsink) {
    // the activity interest was added by the caller before it read the activity state
    bool activity = events & QORE_PTE_ACTIVITY;
    if (queue->getMax() != -1) {
        xsink->raiseException("PROGRAM-THREAD-EVENT-ERROR", "the Queue object passed has a maximum size of " QLLD
            " entr%s; a Queue object with no maximum size is required, so that publishing an event never blocks",
            static_cast<int64>(queue->getMax()), queue->getMax() == 1 ? "y" : "ies");
        if (activity) {
            removeActivityInterest();
        }
        queue->deref(xsink);
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclProgramThreadSubscription, nullptr), xsink);
    ReferenceHolder<QoreListNode> l(new QoreListNode(hashdeclProgramThreadInfo->getTypeInfo()), xsink);

    {
        AutoLocker al(lck);
        if (!closed) {
            // the activity state read by the caller can be newer than the last one published, whose transition is
            // then published later: the subscription records it, so that it is not delivered the transition that
            // its snapshot already has, while the other subscriptions are
            uint64_t sub_gen = activity_gen;
            if (gen > activity_gen) {
                sub_gen = gen;
            } else {
                idle = activity_idle;
            }

            int64 id = program_thread_sub_id_counter.fetch_add(1, std::memory_order_relaxed) + 1;
            subs.push_back(Subscription{id, queue, events, sub_gen});

            // in the order the threads started: thread IDs are given by the starting threads, which can start
            // them in another order
            std::vector<const ThreadRecord*> ordered;
            ordered.reserve(threads.size());
            for (auto& i : threads) {
                ordered.push_back(&i.second);
            }
            std::sort(ordered.begin(), ordered.end(), [](const ThreadRecord* a, const ThreadRecord* b) {
                return a->start_order < b->start_order;
            });
            for (const ThreadRecord* recp : ordered) {
                const ThreadRecord& rec = *recp;
                QoreHashNode* h = new QoreHashNode(hashdeclProgramThreadInfo, nullptr);
                qore_hash_private* ph = qore_hash_private::get(*h);
                ph->setKeyValueIntern("tid", static_cast<int64>(rec.tid));
                ph->setKeyValueIntern("thread_id", rec.id.thread_id);
                ph->setKeyValueIntern("parent_tid", static_cast<int64>(rec.id.parent_tid));
                ph->setKeyValueIntern("parent_thread_id", rec.id.parent_thread_id);
                ph->setKeyValueIntern("name", new QoreStringNode(rec.name));
                ph->setKeyValueIntern("start", pth_make_date(rec.zone, rec.start_epoch_us));
                ph->setKeyValueIntern("cancel_requested", rec.cancel_requested);
                l->push(h, nullptr);
            }

            qore_hash_private* ph = qore_hash_private::get(**rv);
            ph->setKeyValueIntern("id", id);
            ph->setKeyValueIntern("program_id", static_cast<int64>(pgm_id));
            ph->setKeyValueIntern("seq", seq);
            ph->setKeyValueIntern("idle", idle);
            ph->setKeyValueIntern("threads", l.release());
            return rv.release();
        }
    }

    xsink->raiseException("PROGRAM-THREAD-EVENT-ERROR", "the Program is being destroyed; its threads can no longer "
        "be observed");
    if (activity) {
        removeActivityInterest();
    }
    queue->deref(xsink);
    return nullptr;
}

bool ProgramThreadHub::unsubscribe(int64 id, ExceptionSink* xsink) {
    sub_list_t released;
    {
        AutoLocker al(lck);
        for (sub_list_t::iterator i = subs.begin(), e = subs.end(); i != e; ++i) {
            if (i->id == id) {
                if (i->events & QORE_PTE_ACTIVITY) {
                    removeActivityInterest();
                }
                released.push_back(*i);
                subs.erase(i);
                break;
            }
        }
    }
    if (released.empty()) {
        return false;
    }
    releaseQueues(released, xsink);
    return true;
}

void ProgramThreadHub::close(ExceptionSink* xsink) {
    sub_list_t released;
    {
        AutoLocker al(lck);
        if (closed) {
            return;
        }
        closed = true;
        // threads that have left the Program but have not published FINISHED yet close the hub when the last one
        // has
        if (threads.empty()) {
            publishClosedIntern(released);
        }
    }
    releaseQueues(released, xsink);
}

size_t ProgramThreadHub::getRunningCount() const {
    AutoLocker al(lck);
    return threads.size();
}

void ProgramThreadHub::forkChild(int tid, uint64_t gen, bool idle) {
    // the lock may have been held by a thread of the parent
    lck.reinitAfterFork();
    int dropped = 0;
    for (thread_map_t::iterator i = threads.begin(); i != threads.end();) {
        if (i->second.tid == tid) {
            ++i;
            continue;
        }
        i = threads.erase(i);
        ++dropped;
    }
    // each thread held a reference to the hub; the Program holds one as well, so the hub is not deleted here
    if (dropped) {
        int prev = refs.fetch_sub(dropped, std::memory_order_acq_rel);
        assert(prev > dropped);
        (void)prev;
    }
    activity_gen = gen;
    activity_idle = idle;

    // a Program being destroyed whose remaining threads did not exist in the child is closed now
    sub_list_t released;
    if (closed && !close_published && threads.empty()) {
        publishClosedIntern(released);
    }
    if (!released.empty()) {
        ExceptionSink xsink;
        releaseQueues(released, &xsink);
        xsink.handleExceptions();
    }
}
