/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
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

#ifndef _QORE_THREADPOOL_H
#define _QORE_THREADPOOL_H

#define QTP_DEFAULT_RELEASE_MS 5000

#include <deque>
#include <qore/qlist>

class ThreadTask;
class ThreadPoolThread;

typedef std::deque<ThreadTask*> taskq_t;
typedef qlist<ThreadPoolThread*> tplist_t;

class ThreadTask {
public:
    DLLLOCAL ThreadTask(ResolvedCallReferenceNode* c, ResolvedCallReferenceNode* cc) : code(c), cancelCode(cc) {
    }

    DLLLOCAL ~ThreadTask() {
        assert(!code);
        assert(!cancelCode);
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        code->deref(xsink);
        if (cancelCode)
            cancelCode->deref(xsink);
#ifdef DEBUG
        code = nullptr;
        cancelCode = nullptr;
#endif
        delete this;
    }

    DLLLOCAL QoreValue run(ExceptionSink* xsink) {
        // Get the program from the call reference and set up program context
        // This ensures tlpd is set up for thread pool threads
        QoreProgram* pgm = code->getProgram();
        assert(pgm);
        QoreExternalProgramContextHelper pch(xsink, pgm);
        if (*xsink) {
            return QoreValue();
        }
        return code->execValue(0, xsink);
    }

    DLLLOCAL void cancel(ExceptionSink* xsink) {
        if (cancelCode)
            cancelCode->execValue(0, xsink).discard(xsink);
    }

protected:
    ResolvedCallReferenceNode* code;
    ResolvedCallReferenceNode* cancelCode;
};

class ThreadTaskHolder {
protected:
   ThreadTask* task;
   ExceptionSink* xsink;

public:
   DLLLOCAL ThreadTaskHolder(ThreadTask* t, ExceptionSink* xs) : task(t), xsink(xs) {
   }

   DLLLOCAL ~ThreadTaskHolder() {
      if (task) {
         task->del(xsink);
      }
   }

   DLLLOCAL ThreadTask* release() {
      ThreadTask* rv = task;
      task = 0;
      return rv;
   }
};

class ThreadPool;

class ThreadPoolThread {
protected:
    int id;
    ThreadPool& tp;
    ThreadTask* task = nullptr;
    QoreCondition c,
        *stopCond = nullptr;
    QoreThreadLock m;
    tplist_t::iterator pos;
    bool stopflag = false,
        stopped = false;

    DLLLOCAL void finalize(ExceptionSink* xsink);

public:
    DLLLOCAL ThreadPoolThread(ThreadPool& n_tp, ExceptionSink* xsink);

    DLLLOCAL ~ThreadPoolThread() {
        delete stopCond;
        assert(!task);
    }

    DLLLOCAL void setPos(tplist_t::iterator p) {
        pos = p;
    }

    DLLLOCAL bool valid() const {
        return id != -1;
    }

    DLLLOCAL void worker(ExceptionSink* xsink);

    DLLLOCAL void stop() {
        AutoLocker al(m);
        assert(!stopflag);
        stopflag = true;
        c.signal();
        //printd(5, "ThreadPoolThread::stop() signaling stop for id %d\n", id);
    }

    DLLLOCAL void stopWait() {
        assert(!stopCond);
        stopCond = new QoreCondition;

        //printd(5, "ThreadPoolThread::stopWait() stopping id %d\n", id);
        AutoLocker al(m);

        assert(!stopflag);
        stopflag = true;
        c.signal();
    }

    DLLLOCAL void stopConfirm(ExceptionSink* xsink) {
        {
            AutoLocker al(m);
            assert(stopflag);
            assert(stopCond);
            while (!stopped)
                stopCond->wait(m);
        }

        //printd(5, "ThreadPoolThread::stopConfirm() stopped id %d\n", id);
        finalize(xsink);
    }

    DLLLOCAL void submit(ThreadTask* t) {
        AutoLocker al(m);
        assert(!stopflag);
        assert(!task);
        task = t;
        c.signal();
    }

    DLLLOCAL int getId() const {
        return id;
    }

    DLLLOCAL tplist_t::iterator getPos() const {
        return pos;
    }
};

class ThreadPool : public AbstractPrivateData {
protected:
    int max,        // maximum number of threads in pool (if <= 0 then unlimited)
        minidle,     // minimum number of idle threads
        maxidle,     // maximum number of idle threads
        release_ms;  // number of milliseconds before idle threads are released when > minidle

    // mutex for atomicity
    QoreThreadLock m;

    // worker thread condition variable
    QoreCondition cond;

    // stop condition variable
    QoreCondition stopCond;

    tplist_t ah,  // allocated thread list
        fh;        // free thread list

    // quit flag
    bool quit = false;

    // master task queue
    taskq_t q;

    // task waiting flag
    bool waiting = false;

    bool shutdown = false,     // shutdown flag
        stopflag = false,      // stop flag
        stopped = false,       // stopped flag
        detach = false;        // detach flag

