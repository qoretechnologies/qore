/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file staticpgm-module.cpp a test binary module that owns a Program at static scope

    Test scaffolding for process teardown: this module is built into the build tree but is never
    installed.  It does nothing except hold a QoreProgram in an object with static storage
    duration, which is what the jni module does with its global Java context Program.  Such a
    Program is destroyed from a static destructor while exit() runs, and its teardown calls back
    into libqore singletons - so this module is the minimal reproducer for a libqore singleton
    that does not outlive the Programs it serves.  See static-program-exit.qtest.

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

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

static void staticpgm_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void staticpgm_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void staticpgm_module_delete();

// This file is built twice: as "staticpgm" (an empty Program) and, with STATICPGM_CODE defined, as "staticpgmcode",
// whose Program holds an object with a Qore destructor that runs code at static destruction (see
// program-cleanup-at-exit.qtest and qore#5778)
#ifdef STATICPGM_CODE
#define STATICPGM_NAME "staticpgmcode"
#define STATICPGM_DESC_FUNC staticpgmcode_qore_module_desc
#else
#define STATICPGM_NAME "staticpgm"
#define STATICPGM_DESC_FUNC staticpgm_qore_module_desc
#endif

extern "C" DLLEXPORT void STATICPGM_DESC_FUNC(QoreModuleInfo& mod_info) {
    mod_info.name = STATICPGM_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module owning a Program with static storage duration";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = staticpgm_module_init;
    mod_info.ns_init = staticpgm_module_ns_init;
    mod_info.del = staticpgm_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

//! the ExceptionSink the Program helper below reports into
/** Declared before the helper on purpose: QoreProgramHelper keeps a reference to it and
    ~QoreProgramHelper() writes into it, and objects with static storage duration are destroyed in
    reverse order of construction, so the sink has to be constructed first to outlive the helper.
*/
static ExceptionSink module_xsink;

//! the Program this module keeps alive for the life of the process
/** The unique_ptr has static storage duration, so ~QoreProgramHelper() runs from a static
    destructor during exit() - after the module manager has already been torn down.  This is
    deliberately the same shape the jni module uses for its global Java context Program.
*/
static std::unique_ptr<QoreProgramHelper> qph;

#ifdef STATICPGM_CODE
//! the Program of the module if a foreign thread is left blocked in it (QORE_STATICPGM_FOREIGN_BLOCK)
/** the Program cannot be deleted while the thread runs in it, as the deletion would wait for the thread forever: the
    module leaves it as a host language does that abandons the threads that use it at exit (ex: the daemon threads
    of Python, see the python module); the pointer keeps it reachable
*/
static QoreProgramHelper* abandoned_qph = nullptr;

//! the code of a foreign thread that blocks in a call to the function "staticpgm_block" of the Program
/** the thread is registered with Qore and never ends: the process exits while it runs in the Program
*/
static void staticpgm_foreign_block(QoreProgram* pgm) {
    if (q_register_foreign_thread() != QFT_OK) {
        fprintf(stderr, "staticpgmcode: cannot register a foreign thread\n");
        return;
    }
    ExceptionSink xsink;
    pgm->callFunction("staticpgm_block", nullptr, &xsink);
    // the call returns only if it fails
    xsink.handleExceptions();
}
#endif

#ifdef STATICPGM_CODE
//! the code of the Program: a global object whose destructor runs code (closures, recursion, exceptions, containers)
//! when the Program is deleted by a static destructor, after the main thread's thread-local objects were destroyed
static const char* staticpgm_code = R"QORE(
%modern
%no-child-restrictions

int sub fib(int n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}
# channel_select() between two ready channels (the thread's scratch random generator)
int sub select_one() {
    Channel c1(1);
    Channel c2(1);
    c1.send(1);
    c2.send(2);
    hash<auto> r = channel_select(({"channel": c1, "op": "recv"}, {"channel": c2, "op": "recv"}));
    return r.value;
}

class Teardown {
    private {
        list<int> values = (1, 2, 3);
    }

    destructor() {
        # closures that capture locals, called repeatedly (closure caches, IR frames)
        int total = 0;
        code add = int sub (int i) { total += i; return total; };
        for (int i = 0; i < 300; ++i) {
            add(i);
        }
        # recursion (IR call frames)
        total += fib(12);
        # exceptions thrown and caught in a loop (catch stacks)
        for (int i = 0; i < 20; ++i) {
            try {
                if (i % 2) {
                    throw "ERR", "x";
                }
                total += i;
            } catch (hash<ExceptionInfo> ex) {
                total += ex.err.size();
            }
        }
        # containers, map and select
        hash<string, int> h = map {sprintf("k%d", $1): $1}, values;
        list<int> sel = (select values, $1 > 1);
        total += h.size() + sel.size();
        string s = join(",", map string($1), values);
        total += s.size();
        # a call reference and a nested closure
        code r = \fib();
        code nest = code sub () { return int sub (int j) { return j + total; }; };
        total += r(8) + nest()(1);
        # on_exit and on_success blocks (block exit handlers), a hash of closures, constants, string encodings
        for (int i = 0; i < 20; ++i) {
            on_exit total += 1;
            on_success total += 2;
            hash<string, code> hc = {"a": int sub (int x) { return x + total; }, "b": int sub (int x) { return x * 2; }};
            total += hc.a(1) + hc.b(2);
        }
        # channel_select() in the main thread from a static destructor at exit: exit() ends the process without
        # qore_cleanup(), so the thread's data, and its generator, are those that the Program used when it ran
        for (int i = 0; i < 5; ++i) {
            total += select_one();
        }
        string utf = convert_encoding("caf\xc3\xa9", "ISO-8859-1");
        total += utf.size() + convert_encoding(utf, "UTF-8").size();
        total += TeardownConst.size() + TeardownConst2.firstKey().size();
        if (total < 0) {
            printf("unexpected %d\n", total);
        }
        # the marker that program-cleanup-at-exit.qtest checks: the destructor ran to its end
        printf("staticpgmcode: teardown complete\n");
    }
}

const TeardownConst = (1, 2, 3, "a", {"b": 1});
const TeardownConst2 = {"first": TeardownConst[4], "second": (TeardownConst, 5)};

our Teardown teardown_object();
# channel_select() in the main thread when the Program runs, so its scratch generator exists before exit
our int selected = select_one();
)QORE";
#endif

