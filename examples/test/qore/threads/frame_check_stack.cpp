/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Calls the function run() of a program in foreign threads with small stacks (qore#6124, qore#6414): a compiled
// function whose frame is larger than the stack guard must check the stack for its frame before it allocates it, so
// that a thread whose stack cannot take the frame gets STACK-LIMIT-EXCEEDED instead of a crash.
//
// usage: frame-check-stack <file> <ast|ir|jit|tiered> parse|call <stack KB>...
//   parse: parses the file in a thread with each stack size (a module that it loads is loaded there), then calls run()
//          there; only one stack size, as a module is loaded once in a process
//   call:  parses the file and calls run() in the main thread (so that the compiled code is generated there), then
//          calls run() in a thread with each stack size
// prints "<KB>: ok: <result>" or "<KB>: ex: <error>" for each stack size
#include <qore/Qore.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <pthread.h>

namespace {
std::string code;
qore_exec_mode_t mode = QEM_AST;
QoreProgram* program = nullptr;
size_t stack_kb = 0;

std::string describe(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    return err.getType() == NT_STRING ? err.get<const QoreStringNode>()->c_str() : "?";
}

//! calls run() of the program, or returns the error
std::string call_run(QoreProgram* pgm) {
    ExceptionSink xsink;
    ValueHolder rv(pgm->callFunction("run", nullptr, &xsink), &xsink);
    if (xsink) {
        std::string err = "ex: " + describe(xsink);
        xsink.clear();
        return err;
    }
    return std::string("ok: ") + (rv->getType() == NT_STRING ? rv->get<const QoreStringNode>()->c_str() : "?");
}

//! parses the code in a new program, or returns the error
std::string parse(QoreProgramHelper& pgm, ExceptionSink& xsink) {
    pgm->setExecMode(mode, true);
    {
        ProgramRuntimeExternalParseContextHelper parse_context(*pgm);
        if (!parse_context) {
            return "ex: cannot enter the parse context";
        }
        pgm->parse(code.c_str(), "frame-check-stack", &xsink);
    }
    if (xsink) {
        std::string err = "ex: " + describe(xsink);
        xsink.clear();
        return err;
    }
    return std::string();
}

void* parse_and_call(void*) {
    QoreForeignThreadHelper foreign;
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
    QoreExternalProgramContextHelper context(&xsink, *pgm);
    std::string result;
    if (xsink) {
        result = "ex: " + describe(xsink);
        xsink.clear();
    } else {
        result = parse(pgm, xsink);
        if (result.empty()) {
            result = call_run(*pgm);
        }
    }
    std::printf("%zu: %s\n", stack_kb, result.c_str());
    return nullptr;
}

void* call_only(void*) {
    QoreForeignThreadHelper foreign;
    ExceptionSink xsink;
    QoreExternalProgramContextHelper context(&xsink, program);
    std::string result;
    if (xsink) {
        result = "ex: " + describe(xsink);
        xsink.clear();
    } else {
        result = call_run(program);
    }
    std::printf("%zu: %s\n", stack_kb, result.c_str());
    std::fflush(stdout);
    return nullptr;
}

//! runs \a fn in a thread with a stack of stack_kb KB
int run_thread(void* (*fn)(void*)) {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) {
        return -1;
    }
    pthread_t thread;
    int rc = pthread_attr_setstacksize(&attr, stack_kb * 1024);
    if (!rc) {
        rc = pthread_create(&thread, &attr, fn, nullptr);
    }
    pthread_attr_destroy(&attr);
    if (rc || pthread_join(thread, nullptr)) {
        std::fprintf(stderr, "the thread with a %zu KB stack cannot be run\n", stack_kb);
        return -1;
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fputs("usage: frame-check-stack <file> <ast|ir|jit|tiered> parse|call <stack KB>...\n", stderr);
        return 2;
    }
    {
        std::ifstream in(argv[1]);
        std::stringstream buf;
        buf << in.rdbuf();
        code = buf.str();
    }
    if (!std::strcmp(argv[2], "ir")) {
        mode = QEM_IR;
    } else if (!std::strcmp(argv[2], "jit")) {
        mode = QEM_JIT;
    } else if (!std::strcmp(argv[2], "tiered")) {
        mode = QEM_TIERED;
    }
    bool parse_in_thread = !std::strcmp(argv[3], "parse");

    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    int rc = 0;
    if (parse_in_thread) {
        stack_kb = std::strtoul(argv[4], nullptr, 10);
        rc = run_thread(parse_and_call);
    } else {
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
        std::string err = parse(pgm, xsink);
        if (!err.empty()) {
            std::printf("parse: %s\n", err.c_str());
            rc = 1;
        } else {
            // the compiled code is generated here, with the main thread's stack
            std::printf("main: %s\n", call_run(*pgm).c_str());
            program = *pgm;
            for (int i = 4; i < argc && !rc; ++i) {
                stack_kb = std::strtoul(argv[i], nullptr, 10);
                rc = run_thread(call_only);
            }
            program = nullptr;
        }
    }
    qore_cleanup();
    return rc ? 1 : 0;
}