    DLLLOCAL int checkStopUnlocked(const char* m, ExceptionSink* xsink) {
        if (shutdown) {
            xsink->raiseException("THREADPOOL-ERROR", "ThreadPool::%s() cannot be executed because the ThreadPool " \
                "is being destroyed", m);
            return -1;
        }
        return 0;
    }

    DLLLOCAL int addIdleWorker(ExceptionSink* xsink) {
        assert(xsink);
        std::unique_ptr<ThreadPoolThread> tpth(new ThreadPoolThread(*this, xsink));
        if (!tpth->valid()) {
            assert(*xsink);
            return -1;
        }

        ThreadPoolThread* tpt = tpth.release();
        fh.push_back(tpt);
#ifdef DEBUG
        // set to an invalid iterator
        tpt->setPos(fh.end());
#endif
        return 0;
    }

    DLLLOCAL ThreadPoolThread* getThreadUnlocked(ExceptionSink* xsink) {
        assert(xsink);
        while (!stopflag && fh.empty() && max && (int)ah.size() == max) {
            waiting = true;
            cond.wait(m);
            waiting = false;
        }

        if (stopflag)
            return nullptr;

        ThreadPoolThread* tpt;

        if (!fh.empty()) {
            tpt = fh.front();
            fh.pop_front();
        } else {
            std::unique_ptr<ThreadPoolThread> tpt_pt(new ThreadPoolThread(*this, xsink));
            if (!tpt_pt->valid()) {
                assert(*xsink);
                return nullptr;
            }
            tpt = tpt_pt.release();
        }

        ah.push_back(tpt);
        tplist_t::iterator i = ah.end();
        --i;
        tpt->setPos(i);
        return tpt;
    }

public:
    DLLLOCAL ThreadPool(ExceptionSink* xsink, int n_max = 0, int n_minidle = 0, int m_maxidle = 0, int n_release_ms = QTP_DEFAULT_RELEASE_MS);

    DLLLOCAL ~ThreadPool() {
        assert(q.empty());
        assert(ah.empty());
        assert(fh.empty());
        assert(stopped);
    }

    DLLLOCAL void toString(QoreString& str) {
        AutoLocker al(m);

        str.sprintf("ThreadPool %p total: %lu max: %d minidle: %d maxidle: %d release_ms: %d running: [", this,
            ah.size() + fh.size(), max, minidle, maxidle, release_ms);
        for (tplist_t::iterator i = ah.begin(), e = ah.end(); i != e; ++i) {
            if (i != ah.begin())
                str.concat(", ");
            str.sprintf("%d", (*i)->getId());
        }

        str.concat("] idle: [");

        for (tplist_t::iterator i = fh.begin(), e = fh.end(); i != e; ++i) {
            if (i != fh.begin())
                str.concat(", ");
            str.sprintf("%d", (*i)->getId());
        }

        str.concat(']');
    }

    // does not return until the thread pool has been stopped
    DLLLOCAL void stop() {
        AutoLocker al(m);
        if (!stopflag) {
            detach = true;
            shutdown = true;
            stopflag = true;
            cond.signal();
        }

        while (!stopped) {
            stopCond.wait(m);
        }
    }

    DLLLOCAL int stopWait(ExceptionSink* xsink) {
        AutoLocker al(m);
        if (detach && !stopped) {
            xsink->raiseException("THREADPOOL-ERROR", "cannot call ThreadPool::stopWait() after ThreadPool::stop() " \
                "has been called since worker threads have been detached and can no longer be traced");
            return -1;
        }

        if (!shutdown) {
            shutdown = true;
            cond.signal();
        }

        while (!stopped) {
            stopCond.wait(m);
        }

        return 0;
    }

    DLLLOCAL int submit(ResolvedCallReferenceNode* c, ResolvedCallReferenceNode* cc, ExceptionSink* xsink) {
        // optimistically create the task object outside the lock
        ThreadTaskHolder task(new ThreadTask(c, cc), xsink);

        AutoLocker al(m);
        if (checkStopUnlocked("submit", xsink))
            return -1;

        if (q.empty())
            cond.signal();
        q.push_back(task.release());

        return 0;
    }

    DLLLOCAL void threadCounts(int& idle, int& running) {
        AutoLocker al(m);
        idle = fh.size();
        running = ah.size();
    }

    DLLLOCAL int done(ThreadPoolThread* tpt) {
        {
            AutoLocker al(m);
            // allow the thread to be removed from the active list by ThreadPool::worker() to avoid race conditions
            if (stopflag) {
                return 0;
            }

            tplist_t::iterator i = tpt->getPos();
            ah.erase(i);

            // requeue thread if possible
            if ((!maxidle && release_ms) || ((int)fh.size() < maxidle) || q.size() > fh.size()) {
                fh.push_back(tpt);
                if (waiting || (release_ms && (int)fh.size() > minidle))
                    cond.signal();
                return 0;
            }
        }

        return -1;
    }

    DLLLOCAL void worker(ExceptionSink* xsink);
};

#endif
