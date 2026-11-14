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
#include <future>
#include <thread>

static void staticpgm_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
#ifdef STATICPGM_CODE
static void staticpgm_cleanup_callback(QoreProgram* pgm);
#endif
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

# a thread of another runtime that is blocked in Qore code (QORE_STATICPGM_FOREIGN_THREAD, see below)
our Counter foreign_ready(1);
our Queue foreign_queue();
sub foreign_block() {
    foreign_ready.dec();
    foreign_queue.get();
}
sub foreign_release() {
    foreign_ready.dec();
}
sub foreign_probe() {
}
sub foreign_wait_ready() {
    foreign_ready.waitForZero();
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
    // QORE_STATICPGM_FOREIGN_THREAD: a thread that this module does not tell the library about starts in the Program
    // and blocks in Qore code that nothing ends, as a daemon thread of the Python module does when it waits in
    // Queue::get() at the end of the process; the module returns when the thread is in the Qore call (the Program
    // counts the thread), so the state at the end of the process does not depend on timing
    if (!xsink && getenv("QORE_STATICPGM_FOREIGN_ENTRY")) {
        qore_register_program_cleanup_callback(staticpgm_cleanup_callback);
    }
    if (!xsink && getenv("QORE_STATICPGM_FOREIGN_THREAD")) {
        std::thread([]() {
            // the thread never ends: the exceptions it can raise before it blocks are reported with its sink
            ExceptionSink thread_xsink;
            if (q_register_foreign_thread() == QFT_ERROR) {
                // let the module init return, it fails the test with the missing thread
                (*qph)->callFunction("foreign_release", nullptr, &thread_xsink);
                return;
            }
            (*qph)->callFunction("foreign_block", nullptr, &thread_xsink);
            thread_xsink.handleExceptions();
        }).detach();
        (*qph)->callFunction("foreign_wait_ready", nullptr, &xsink);
    }
#endif
}

#ifdef STATICPGM_CODE
//! the cleanup callback of QORE_STATICPGM_FOREIGN_ENTRY: a foreign thread tries to enter the Program that is cleaned up
/** The callback runs after the teardown has decided that no foreign thread runs in the Program, before its data is
    destroyed: the entry must be refused (PROGRAM-ERROR) then, not accepted into a Program whose data is destroyed
    meanwhile; the result is printed for the test (program-cleanup-at-exit.qtest)
*/
static void staticpgm_cleanup_callback(QoreProgram* pgm) {
    if (!qph || pgm != (*qph).operator->()) {
        return;
    }
    std::promise<bool> refused;
    std::future<bool> result = refused.get_future();
    std::thread t([&refused]() {
        ExceptionSink thread_xsink;
        bool r = false;
        if (q_register_foreign_thread() == QFT_OK) {
            (*qph)->callFunction("foreign_probe", nullptr, &thread_xsink);
            r = (bool)thread_xsink;
            thread_xsink.clear();
            q_deregister_foreign_thread();
        }
        refused.set_value(r);
    });
    bool r = result.get();
    t.join();
    printf("staticpgmcode: foreign thread entry %s\n", r ? "refused" : "accepted");
    fflush(stdout);
}

//! staticpgm_foreign_call(code c): runs c in a thread that the library does not control, as a thread of another runtime
//! does (a daemon thread of the Python module, for example); the thread never ends if c blocks
static QoreValue f_staticpgm_foreign_call(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    // the thread owns a reference to the call reference
    std::unique_ptr<QoreValue> code_holder(new QoreValue());
    try {
        *code_holder = args->retrieveEntry(0).refSelf();
    } catch (...) {
        xsink->raiseException("STATICPGM-ERROR", "cannot keep the call reference");
        return QoreValue();
    }
    QoreValue* code = code_holder.get();
    std::thread t;
    try {
        t = std::thread([code]() {
            ExceptionSink thread_xsink;
            if (q_register_foreign_thread() != QFT_ERROR) {
                code->get<ResolvedCallReferenceNode>()->invoke(nullptr, &thread_xsink).discard(&thread_xsink);
            }
            code->discard(&thread_xsink);
            delete code;
            thread_xsink.handleExceptions();
        });
    } catch (const std::exception& e) {
        code_holder->discard(xsink);
        xsink->raiseException("STATICPGM-ERROR", "cannot start the thread: %s", e.what());
        return QoreValue();
    }
    // the thread owns the reference now
    code_holder.release();
    t.detach();
    return QoreValue();
}

//! the object that staticpgm_keep_object() keeps for the life of the process
static QoreObject* kept_object = nullptr;

//! staticpgm_keep_object(object o): keeps a reference to the object, so that it outlives qore_cleanup() as the object of
//! a host that releases it only after the library has been cleaned up does (a Program object that Python holds)
static QoreValue f_staticpgm_keep_object(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(o, QoreObject, args, 0);
    if (!kept_object) {
        kept_object = static_cast<QoreObject*>(o->refSelf());
    }
    return QoreValue();
}
#endif

static void staticpgm_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
#ifdef STATICPGM_CODE
    rns->addBuiltinVariant("staticpgm_foreign_call", f_staticpgm_foreign_call, QCF_NO_FLAGS, QDOM_DEFAULT,
        nothingTypeInfo, 1, codeTypeInfo, QORE_PARAM_NO_ARG, "code");
    rns->addBuiltinVariant("staticpgm_keep_object", f_staticpgm_keep_object, QCF_NO_FLAGS, QDOM_DEFAULT,
        nothingTypeInfo, 1, objectTypeInfo, QORE_PARAM_NO_ARG, "obj");
#endif
}

static void staticpgm_module_delete() {
    // the Program is deliberately not released here: the point of this module is that the
    // Program is torn down from a static destructor while exit() runs
}
