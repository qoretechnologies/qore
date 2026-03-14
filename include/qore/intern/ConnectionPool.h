/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ConnectionPool.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_CONNECTIONPOOL_H
#define _QORE_INTERN_CONNECTIONPOOL_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/QoreThreadLock.h>
#include <qore/AbstractThreadResource.h>

#include <vector>
#include <unordered_map>
#include <deque>

//! Generic connection pool with per-thread allocation (C++ implementation)
/** Follows the DatasourcePool pattern: inherits AbstractThreadResource directly
    so the pool object IS the thread resource registered via set_thread_resource().
    The QPP class does NOT use vparent=AbstractThreadResource; ATR is internal.
*/
class ConnectionPool : public AbstractThreadResource, public QoreCondition, public QoreThreadLock {
protected:
    QoreObject& obj;

    std::vector<QoreObject*> pool;        //!< resource pool slots (nullptr = tombstone)
    std::vector<int64> last_released;     //!< timestamps parallel to pool
    std::unordered_map<int, int> tmap;    //!< thread ID -> pool slot index
    std::deque<int> free_list;            //!< available slot indices (LIFO)

    int live_count = 0;
    int creating_count = 0;
    int min_size;
    int max_size;
    int64 peak_size = 0;
    int wait_count_val = 0;
    int64 total_acquires = 0;
    int64 total_timeouts = 0;
    int64 timeout_ms;
    int64 idle_timeout_ms;
    bool valid = true;

    ResolvedCallReferenceNode* warning_callback = nullptr;
    QoreValue warning_arg;
    int64 warning_ms = 0;

    //! Creates a new resource (calls back into Qore)
    virtual QoreObject* createResourceImpl(ExceptionSink* xsink) = 0;

    //! Resets a resource (calls back into Qore)
    virtual void resetResourceImpl(QoreObject* resource, ExceptionSink* xsink) = 0;

    //! Destroys a resource (calls back into Qore)
    virtual void destroyResourceImpl(QoreObject* resource, ExceptionSink* xsink) = 0;

    //! Returns stats snapshot (must be called under lock)
    DLLLOCAL QoreHashNode* getStatsIntern(ExceptionSink* xsink) const;

public:
    DLLLOCAL ConnectionPool(QoreObject& obj, int min, int max, int64 timeout,
        int64 idle_timeout);
    DLLLOCAL virtual ~ConnectionPool();

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            delete this;
        }
    }

    //! AbstractThreadResource::cleanup() — called when thread exits without release
    DLLLOCAL virtual void cleanup(ExceptionSink* xsink) override;

    //! Pre-creates minimum resources (call after setPrivate in QPP constructor)
    DLLLOCAL void init(ExceptionSink* xsink);

    //! Acquires a resource for the current thread
    DLLLOCAL QoreObject* acquire(ExceptionSink* xsink);

    //! Releases the current thread's resource
    DLLLOCAL void release(ExceptionSink* xsink);

    //! Shuts down the pool
    DLLLOCAL void shutdown(ExceptionSink* xsink);

    //! Destructor cleanup
    DLLLOCAL void destructor(ExceptionSink* xsink);

    //! Returns pool statistics
    DLLLOCAL QoreHashNode* getStats(ExceptionSink* xsink) const;

    //! Removes idle free resources above minimum
    DLLLOCAL int cleanupIdle(ExceptionSink* xsink);

    //! Sets a warning callback for long wait times
    DLLLOCAL void setWarningCallback(int64 wms, ResolvedCallReferenceNode* cb, QoreValue arg,
        ExceptionSink* xsink);

    //! Clears the warning callback
    DLLLOCAL void clearWarningCallback(ExceptionSink* xsink);
};

//! Qore-bridging ConnectionPool subclass that dispatches virtual calls to Qore methods
class QoreConnectionPool : public ConnectionPool {
protected:
    DLLLOCAL virtual QoreObject* createResourceImpl(ExceptionSink* xsink) override;
    DLLLOCAL virtual void resetResourceImpl(QoreObject* resource, ExceptionSink* xsink) override;
    DLLLOCAL virtual void destroyResourceImpl(QoreObject* resource, ExceptionSink* xsink) override;

public:
    DLLLOCAL QoreConnectionPool(QoreObject& obj, int min, int max, int64 timeout,
        int64 idle_timeout);
};

#endif
