/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Parses and runs ordinary code in a foreign thread with a 128KB stack (std::thread with musl's default thread
// stack, an explicit 128KB pthread elsewhere), checks that the code is lowered to IR there in IR, JIT and tiered modes
// and compiled to native code in JIT and tiered modes, and that runaway recursion there raises STACK-LIMIT-EXCEEDED
#define _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/intern/Function.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <pthread.h>
#if defined(__linux__) && !defined(__GLIBC__)
#include <thread>
#define SMALL_STACK_STD_THREAD 1
#endif

static constexpr size_t SMALL_STACK_SIZE = 128 * 1024;

static const char* ordinary_code = "%modern\n"
    "%requires Util\n"
    "class Account {\n"
    "    private { hash<string, float> book; }\n"
    "    add(string k, float v) { book{k} = (book{k} ?? 0.0) + v; }\n"
    "    float total() { return foldl $1 + $2, book.values(); }\n"
    "}\n"
    "hashdecl Row { string name; int qty = 1; }\n"
    "string sub run() {\n"
    "    Account a();\n"
    "    list<hash<Row>> rows = map <Row>{\"name\": \"n\" + $1, \"qty\": $1}, xrange(50);\n"
    "    foreach hash<Row> r in (rows) {\n"
    "        a.add(r.name, r.qty * 1.5);\n"
    "    }\n"
    "    list<string> s = sort(map $1.name, rows, int sub (string x, string y) { return y <=> x; });\n"
    "    code f = string sub (int n) { return sprintf(\"%d:%s\", n, regex(s[0], \"^n(\\\\d+)$\") ? \"m\" : \"x\"); };\n"
    "    return sprintf(\"%.2f %s %s %y %s\", a.total(), f(3), s[0], {\"a\": (1, 2)}, get_byte_size(123456789));\n"
    "}\n"
    "int sub work(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += i * 3 % 7; } return t; }\n"
    "int sub r(int n) { return r(n + 1) + 1; }\n"
    "string sub recurse() { try { r(0); } catch (hash<ExceptionInfo> ex) { return ex.err; } return \"no exception\"; }\n";

static const char* expected_result = "1837.50 3:m n0 {a: [1, 2]} 117.74 MiB";

static size_t current_stack_size() {
#ifdef __APPLE__
    return pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attr;
    assert(!pthread_getattr_np(pthread_self(), &attr));
    size_t size = 0;
    assert(!pthread_attr_getstacksize(&attr, &size));
    assert(!pthread_attr_destroy(&attr));
    return size;
#endif
}

static std::string describe(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    return std::string(err.getType() == NT_STRING ? err.get<const QoreStringNode>()->c_str() : "?") + ": "
        + (desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "?");
}

static std::string call_string(QoreProgram* program, const char* function) {
    ExceptionSink xsink;
    ValueHolder rv(program->callFunction(function, nullptr, &xsink), &xsink);
    if (xsink) {
        std::string rv_desc = describe(xsink);
        xsink.clear();
        return rv_desc;
    }
    return rv->getType() == NT_STRING ? rv->get<const QoreStringNode>()->c_str() : "<not a string>";
}

static unsigned failures = 0;

//! Returns the variant of the given function of the program
static const UserVariantBase* function_variant(QoreProgram* program, const char* name) {
    const QoreFunction* f = reinterpret_cast<const QoreFunction*>(program->findFunction(name));
    assert(f);
    return f->first()->getUserVariantBase();
}

//! Calls work() (the tiered thresholds are set low by the test), and checks the tier that the functions reached
static void check_tiers(QoreProgram* program, qore_exec_mode_t mode) {
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(QoreValue(int64_t(100)), &xsink);
    for (unsigned i = 0; i < 20; ++i) {
        ValueHolder rv(program->callFunction("work", *args, &xsink), &xsink);
        if (xsink || rv->getAsBigInt() != 297) {
            std::fprintf(stderr, "mode %d: work(): %s\n", static_cast<int>(mode),
                xsink ? describe(xsink).c_str() : "wrong result");
            xsink.clear();
            ++failures;
            return;
        }
    }
    if (mode == QEM_AST) {
        return;
    }
    for (const char* name : {"run", "work"}) {
        if (!function_variant(program, name)->getCachedIR()) {
            std::fprintf(stderr, "mode %d: %s() was not lowered to IR\n", static_cast<int>(mode), name);
            ++failures;
        }
    }
    if ((mode == QEM_JIT || mode == QEM_TIERED) && !function_variant(program, "work")->hasCachedFunction()) {
        std::fprintf(stderr, "mode %d: work() was not compiled to native code\n", static_cast<int>(mode));
        ++failures;
    }
}

static void work() {
    QoreForeignThreadHelper foreign;
    // the C library can take up to a page more for its own use, and glibc takes its static TLS from the stack
    size_t size = current_stack_size();
    if (size > SMALL_STACK_SIZE + 16 * 1024) {
        std::fprintf(stderr, "the thread's stack has %zu bytes, not about %zu\n", size, SMALL_STACK_SIZE);
        ++failures;
        return;
    }
    for (qore_exec_mode_t mode : {QEM_AST, QEM_IR, QEM_JIT, QEM_TIERED}) {
        ExceptionSink xsink;
        QoreProgramHelper program(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
        QoreExternalProgramContextHelper context(&xsink, *program);
        assert(!xsink);
        program->setExecMode(mode, true);
        {
            ProgramRuntimeExternalParseContextHelper parse(*program);
            assert(parse);
            program->parse(ordinary_code, "small-thread-stack", &xsink);
        }
        if (xsink) {
            std::fprintf(stderr, "mode %d: parse: %s\n", static_cast<int>(mode), describe(xsink).c_str());
            xsink.clear();
            ++failures;
            continue;
        }
        std::string result = call_string(*program, "run");
        if (result != expected_result) {
            std::fprintf(stderr, "mode %d: run: %s\n", static_cast<int>(mode), result.c_str());
            ++failures;
        }
        result = call_string(*program, "recurse");
        if (result != "STACK-LIMIT-EXCEEDED") {
            std::fprintf(stderr, "mode %d: recurse: %s\n", static_cast<int>(mode), result.c_str());
            ++failures;
        }
        // the thread can run code again after the stack exception
        result = call_string(*program, "run");
        if (result != expected_result) {
            std::fprintf(stderr, "mode %d: run after recursion: %s\n", static_cast<int>(mode), result.c_str());
            ++failures;
        }
        check_tiers(*program, mode);
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
#ifdef SMALL_STACK_STD_THREAD
    // musl's default thread stack is 128KB
    std::thread worker(work);
    worker.join();
#else
    pthread_attr_t attr;
    assert(!pthread_attr_init(&attr));
    assert(!pthread_attr_setstacksize(&attr, SMALL_STACK_SIZE));
    pthread_t thread;
    int rc = pthread_create(&thread, &attr, [](void*) -> void* { work(); return nullptr; }, nullptr);
    assert(!pthread_attr_destroy(&attr));
    assert(!rc);
    assert(!pthread_join(thread, nullptr));
#endif
    qore_cleanup();
    if (failures) {
        return 1;
    }
    std::puts("small-thread-stack: ok");
    return 0;
}
