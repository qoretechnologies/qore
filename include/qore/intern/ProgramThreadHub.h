/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ProgramThreadHub.h

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

#ifndef _QORE_INTERN_PROGRAMTHREADHUB_H
#define _QORE_INTERN_PROGRAMTHREADHUB_H

#include <qore/Qore.h>
#include <qore/QoreQueue.h>

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

//! the hashdecls of Program thread lifecycle events; see design/program-thread-lifecycle-events.md
DLLLOCAL extern const TypedHashDecl* hashdeclProgramThreadEventOptions;
DLLLOCAL extern const TypedHashDecl* hashdeclProgramThreadInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclProgramThreadSubscription;
DLLLOCAL extern const TypedHashDecl* hashdeclProgramThreadEvent;

//! the event bits of Program thread lifecycle subscriptions (Qore constants PTE_*)
#define QORE_PTE_STARTED          (1 << 0)
#define QORE_PTE_CANCEL_REQUESTED (1 << 1)
#define QORE_PTE_FINISHED         (1 << 2)
#define QORE_PTE_ACTIVITY         (1 << 3)
#define QORE_PTE_CLOSED           (1 << 4)
//! every event type
#define QORE_PTE_ALL (QORE_PTE_STARTED | QORE_PTE_CANCEL_REQUESTED | QORE_PTE_FINISHED | QORE_PTE_ACTIVITY \
    | QORE_PTE_CLOSED)
//! the events delivered by default: every event but ACTIVITY
#define QORE_PTE_DEFAULT (QORE_PTE_STARTED | QORE_PTE_CANCEL_REQUESTED | QORE_PTE_FINISHED | QORE_PTE_CLOSED)

//! how a background thread of a Program ended
enum class ProgramThreadOutcome {
    COMPLETED,      //!< the thread's code returned without an exception
    EXCEPTION,      //!< an exception other than the ones below ended the thread
    CANCELLED,      //!< THREAD-CANCELLED ended the thread
    INTERRUPTED,    //!< PROGRAM-INTERRUPTED or SANDBOX-TIMEOUT ended the thread
    EXITED,         //!< the thread called thread_exit
    START_FAILED,   //!< the Program's thread entry handler failed, so the thread's code did not run
};

//! returns the name of an outcome, as delivered in the "outcome" key of FINISHED events
DLLLOCAL const char* program_thread_outcome_name(ProgramThreadOutcome outcome);

//! returns the outcome of a background thread from the exceptions that its code left in its sink
/** @param xsink the thread's sink, right after its code returned
    @param entered false if the thread's entry into its Program failed, so its code did not run
*/
DLLLOCAL ProgramThreadOutcome program_thread_get_outcome(const ExceptionSink& xsink, bool entered);

//! returns a new process-unique ID for a background thread
DLLLOCAL int64 program_thread_new_id();

//! returns the number of exceptions in a sink
DLLLOCAL int64 program_thread_count_exceptions(ExceptionSink& xsink);

//! returns a hash<ExceptionInfo> for a lifecycle event of the exception at the given index in a sink, or nullptr
/** The hash has no chained exceptions (\c next), and its \c err, \c desc and \c arg keys are left empty if their
    value refers to objects or code: an event must not extend the life of the thread's values, and its values must
    need no garbage-collection scan in the queues that receive it.
*/
DLLLOCAL QoreHashNode* program_thread_event_error(ExceptionSink& xsink, int64 index);

//! the identity of a background thread, recorded by the thread that starts it
struct ProgramThreadIdentity {
    //! the thread's process-unique ID
    int64 thread_id = 0;
    //! the TID of the thread that started it
    int parent_tid = 0;
    //! the process-unique ID of the thread that started it, or 0 if that thread is not a background thread
    int64 parent_thread_id = 0;
};

//! publishes the lifecycle events of the background threads of one Program to subscribed Queues
/** The hub is reference-counted: the Program holds a reference, and each background thread of the Program holds one
    from its start until it has published its FINISHED event, as the Program can be destroyed before then.

    The hub's lock is a leaf lock: no other lock is taken while it is held but the lock of a subscription's Queue,
    and no Qore code runs while it is held.  Events are pushed to the queues under the lock, so every queue receives
    the events in the order of their sequence numbers.  A subscription's queue has no maximum size, so a push never
    blocks; the values of events refer to no objects or code, so a push never needs a garbage-collection scan and
    releasing an event never runs Qore code.

    See design/program-thread-lifecycle-events.md.
*/
class ProgramThreadHub {
public:
    DLLLOCAL ProgramThreadHub(qore_program_id_t pgm_id);

    DLLLOCAL ProgramThreadHub(const ProgramThreadHub&) = delete;
    DLLLOCAL ProgramThreadHub& operator=(const ProgramThreadHub&) = delete;

