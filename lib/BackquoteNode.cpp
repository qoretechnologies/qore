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
#ifdef __linux__
#include <spawn.h>
#endif

#ifdef __linux__
extern char **environ;
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/types.h>
#include <sys/wait.h>
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

QoreStringNode* backquoteEval(const char* cmd, int& rc, ExceptionSink* xsink) {
    rc = 0;
    QoreSandboxManagerHelper smh;
    const bool use_pgroup = (bool)smh;
#if defined(__linux__) && defined(HAVE_FORK) && defined(HAVE_SIGNAL_HANDLING) && defined(HAVE_POLL)
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
                posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
                posix_spawn_file_actions_addclose(&actions, pipefd[0]);
                posix_spawn_file_actions_addclose(&actions, pipefd[1]);

                const char* argv[] = {"sh", "-c", cmd, nullptr};
                spawn_rc = posix_spawn(&pid, "/bin/sh", &actions, &attr, const_cast<char* const*>(argv), environ);
            }
        }

        if (spawn_rc != 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            if (spawn_rc == -1) {
                xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
            } else {
                xsink->raiseException("BACKQUOTE-ERROR", q_strerror(spawn_rc));
            }
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
            if (qore_check_io_interrupt(xsink, "backquote read")) {
                // Use SIGKILL to enforce immediate termination on interrupt.
                kill((use_pgroup && pgroup_ok) ? -pid : pid, SIGKILL);
                waitpid(pid, &rc, 0);
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
        int status;
        if (waitpid(pid, &status, 0) != pid) {
            rc = -1;
            return s.release();
        }
        rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return s.release();
    }
#endif
#if !defined(__linux__) && defined(HAVE_FORK) && defined(HAVE_SIGNAL_HANDLING) && defined(HAVE_POLL)
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
            if (qore_check_io_interrupt(xsink, "backquote read")) {
                // Use SIGKILL to enforce immediate termination on interrupt.
                kill((use_pgroup && pgroup_ok) ? -pid : pid, SIGKILL);
                waitpid(pid, &rc, 0);
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
        int status;
        if (waitpid(pid, &status, 0) != pid) {
            rc = -1;
            return s.release();
        }
        rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return s.release();
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
        if (qore_check_io_interrupt(xsink, "backquote read")) {
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
