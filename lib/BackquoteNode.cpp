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
#ifdef DARWIN
#include <libproc.h>
#include <sys/event.h>
#include <set>
#include <vector>
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
// waits for a child process to end without reaping it, so that its PID, and the ID of the process group that it
// leads, stay reserved until it is reaped; returns 0 when the process has ended, or the errno value of waitid()
int qore_wait_child_exit(pid_t pid) {
    while (true) {
        siginfo_t info;
        if (!waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOWAIT)) {
            return 0;
        }
        if (errno != EINTR) {
            return errno;
        }
    }
}

#ifdef DARWIN
// the time that the processes of a killed process group are waited for while none of them ends
constexpr int QORE_PGROUP_EXIT_STALL_MS = 2000;

// returns the PIDs of the processes in the process group, including the zombies listed by the system
void qore_list_pgroup_pids(pid_t pgid, std::vector<pid_t>& pids) {
    // proc_listpgrppids() with no buffer returns an estimate of the number of processes; the list is read again with
    // a larger buffer while it fills the buffer, as processes may be created in the meantime
    int cap = proc_listpgrppids(pgid, nullptr, 0);
    if (cap < 16) {
        cap = 16;
    }
    while (true) {
        pids.resize(cap);
        int n = proc_listpgrppids(pgid, pids.data(), static_cast<int>(cap * sizeof(pid_t)));
        if (n < cap) {
            pids.resize(n > 0 ? n : 0);
            return;
        }
        cap *= 2;
    }
}

// closes a kqueue descriptor on every path out of its scope
class QoreKqueueHolder {
public:
    QoreKqueueHolder() : fd(kqueue()) {
    }

    ~QoreKqueueHolder() {
        if (fd >= 0) {
            close(fd);
        }
    }

    int fd;
};
#endif

// called after the child process \a pid, killed with SIGKILL sent to \a kill_target, has ended and before it is
// reaped: when \a kill_target is the process group that the child leads, every other process in the group is killed
/* The child's PID, which is the ID of the group, cannot be reused until it is reaped, so the signals sent here
   cannot reach another process group.

   On Linux, a signal sent to a process group reaches every process in the group, including one that a member is
   creating with fork() at that moment (the kernel restarts a fork() that a signal interrupts), so the first SIGKILL
   has reached every process in the group, and nothing is done here.

   On macOS, a process that a member of the group is creating with fork() when the signal is sent does not receive
   it, and is left running in the group.  The member that created it has been sent SIGKILL, and it ends once fork()
   returns, so the new process is in the group before its creator has ended.  The other processes in the group are
   therefore listed and waited for with kqueue (EVFILT_PROC, NOTE_EXIT):
   - SIGKILL is sent to the group, and the processes in the group are listed;
   - when the list has a process that was not waited for, or a process that has ended that was not known to have
     ended, the group may have a process created after the signal was sent: the signal is sent and the group listed
     again;
   - otherwise, every process in the group has been sent SIGKILL after it was created, and every one of them is waited
     for; a process created after the list was made was created by one of them, and is in the group when its creator
     has ended: when one or more of them end, the signal is sent and the group is listed again;
   - it ends when no process but the leader is left in the group.
   A killed process that does not end, for example one in an uninterruptible wait, would hold the wait indefinitely:
   the wait ends when no process in the group has ended for QORE_PGROUP_EXIT_STALL_MS.  Every process left in the
   group has been sent SIGKILL after it was created then, and it ends when the wait that holds it ends.

   Elsewhere, SIGKILL is sent to the group again once, which reaches every process that the leader itself created.
*/
void qore_kill_child_process_group(pid_t pid, pid_t kill_target) {
    if (kill_target == pid) {
        return;
    }
    assert(kill_target == -pid);
#if defined(__linux__)
    // the first signal has reached every process in the group
#elif defined(DARWIN)
    const pid_t pgid = pid;
    QoreKqueueHolder kq;
    if (kq.fd < 0) {
        // the processes cannot be waited for; the signal reaches every process that the leader itself created
        kill(kill_target, SIGKILL);
        return;
    }
    // the processes in the group that are waited for
    std::set<pid_t> waiting;
    // the processes in the group known to have ended; zombies are listed until they are reaped
    std::set<pid_t> ended;
    std::vector<pid_t> pids;
    while (true) {
        kill(kill_target, SIGKILL);
        qore_list_pgroup_pids(pgid, pids);
        bool changed = false;
        for (pid_t member : pids) {
            if (member == pgid || waiting.find(member) != waiting.end()) {
                continue;
            }
            struct kevent ev, receipt;
            EV_SET(&ev, member, EVFILT_PROC, EV_ADD | EV_ONESHOT | EV_RECEIPT, NOTE_EXIT, 0, nullptr);
            // the receipt reports the result of the registration: ESRCH if the process has ended
            if (kevent(kq.fd, &ev, 1, &receipt, 1, nullptr) == 1 && (receipt.flags & EV_ERROR) && !receipt.data) {
                // a process that has not been waited for, also one with the PID of a process that has ended
                waiting.insert(member);
                ended.erase(member);
                changed = true;
            } else if (ended.insert(member).second) {
                changed = true;
            }
        }
        if (changed) {
            continue;
        }
        if (waiting.empty()) {
            return;
        }
        struct kevent events[64];
        struct timespec timeout = {QORE_PGROUP_EXIT_STALL_MS / 1000, (QORE_PGROUP_EXIT_STALL_MS % 1000) * 1000000};
        int n = kevent(kq.fd, nullptr, 0, events, 64, &timeout);
        if (!n) {
            // no process in the group has ended within the stall time
            return;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        for (int i = 0; i < n; ++i) {
            pid_t member = static_cast<pid_t>(events[i].ident);
            waiting.erase(member);
            ended.insert(member);
        }
    }
#else
    kill(kill_target, SIGKILL);
#endif
}
}