    DLLLOCAL void ref() {
        refs.fetch_add(1, std::memory_order_relaxed);
    }

    //! releases a reference; the last one deletes the hub
    DLLLOCAL void deref();

    //! records a background thread that has started in the Program and publishes STARTED
    /** Called by the thread itself, after it has been registered in the Program and before its code runs.
    */
    DLLLOCAL void threadStarted(int tid, const ProgramThreadIdentity& id);

    //! records that a background thread has ended and publishes FINISHED
    /** Called by the thread itself, after it has left the Program and its thread-local data has been destroyed, and
        before its TID is released.

        @param thread_id the thread's ID
        @param outcome how the thread ended
        @param error the first exception that ended the thread, or nullptr; the reference is taken
        @param exception_count the number of exceptions that ended the thread
        @param cleanup_error the first exception raised by the thread's teardown, or nullptr; the reference is taken
    */
    DLLLOCAL void threadFinished(int64 thread_id, ProgramThreadOutcome outcome, QoreHashNode* error,
            int64 exception_count, QoreHashNode* cleanup_error);

    //! publishes CANCEL_REQUESTED for a live thread, once for each claim; does nothing once the thread has published
    //! FINISHED
    /** Called without the thread list's lock by the canceller after it claimed the request, and by the thread itself
        for all of its claims before it publishes FINISHED (see QoreLifecycleCancelClaim), so a claim can be published
        twice: the second time does nothing.

        @param thread_id the thread
        @param gen the claim's generation
        @param reason the reason given with the request, or nullptr
    */
    DLLLOCAL void cancelRequested(int64 thread_id, uint64_t gen, const char* reason);

    //! records a transition of the Program between idle and active and publishes ACTIVITY
    /** @param gen the Program's activity generation of the transition; a transition older than the last one
        recorded is ignored, as transitions are published after the Program's lock is released and can arrive out of
        order
        @param idle true if the Program became idle, false if it became active
    */
    DLLLOCAL void activityChanged(uint64_t gen, bool idle);

    //! returns true if a subscription takes ACTIVITY events; the Program checks this after a transition
    DLLLOCAL bool hasActivityInterest() const {
        return activity_subs.load(std::memory_order_seq_cst);
    }

    //! records the interest of a subscription in ACTIVITY events, before the Program's activity state is read
    /** subscribe() takes over the interest, and removes it if it fails
    */
    DLLLOCAL void addActivityInterest();

    //! subscribes a queue to the hub's events
    /** @param queue the queue, which must have no maximum size; the reference is taken, also on error
        @param events the event bits to deliver (QORE_PTE_*); if QORE_PTE_ACTIVITY is set, addActivityInterest()
        must have been called first
        @param gen the Program's activity generation, read after addActivityInterest()
        @param idle true if the Program was idle at \a gen
        @param xsink for errors

        @return a ProgramThreadSubscription hash with the snapshot of the live background threads, or nullptr if an
        exception was raised
    */
    DLLLOCAL QoreHashNode* subscribe(Queue* queue, int64 events, uint64_t gen, bool idle, ExceptionSink* xsink);

    //! removes a subscription; returns false if it does not exist
    DLLLOCAL bool unsubscribe(int64 id, ExceptionSink* xsink);

    //! the Program is being destroyed: publishes CLOSED once every live thread has published FINISHED
    /** Idempotent; the subscriptions are removed when CLOSED is published.
    */
    DLLLOCAL void close(ExceptionSink* xsink);

    //! returns the number of live background threads
    DLLLOCAL size_t getRunningCount() const;

    //! fixes up the hub in a child process created by fork()
    /** Only the forking thread exists in the child: the records of the other threads are dropped without events,
        with the references they held, and the activity state is set to the child's.

        @param tid the forking thread
        @param gen the Program's activity generation in the child
        @param idle true if the Program is idle in the child
    */
    DLLLOCAL void forkChild(int tid, uint64_t gen, bool idle);

private:
    //! a live background thread
    struct ThreadRecord {
        int tid;
        ProgramThreadIdentity id;
        //! the name of the thread when it started
        std::string name;
        //! when the thread started (wall clock)
        int64 start_epoch_us;
        //! when the thread started (monotonic clock), for its duration
        int64 start_mono_us;
        //! the time zone of the thread's dates: the zone of its Program when it started
        const AbstractQoreZoneInfo* zone;
        //! the order in which the thread started in the Program
        uint64_t start_order = 0;
        //! true once a cancellation request has been published for the thread
        bool cancel_requested = false;
        //! the generations of the cancellation claims published for the thread
        std::set<uint64_t> cancel_gens;
    };

