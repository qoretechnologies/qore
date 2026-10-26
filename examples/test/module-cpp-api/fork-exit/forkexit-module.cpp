/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file forkexit-module.cpp test binary modules whose exit-time code is not safe in a child created with fork()

    Test scaffolding for qore_fork_child_exit_quickly(): this file is built twice, as the modules \c forkexita and
    \c forkexitb, which are built into the build tree but never installed.  Each stands for a module that uses a
    third-party library whose exit-time code is not safe in a child process created with \c fork():
    - \c forkexita registers an \c atexit() handler that blocks forever in a child (as one that joins a thread of
      the parent or takes a lock that a thread of the parent held)
    - \c forkexitb has a static object whose destructor aborts in a child (as one that destroys a kernel object that
      the child does not have); the deliberate abort leaves no core file

    The exit-time code does nothing unless the module is armed with a marker descriptor with \c arm().  Before it blocks
    or aborts, it writes a marker line to that descriptor, so that the test sees deterministically that it ran; in the
    process that loaded the module it writes another marker and returns.  Each module registers a child fork handler (at a different rank) that calls
    qore_fork_child_exit_quickly() twice if requested with \c arm().

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
*/

#include <qore/Qore.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

#if !defined(FORKEXIT_ID) || !defined(FORKEXIT_NAME) || !defined(FORKEXIT_NS) || !defined(FORKEXIT_ATEXIT) \
    || !defined(FORKEXIT_RANK)
#error FORKEXIT_ID, FORKEXIT_NAME, FORKEXIT_NS, FORKEXIT_ATEXIT and FORKEXIT_RANK must be defined
#endif

#define FORKEXIT_CAT2(a, b) a ## b
#define FORKEXIT_CAT(a, b) FORKEXIT_CAT2(a, b)
#define FORKEXIT_DESC_FUNC FORKEXIT_CAT(FORKEXIT_ID, _qore_module_desc)

namespace {
//! the descriptor that the exit-time code writes its marker to, -1 for none
std::atomic<int> marker_fd{-1};
//! true if the child fork handler requests that the child exits quickly
std::atomic<bool> request_quick_exit{false};
//! the process that loaded the module
pid_t init_pid = 0;
//! the registration of the fork handler
QoreForkHandlerRegistration* fork_reg = nullptr;
//! the namespace of the module
QoreNamespace* module_ns = nullptr;

//! writes a marker line to the marker descriptor, if any
void write_marker(const char* tag, const char* kind) {
    int fd = marker_fd.load();
    if (fd < 0) {
        return;
    }
    char buf[64];
    size_t len = strlen(tag);
    memcpy(buf, tag, len);
    buf[len++] = '-';
    size_t klen = strlen(kind);
    memcpy(buf + len, kind, klen);
    len += klen;
    buf[len++] = '\n';
    ssize_t rc = write(fd, buf, len);
    (void)rc;
}

//! the module's exit-time code: in a child created with fork(), it blocks forever (atexit) or aborts (destructor)
/** It does nothing unless the module is armed with a marker descriptor
*/
void run_exit_code() {
    if (marker_fd.load() < 0) {
        return;
    }
    if (getpid() == init_pid) {
        write_marker(FORKEXIT_NAME, "parent");
        return;
    }
#if FORKEXIT_ATEXIT
    write_marker(FORKEXIT_NAME, "hang");
    // blocks like code waiting for a thread that only existed in the parent; only a signal ends the process
    while (true) {
        pause();
    }
#else
    write_marker(FORKEXIT_NAME, "abort");
    // the abort is the expected outcome of this control case, not a crash: it leaves no core file, so that a core
    // file of a test run always means a real crash; the test sees the abort from the marker and the exit status
    struct rlimit no_core = {0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
    abort();
#endif
}

#if FORKEXIT_ATEXIT
extern "C" void forkexit_atexit_handler() {
    run_exit_code();
}
#else
//! a static object whose destructor runs when the process exits
class ForkExitStatic {
public:
    ~ForkExitStatic() {
        run_exit_code();
    }
};
ForkExitStatic forkexit_static;
#endif

//! the child fork handler: requests a quick exit (twice: the call is idempotent)
void forkexit_fork_child(void* arg) {
    if (request_quick_exit.load()) {
        qore_fork_child_exit_quickly();
        qore_fork_child_exit_quickly();
    }
}

//! arm(int fd, bool request): sets the marker descriptor and whether the child handler requests a quick exit
/** The module writes to a copy of the descriptor, which stays open when the program's File objects are destroyed at
    the end of the main program; -1 disarms the module
*/
QoreValue f_arm(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    int64 fd = get_param_value(args, 0).getAsBigInt();
    bool request = get_param_value(args, 1).getAsBool();
    int copy = -1;
    if (fd >= 0) {
        copy = dup(static_cast<int>(fd));
        if (copy < 0) {
            return xsink->raiseErrnoException("FORKEXIT-ERROR", errno, "dup() failed");
        }
    }
    int old = marker_fd.exchange(copy);
    if (old >= 0) {
        close(old);
    }
    request_quick_exit.store(request);
    return QoreValue();
}

//! request_now(): calls qore_fork_child_exit_quickly() outside of a fork handler, which must have no effect
QoreValue f_request_now(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    qore_fork_child_exit_quickly();
    return QoreValue();
}

//! exits_quickly(): returns qore_fork_child_exits_quickly()
QoreValue f_exits_quickly(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return qore_fork_child_exits_quickly();
}
}

static void forkexit_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_pid = getpid();
    module_ns = new QoreNamespace(FORKEXIT_NS);
    module_ns->addBuiltinVariant("arm", f_arm, QCF_NO_FLAGS, QDOM_PROCESS, nothingTypeInfo, 2, bigIntTypeInfo,
        QORE_PARAM_NO_ARG, "fd", boolTypeInfo, QORE_PARAM_NO_ARG, "request");
    module_ns->addBuiltinVariant("request_now", f_request_now, QCF_NO_FLAGS, QDOM_PROCESS, nothingTypeInfo);
    module_ns->addBuiltinVariant("exits_quickly", f_exits_quickly, QCF_NO_FLAGS, QDOM_PROCESS, boolTypeInfo);
    fork_reg = qore_register_fork_handler(FORKEXIT_RANK, FORKEXIT_NAME, nullptr, nullptr, forkexit_fork_child,
        nullptr);
    if (!fork_reg) {
        xsink.outOfMemory();
        return;
    }
#if FORKEXIT_ATEXIT
    if (atexit(forkexit_atexit_handler)) {
        xsink.raiseException("MODULE-INIT-ERROR", "atexit() failed");
    }
#endif
}

static void forkexit_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(module_ns->copy());
}

static void forkexit_module_delete() {
    qore_unregister_fork_handler(fork_reg);
    fork_reg = nullptr;
    delete module_ns;
    module_ns = nullptr;
}

extern "C" DLLEXPORT void FORKEXIT_DESC_FUNC(QoreModuleInfo& mod_info) {
    mod_info.name = FORKEXIT_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module whose exit-time code is not safe in a child process created with fork()";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = forkexit_module_init;
    mod_info.ns_init = forkexit_module_ns_init;
    mod_info.del = forkexit_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