void qore_kill_child_process(pid_t pid, pid_t kill_target) {
    kill(kill_target, SIGKILL);
    if (!qore_wait_child_exit(pid)) {
        qore_kill_child_process_group(pid, kill_target);
    }
    int ignored;
    qore_reap_child_process(pid, ignored);
}

namespace {
// the state that qore_wait_child_process() shares with its helper thread, which blocks until the process ends
class QoreChildProcessWaiter {
public:
    QoreChildProcessWaiter(pid_t pid) : pid(pid) {
    }

    // the helper thread: waits for the process to end without reaping it, so that its PID stays valid for kill()
    // until the caller reaps it
    static void* run(void* arg) {
        QoreChildProcessWaiter* w = static_cast<QoreChildProcessWaiter*>(arg);
        int err = qore_wait_child_exit(w->pid);
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
        qore_kill_child_process(pid, kill_target);
        xsink->raiseErrnoException(err, rc, "cannot start a thread to wait for process %d", static_cast<int>(pid));
        return QoreChildWaitResult::RAISED;
    }

    // joins the helper thread on every path out of this function, so that it never outlives the state it uses
    class ThreadJoiner {
    public:
        ThreadJoiner(pthread_t ptid) : ptid(ptid) {
        }

        ~ThreadJoiner() {
            join();
        }

        void join() {
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
            qore_kill_child_process_group(pid, kill_target);
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
    // the child runs in its own process group when a sandbox governs the thread, so that interrupting the sandbox
    // ends the whole command; this is not a policy check, and a system-policy capability does not change it
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);
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
                // the child gets a copy of the environment: it must not change while it is copied
                QoreEnvironmentReadLock env_lock;
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
        // posix_spawn() succeeded, so POSIX_SPAWN_SETPGROUP made the child the leader of a new process group before
        // the command was executed (calling setpgid() here instead fails once the child has ended, ESRCH on macOS,
        // and would kill only the child)
        const bool pgroup_ok = use_pgroup;

        QoreStringNodeHolder s(new QoreStringNode);
        char buf[READ_BLOCK];
        while (true) {
            // the wait ends at once when the thread is cancelled or its Program is interrupted
            struct pollfd pfd;
            pfd.fd = pipefd[0];
            pfd.events = POLLIN;
            pfd.revents = 0;
            int prc = qore_cancellable_poll(&pfd, 1, -1, xsink, "backquote read");
            if (prc == QORE_POLL_CANCELLED) {
                // SIGKILL ends the command at once on interrupt
                qore_kill_child_process(pid, (use_pgroup && pgroup_ok) ? -pid : pid);
                close(pipefd[0]);
                rc = -1;
                return nullptr;
            }
            if (prc < 0) {
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

        // the child gets a copy of the environment, which it passes to the command: the library's fork handlers hold
        // the environment lock while the process is copied
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
        // the child makes itself the leader of a new process group as well, so that the group exists whichever of
        // the two calls is made first: EACCES means the child has executed the command, and ESRCH (macOS) that it has
        // ended, after its own call
        if (use_pgroup && (setpgid(pid, pid) == 0 || errno == EACCES || errno == ESRCH)) {
            pgroup_ok = true;
        }
        close(pipefd[1]);

        QoreStringNodeHolder s(new QoreStringNode);
        char buf[READ_BLOCK];
        while (true) {
            // the wait ends at once when the thread is cancelled or its Program is interrupted
            struct pollfd pfd;
            pfd.fd = pipefd[0];
            pfd.events = POLLIN;
            pfd.revents = 0;
            int prc = qore_cancellable_poll(&pfd, 1, -1, xsink, "backquote read");
            if (prc == QORE_POLL_CANCELLED) {
                // SIGKILL ends the command at once on interrupt
                qore_kill_child_process(pid, (use_pgroup && pgroup_ok) ? -pid : pid);
                close(pipefd[0]);
                rc = -1;
                return nullptr;
            }
            if (prc < 0) {
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

    // execute command in a new process and read stdout in parent; the child gets a copy of the environment, which
    // must not change while it is copied
    FILE* p;
    {
        QoreEnvironmentReadLock env_lock;
        p = popen(cmd, "r");
    }
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
