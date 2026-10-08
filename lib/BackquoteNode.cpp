/*
    BackquoteNode.cpp

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
#include <qore/QoreSandboxManager.h>

#include <cerrno>
#include <csignal>
#include <unistd.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
// a command is started with posix_spawn() where available: unlike fork(), it runs no code in the child before the
// command is executed, which a multithreaded process must not do (another thread may hold a lock the child needs)
#if defined(HAVE_POSIX_SPAWN) && defined(HAVE_SPAWN_H) && defined(HAVE_SIGNAL_HANDLING) && defined(HAVE_POLL)
#define QORE_BACKQUOTE_POSIX_SPAWN 1
#include <spawn.h>
#ifdef DARWIN
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/types.h>
#include <sys/wait.h>
#endif
#ifndef _Q_WINDOWS
#include <pthread.h>
#endif

BackquoteNode::BackquoteNode(const QoreProgramLocation* loc, char *c_str) : ParseNode(loc, NT_BACKQUOTE), str(c_str) {
}

BackquoteNode::~BackquoteNode() {
    if (str)
        free(str);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int BackquoteNode::getAsString(QoreString &qstr, int foff, ExceptionSink *xsink) const {
    qstr.sprintf("backquote '%s' (%p)", str ? str : "<null>", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *BackquoteNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char *BackquoteNode::getTypeName() const {
    return "backquote expression";
}

// eval(): return value requires a deref(xsink)
QoreValue BackquoteNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    int rc;
    return backquoteEval(str, rc, xsink);
}

#ifndef READ_BLOCK
#define READ_BLOCK 1024
#endif

#ifndef _Q_WINDOWS
int qore_reap_child_process(pid_t pid, int& status) {
    while (true) {
        pid_t rc = waitpid(pid, &status, 0);
        if (rc == pid) {
            return 0;
        }
        assert(rc == -1);
        if (errno != EINTR) {
            return -1;
        }
    }
}

namespace {
// the state that qore_wait_child_process() shares with its helper thread, which blocks until the process ends
class QoreChildProcessWaiter {
public:
    DLLLOCAL QoreChildProcessWaiter(pid_t pid) : pid(pid) {
    }

    // the helper thread: waits for the process to end without reaping it, so that its PID stays valid for kill()
    // until the caller reaps it
    DLLLOCAL static void* run(void* arg) {
        QoreChildProcessWaiter* w = static_cast<QoreChildProcessWaiter*>(arg);
        int err = 0;
        while (true) {
            siginfo_t info;
            if (!waitid(P_PID, static_cast<id_t>(w->pid), &info, WEXITED | WNOWAIT)) {
                break;
            }
            if (errno != EINTR) {
                err = errno;
                break;
            }
        }
        AutoLocker al(w->m);
        w->wait_errno = err;
        w->done = true;
        w->cond.broadcast();
        return nullptr;
    }

    pid_t pid;
    QoreThreadLock m;
    QoreCondition cond;
    // the errno value of waitid() if it failed
    int wait_errno = 0;
    // set when the process has ended or waitid() failed
    bool done = false;
};
}

QoreChildWaitResult qore_wait_child_process(pid_t pid, pid_t kill_target, int& status, int& wait_errno,
        const char* err, ExceptionSink* xsink) {
    QoreChildProcessWaiter w(pid);
    pthread_t ptid;
    int rc;
    {
        // the helper thread inherits a mask blocking all signals: signals are handled by other threads
        sigset_t all, old;
        sigfillset(&all);
        rc = pthread_sigmask(SIG_SETMASK, &all, &old);
        if (!rc) {
            rc = pthread_create(&ptid, nullptr, QoreChildProcessWaiter::run, &w);
            int mrc = pthread_sigmask(SIG_SETMASK, &old, nullptr);
            assert(!mrc);
            (void)mrc;
        }
    }
    if (rc) {
        // the process cannot be waited for with cancellation; it is not left running unsupervised
        kill(kill_target, SIGKILL);
        int ignored;
        qore_reap_child_process(pid, ignored);
        xsink->raiseErrnoException(err, rc, "cannot start a thread to wait for process %d", static_cast<int>(pid));
        return QoreChildWaitResult::RAISED;
    }

    // joins the helper thread on every path out of this function, so that it never outlives the state it uses
    class ThreadJoiner {
    public:
        DLLLOCAL ThreadJoiner(pthread_t ptid) : ptid(ptid) {
        }

        DLLLOCAL ~ThreadJoiner() {
            join();
        }

        DLLLOCAL void join() {
            if (joinable) {
                joinable = false;
                int jrc = pthread_join(ptid, nullptr);
                assert(!jrc);
                (void)jrc;
            }
        }

    private:
        pthread_t ptid;
        bool joinable = true;
    } joiner(ptid);

    bool interrupted = false;
    {
        AutoLocker al(w.m);
        while (!w.done) {
            // this wait is woken when the thread is cancelled or its Program is interrupted
            if (w.cond.waitWithInterrupt(w.m) == QORE_COND_RESULT_INTERRUPTED
                && qore_check_cancel(xsink, "process wait")) {
                interrupted = true;
                break;
            }
        }
    }
    if (interrupted) {
        // the process has not been reaped, so its PID is still valid; SIGKILL ends it at once
        kill(kill_target, SIGKILL);
        AutoLocker al(w.m);
        while (!w.done) {
            w.cond.wait(w.m);
        }
    }
    joiner.join();

    if (interrupted) {
        if (!w.wait_errno) {
            int ignored;
            qore_reap_child_process(pid, ignored);
        }
        return QoreChildWaitResult::RAISED;
    }
    if (w.wait_errno) {
        wait_errno = w.wait_errno;
        return QoreChildWaitResult::WAIT_FAILED;
    }
    int st = 0;
    if (qore_reap_child_process(pid, st)) {
        wait_errno = errno;
        return QoreChildWaitResult::WAIT_FAILED;
    }
    status = st;
    return QoreChildWaitResult::EXITED;
}
#endif

QoreStringNode* backquoteEval(const char* cmd, int& rc, ExceptionSink* xsink) {
    rc = 0;
    QoreSandboxManagerHelper smh;
    const bool use_pgroup = (bool)smh;
#ifdef QORE_BACKQUOTE_POSIX_SPAWN
    {
        int pipefd[2];
        if (pipe(pipefd)) {
            xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
            return nullptr;
        }

        pid_t pid = -1;
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attr;
        bool actions_inited = false;
        bool attr_inited = false;
        int spawn_rc = posix_spawn_file_actions_init(&actions);
        if (spawn_rc == 0) {
            actions_inited = true;
            spawn_rc = posix_spawnattr_init(&attr);
            if (spawn_rc == 0) {
                attr_inited = true;
            }
        }
        if (spawn_rc == 0) {
            short flags = static_cast<short>(POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
            if (use_pgroup) {
                flags |= POSIX_SPAWN_SETPGROUP;
            }
            spawn_rc = posix_spawnattr_setflags(&attr, flags);

            if (spawn_rc == 0) {
                sigset_t empty;
                sigemptyset(&empty);
                spawn_rc = posix_spawnattr_setsigmask(&attr, &empty);
            }
            if (spawn_rc == 0) {
                sigset_t def;
                sigemptyset(&def);
                sigaddset(&def, SIGINT);
                sigaddset(&def, SIGQUIT);
                spawn_rc = posix_spawnattr_setsigdefault(&attr, &def);
            }
            if (spawn_rc == 0 && use_pgroup) {
                spawn_rc = posix_spawnattr_setpgroup(&attr, 0);
            }

            if (spawn_rc == 0) {
                spawn_rc = posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
            }
            if (spawn_rc == 0) {
                spawn_rc = posix_spawn_file_actions_addclose(&actions, pipefd[0]);
            }
            if (spawn_rc == 0) {
                spawn_rc = posix_spawn_file_actions_addclose(&actions, pipefd[1]);
            }
            if (spawn_rc == 0) {
                const char* argv[] = {"sh", "-c", cmd, nullptr};
                spawn_rc = posix_spawn(&pid, "/bin/sh", &actions, &attr, const_cast<char* const*>(argv), environ);
            }
        }

        if (spawn_rc != 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            // posix_spawn() and the functions preparing it return an errno value and do not set errno
            xsink->raiseException("BACKQUOTE-ERROR", q_strerror(spawn_rc));
            if (actions_inited) {
                posix_spawn_file_actions_destroy(&actions);
            }
            if (attr_inited) {
                posix_spawnattr_destroy(&attr);
            }
            return nullptr;
        }

        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attr);
        close(pipefd[1]);
        bool pgroup_ok = false;
        if (use_pgroup) {
            // Best-effort safeguard in case POSIX_SPAWN_SETPGROUP is not applied.
            if (setpgid(pid, pid) == 0 || errno == EACCES) {
                pgroup_ok = true;
            }
        }

        QoreStringNodeHolder s(new QoreStringNode);
        char buf[READ_BLOCK];
        while (true) {
            if (qore_check_cancel(xsink, "backquote read")) {
                // Use SIGKILL to enforce immediate termination on interrupt.
                kill((use_pgroup && pgroup_ok) ? -pid : pid, SIGKILL);
                int ignored;
                qore_reap_child_process(pid, ignored);
                close(pipefd[0]);
                rc = -1;
                return nullptr;
            }

            struct pollfd pfd;
            pfd.fd = pipefd[0];
            pfd.events = POLLIN;
            pfd.revents = 0;
            int prc = poll(&pfd, 1, QORE_IO_POLL_INTERVAL_MS);
            if (prc == 0) {
                continue;
            }
            if (prc < 0) {
#ifdef EINTR
                if (errno == EINTR) {
                    continue;
                }
#endif
                break;
            }

            int size = read(pipefd[0], buf, READ_BLOCK);
            if (size == 0) {
                break;
            }
            if (size < 0) {
#ifdef EINTR
                if (errno == EINTR) {
                    continue;
                }
#endif
                break;
            }

            s->concat(buf, size);
        }

        close(pipefd[0]);
        // the command may go on after closing its output; the wait can be cancelled
        int status = 0;
        int wait_errno = 0;
        switch (qore_wait_child_process(pid, (use_pgroup && pgroup_ok) ? -pid : pid, status, wait_errno,
            "BACKQUOTE-ERROR", xsink)) {
            case QoreChildWaitResult::EXITED:
                rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                return s.release();
            case QoreChildWaitResult::WAIT_FAILED:
                rc = -1;
                return s.release();
            case QoreChildWaitResult::RAISED:
                break;
        }
        assert(*xsink);
        rc = -1;
        return nullptr;
    }
#endif
#if !defined(QORE_BACKQUOTE_POSIX_SPAWN) && defined(HAVE_FORK) && defined(HAVE_SIGNAL_HANDLING) && defined(HAVE_POLL)
    {
        int pipefd[2];
        if (pipe(pipefd)) {
            xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
            return nullptr;
        }

        pid_t pid = fork();
        bool pgroup_ok = false;
        if (!pid) {
            if (use_pgroup) {
                setpgid(0, 0);
            }

            sigset_t empty;
            sigemptyset(&empty);
            sigprocmask(SIG_SETMASK, &empty, nullptr);
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            execl("/bin/sh", "sh", "-c", cmd, NULL);
            fprintf(stderr, "execl() failed in child process for target '/bin/sh' with error code %d: %s\n", errno,
                strerror(errno));
            _Exit(-1);
        }
        if (pid == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
            return nullptr;
        }
        if (use_pgroup && (setpgid(pid, pid) == 0 || errno == EACCES)) {
            pgroup_ok = true;
        }
        close(pipefd[1]);

        QoreStringNodeHolder s(new QoreStringNode);
        char buf[READ_BLOCK];
        while (true) {
            if (qore_check_cancel(xsink, "backquote read")) {
                // Use SIGKILL to enforce immediate termination on interrupt.
                kill((use_pgroup && pgroup_ok) ? -pid : pid, SIGKILL);
                int ignored;
                qore_reap_child_process(pid, ignored);
                close(pipefd[0]);
                rc = -1;
                return nullptr;
            }

            struct pollfd pfd;
            pfd.fd = pipefd[0];
            pfd.events = POLLIN;
            pfd.revents = 0;
            int prc = poll(&pfd, 1, QORE_IO_POLL_INTERVAL_MS);
            if (prc == 0) {
                continue;
            }
            if (prc < 0) {
#ifdef EINTR
                if (errno == EINTR) {
                    continue;
                }
#endif
                break;
            }

            int size = read(pipefd[0], buf, READ_BLOCK);
            if (size == 0) {
                break;
            }
            if (size < 0) {
#ifdef EINTR
                if (errno == EINTR) {
                    continue;
                }
#endif
                break;
            }

            s->concat(buf, size);
        }

        close(pipefd[0]);
        // the command may go on after closing its output; the wait can be cancelled
        int status = 0;
        int wait_errno = 0;
        switch (qore_wait_child_process(pid, (use_pgroup && pgroup_ok) ? -pid : pid, status, wait_errno,
            "BACKQUOTE-ERROR", xsink)) {
            case QoreChildWaitResult::EXITED:
                rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                return s.release();
            case QoreChildWaitResult::WAIT_FAILED:
                rc = -1;
                return s.release();
            case QoreChildWaitResult::RAISED:
                break;
        }
        assert(*xsink);
        rc = -1;
        return nullptr;
    }
#endif

    // execute command in a new process and read stdout in parent
    FILE* p = popen(cmd, "r");
    if (!p) {
        // could not fork or create pipe
        xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
        return 0;
    }

    // allocate buffer for return value
    QoreStringNodeHolder s(new QoreStringNode);

    // read in result string
    while (true) {
        // check for interrupt before blocking read
        if (qore_check_cancel(xsink, "backquote read")) {
            pclose(p);
            rc = -1;
            return nullptr;
        }

        char buf[READ_BLOCK];
        int size = fread(buf, 1, READ_BLOCK, p);

        // break if no data is available or an error occurred
        if (!size || size == -1)
            break;

        s->concat(buf, size);

        // break if there is no more data
        if (size != READ_BLOCK)
            break;
    }

    // wait for child process to terminate and close pipe
    rc = pclose(p);
#ifdef HAVE_SYS_WAIT_H
    if (WIFEXITED(rc))
        rc = WEXITSTATUS(rc);
#endif
    return s.release();
}