    //! a subscription
    struct Subscription {
        int64 id;
        Queue* queue;
        int64 events;
        //! the activity generation of the subscription's snapshot, then of the last ACTIVITY event delivered to it
        /** A transition at or below it is not delivered: the subscription's snapshot already reflects it
        */
        uint64_t activity_gen;
    };

    typedef std::map<int64, ThreadRecord> thread_map_t;
    typedef std::vector<Subscription> sub_list_t;

    //! the ID of the Program
    qore_program_id_t pgm_id;

    std::atomic<int> refs{1};

    //! the leaf lock
    mutable QoreThreadLock lck;

    //! the sequence number of the last event published
    int64 seq = 0;

    //! the number of threads started, for their order
    uint64_t start_counter = 0;

    //! the live background threads by thread ID
    thread_map_t threads;

    //! the subscriptions
    sub_list_t subs;

    //! true once the Program is being destroyed
    bool closed = false;

    //! true once CLOSED has been published
    bool close_published = false;

    //! the number of subscriptions that take ACTIVITY events, including ones being created
    std::atomic<int> activity_subs{0};

    //! the activity generation of the last transition published
    /** Only publication advances it: a subscription that reads a newer state records it for itself
        (Subscription::activity_gen), so that the transition is still published to the other subscriptions
    */
    uint64_t activity_gen = 0;

    //! true if the Program was idle at activity_gen
    bool activity_idle = true;

    //! removes the interest of a subscription in ACTIVITY events
    DLLLOCAL void removeActivityInterest();

    DLLLOCAL ~ProgramThreadHub();

    //! returns a new event hash with the common keys of a thread event set
    DLLLOCAL QoreHashNode* makeThreadEvent(const char* event, const ThreadRecord& rec) const;

    //! assigns the next sequence number to the event and pushes a copy to each subscription that takes it
    /** Must be called with the lock held; the event's reference is not taken.

        @param event_bit the event's bit
        @param event the event
        @param gen for ACTIVITY, the transition's generation: a subscription whose snapshot is at least as recent
        is skipped
    */
    DLLLOCAL void publishIntern(int64 event_bit, QoreHashNode* event, uint64_t gen = 0);

    //! publishes CLOSED and moves the subscriptions to the given list, whose queues must be released after the lock
    /** Must be called with the lock held.
    */
    DLLLOCAL void publishClosedIntern(sub_list_t& released);

    //! releases the queues of removed subscriptions; called without the lock
    DLLLOCAL static void releaseQueues(sub_list_t& released, ExceptionSink* xsink);
};

//! holds a reference to a hub and releases it at the end of the scope
class ProgramThreadHubHolder {
public:
    DLLLOCAL ProgramThreadHubHolder(ProgramThreadHub* hub = nullptr) : hub(hub) {
    }

    DLLLOCAL ~ProgramThreadHubHolder() {
        if (hub) {
            hub->deref();
        }
    }

    DLLLOCAL ProgramThreadHubHolder(const ProgramThreadHubHolder&) = delete;
    DLLLOCAL ProgramThreadHubHolder& operator=(const ProgramThreadHubHolder&) = delete;

    DLLLOCAL ProgramThreadHub* operator->() const {
        return hub;
    }

    DLLLOCAL ProgramThreadHub* operator*() const {
        return hub;
    }

    DLLLOCAL explicit operator bool() const {
        return hub;
    }

    //! takes a reference; the holder must be empty
    DLLLOCAL void set(ProgramThreadHub* h) {
        assert(!hub);
        hub = h;
    }

    //! releases the reference
    DLLLOCAL void reset() {
        if (hub) {
            hub->deref();
            hub = nullptr;
        }
    }

private:
    ProgramThreadHub* hub;
};

//! publishes a transition of a Program between idle and active when it goes out of scope
/** Declared before the lock of the Program's thread count (plock) is taken, so that the transition is published
    after the lock is released.  It holds a reference to the hub, as the Program can be destroyed by another thread
    once its thread count has been released.
*/
class ProgramThreadActivityPublisher {
public:
    DLLLOCAL ProgramThreadActivityPublisher() = default;

    DLLLOCAL ~ProgramThreadActivityPublisher() {
        if (hub) {
            hub->activityChanged(gen, idle);
            hub->deref();
        }
    }

    DLLLOCAL ProgramThreadActivityPublisher(const ProgramThreadActivityPublisher&) = delete;
    DLLLOCAL ProgramThreadActivityPublisher& operator=(const ProgramThreadActivityPublisher&) = delete;

    //! records a transition to publish; called with the Program's lock held
    DLLLOCAL void set(ProgramThreadHub* h, uint64_t g, bool i) {
        assert(!hub);
        h->ref();
        hub = h;
        gen = g;
        idle = i;
    }

private:
    ProgramThreadHub* hub = nullptr;
    uint64_t gen = 0;
    bool idle = false;
};

#endif
