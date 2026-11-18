/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Checks that a foreign thread created with a small stack has that stack for its own use, and that it can parse and
// run ordinary code there: glibc takes the static TLS block of the process (that of libqore included) from the stack
// of every thread, and libqore's static thread_local buffers once took about 58KB of it.  Runaway recursion there must
// raise STACK-LIMIT-EXCEEDED, and the thread must run code again afterwards.
#include <qore/Qore.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <pthread.h>

//! the stack size of the thread: Qore's stack guard (80KB on x86_64 and aarch64) and parse reserve (64KB), and the
//! stack that the code below needs; with the 58KB of libqore's static TLS taken from it, the code could not be parsed
static constexpr size_t SMALL_STACK_SIZE = 224 * 1024;

//! the most of the thread's stack that may be used before the thread's entry point: the C library's own data and the
//! static TLS of the libraries that the process loads (libqore's must be small)
static constexpr size_t MAX_STACK_OVERHEAD = 24 * 1024;

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
    "int sub r(int n) { return r(n + 1) + 1; }\n"
    "string sub recurse() { try { r(0); } catch (hash<ExceptionInfo> ex) { return ex.err; } return \"no exception\"; }\n";

static const char* expected_result = "1837.50 3:m n0 {a: [1, 2]} 117.74 MiB";

static unsigned failures = 0;

//! gets the lowest address of the calling thread's stack and its size as the C library reports it
/** @return 0 for success, -1 if the stack could not be determined
*/
static int current_stack_bounds(uintptr_t& base, size_t& size) {
#ifdef __APPLE__
    size = pthread_get_stacksize_np(pthread_self());
    // the address of the top of the stack
    base = reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(pthread_self())) - size;
    return 0;
#else
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr)) {
        return -1;
    }
    void* addr = nullptr;
    int rc = pthread_attr_getstack(&attr, &addr, &size);
    pthread_attr_destroy(&attr);
    if (rc) {
        return -1;
    }
    base = reinterpret_cast<uintptr_t>(addr);
    return 0;
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

static void run_code() {
    QoreForeignThreadHelper foreign;
    for (qore_exec_mode_t mode : {QEM_AST, QEM_IR, QEM_JIT, QEM_TIERED}) {
        ExceptionSink xsink;
        QoreProgramHelper program(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
        QoreExternalProgramContextHelper context(&xsink, *program);
        if (xsink) {
            std::fprintf(stderr, "mode %d: program context: %s\n", static_cast<int>(mode), describe(xsink).c_str());
            xsink.clear();
            ++failures;
            continue;
        }
        program->setExecMode(mode, true);
        {
            ProgramRuntimeExternalParseContextHelper parse(*program);
            if (!parse) {
                std::fprintf(stderr, "mode %d: the program cannot be parsed\n", static_cast<int>(mode));
                ++failures;
                continue;
            }
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
    }
}

static void* work(void*) {
    uintptr_t base = 0;
    size_t size = 0;
    // the stack below this frame is what the thread has for its own use
    char marker = 0;
    uintptr_t here = reinterpret_cast<uintptr_t>(&marker);
    if (current_stack_bounds(base, size) || here <= base) {
        std::fprintf(stderr, "the thread's stack cannot be determined\n");
        ++failures;
        return nullptr;
    }
    size_t available = here - base;
    if (available + MAX_STACK_OVERHEAD < SMALL_STACK_SIZE) {
        std::fprintf(stderr, "the thread's stack of %zu bytes (%zu reported) has only %zu bytes below its entry "
            "point: %zu bytes are taken before it (at most %zu expected)\n", SMALL_STACK_SIZE, size, available,
            SMALL_STACK_SIZE - available, MAX_STACK_OVERHEAD);
        ++failures;
    }
    run_code();
    return nullptr;
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) {
        std::fprintf(stderr, "pthread_attr_init() failed\n");
        qore_cleanup();
        return 1;
    }
    pthread_t thread;
    int rc = pthread_attr_setstacksize(&attr, SMALL_STACK_SIZE);
    if (!rc) {
        rc = pthread_create(&thread, &attr, work, nullptr);
    }
    pthread_attr_destroy(&attr);
    if (rc || pthread_join(thread, nullptr)) {
        std::fprintf(stderr, "the thread with a %zu byte stack cannot be run\n", SMALL_STACK_SIZE);
        qore_cleanup();
        return 1;
    }
    qore_cleanup();
    if (failures) {
        return 1;
    }
    std::puts("small-thread-stack: ok");
    return 0;
}