static void staticpgm_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    qph.reset(new QoreProgramHelper(PO_NEW_STYLE, module_xsink));
    (*qph)->setScriptPath("staticpgm module global Program context");
#ifdef STATICPGM_CODE
    // a test can give other code in a file: QORE_STATICPGM_CODE_FILE (see program-cleanup-at-exit.qtest)
    std::string code = staticpgm_code;
    if (const char* file = getenv("QORE_STATICPGM_CODE_FILE")) {
        std::ifstream in(file);
        if (!in) {
            xsink.raiseException("STATICPGM-ERROR", "cannot read QORE_STATICPGM_CODE_FILE '%s'", file);
            return;
        }
        std::ostringstream str;
        str << in.rdbuf();
        code = str.str();
    }
    (*qph)->parse(code.c_str(), "staticpgmcode", &xsink, &xsink);
    if (!xsink) {
        (*qph)->run(&xsink);
    }
    // a foreign thread blocks in the Program, which then still runs code when the library is cleaned up: its code
    // has the functions "staticpgm_block", which blocks forever, and "staticpgm_wait_blocked", which returns once
    // the thread has entered "staticpgm_block" (see program-cleanup-at-exit.qtest)
    if (!xsink && getenv("QORE_STATICPGM_FOREIGN_BLOCK")) {
        QoreProgram* pgm = **qph;
        try {
            std::thread(staticpgm_foreign_block, pgm).detach();
        } catch (const std::system_error& e) {
            xsink.raiseException("STATICPGM-ERROR", "cannot start a foreign thread: %s", e.what());
            return;
        }
        // the Program is never deleted: the thread never leaves it
        abandoned_qph = qph.release();
        pgm->callFunction("staticpgm_wait_blocked", nullptr, &xsink);
    }
#endif
}

static void staticpgm_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

static void staticpgm_module_delete() {
    // the Program is deliberately not released here: the point of this module is that the
    // Program is torn down from a static destructor while exit() runs
}
