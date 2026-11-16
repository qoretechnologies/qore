/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Parse just above and below the stack-check boundary. An operand rejected there must not be used as though it
// were initialized (#6047): a local declaration can still have a null LocalVar.
#include <qore/Qore.h>

#include <alloca.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <string>

static constexpr size_t STACK_SIZE = 384 * 1024;
static constexpr size_t STEP = 8 * 1024;
static constexpr size_t ALIGNMENT = 16;
static constexpr const char* STACK_ERROR =
    "the code is nested too deeply to be parsed within the stack of this thread";

enum class Outcome { Accepted, Rejected, Failed };

struct Case {
    const char* name;
    std::string code;
    unsigned accepted = 0;
    unsigned rejected = 0;
    bool failed = false;
};

static Outcome parse_code(const std::string& code) {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
    // This fixture isolates parse initialization; IR/JIT lowering has separate stack checks.
    program->setExecMode(QEM_AST, true);
    QoreExternalProgramContextHelper context(&xsink, *program);
    if (!xsink) {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        if (!parse) {
            std::fputs("cannot enter parse context\n", stderr);
            return Outcome::Failed;
        }
        program->parse(code.c_str(), "parse-stack-boundary", &xsink);
    }
    if (!xsink) {
        return Outcome::Accepted;
    }
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    bool expected = err.getType() == NT_STRING && desc.getType() == NT_STRING
        && !std::strcmp(err.get<const QoreStringNode>()->c_str(), "PARSE-EXCEPTION")
        && std::strstr(desc.get<const QoreStringNode>()->c_str(), STACK_ERROR);
    if (!expected) {
        std::fprintf(stderr, "unexpected parse error: %s\n",
            desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "<no description>");
    }
    xsink.clear();
    return expected ? Outcome::Rejected : Outcome::Failed;
}

// Keep this frame and its live padding across parse_code(). Varying the available stack in small steps reaches
// a narrow failure window even when pthread stack sizes must be page-aligned, or TLS/compiler frames differ.
// The library's stack guard and parse reserve are not changed.
__attribute__((noinline)) static Outcome parse_with_padding(const std::string& code, size_t padding) {
    volatile char* space = static_cast<volatile char*>(alloca(padding + 1));
    space[0] = 0;
    Outcome result = parse_code(code);
    space[padding] = space[0];
    return result;
}

static Outcome probe(Case& test, size_t padding) {
    Outcome result = parse_with_padding(test.code, padding);
    if (result == Outcome::Accepted) {
        ++test.accepted;
    } else if (result == Outcome::Rejected) {
        ++test.rejected;
    } else {
        test.failed = true;
    }
    return result;
}

static void* work(void* arg) {
    Case& test = *static_cast<Case*>(arg);
    QoreForeignThreadHelper foreign;
    if (probe(test, 0) != Outcome::Accepted) {
        std::fprintf(stderr, "%s: initial parse did not fit\n", test.name);
        test.failed = true;
        return nullptr;
    }
    size_t good = 0;
    size_t bad = STEP;
    // Find the first rejected parse without jumping into a nearly exhausted native stack. The parse reserve
    // leaves room for cleanup even when an operand could not be initialized.
    for (; bad <= STACK_SIZE - 64 * 1024; bad += STEP) {
        Outcome result = probe(test, bad);
        if (result == Outcome::Failed) {
            return nullptr;
        }
        if (result == Outcome::Rejected) {
            break;
        }
        good = bad;
    }
    if (!test.rejected) {
        std::fprintf(stderr, "%s: did not reach the parse stack check\n", test.name);
        test.failed = true;
        return nullptr;
    }
    // Locate both sides of the transition to the alignment of the native frame. Before the fix the window
    // between successful initialization and early rejection dereferenced an uninitialized local declaration.
    while (bad - good > ALIGNMENT) {
        size_t middle = ((good + bad) / 2) & ~(ALIGNMENT - 1);
        Outcome result = probe(test, middle);
        if (result == Outcome::Failed) {
            return nullptr;
        }
        if (result == Outcome::Rejected) {
            bad = middle;
        } else {
            good = middle;
        }
    }
    // A failed parse must leave the thread usable when its caller has unwound the padding.
    if (probe(test, 0) != Outcome::Accepted) {
        std::fprintf(stderr, "%s: parse after stack rejection failed\n", test.name);
        test.failed = true;
    }
    return nullptr;
}

static std::string nested(const char* prefix, const char* suffix, unsigned count) {
    std::string result = "1";
    for (unsigned i = 0; i < count; ++i) {
        result = prefix + result + suffix;
    }
    return "%modern\nsub run() { auto value = " + result + "; }";
}

int main(int argc, char** argv) {
    Case cases[] = {
        {"list-assignment", "%modern\nsub run() { (int a, int b) = (1, 2); }"},
        {"reference-declaration", "%modern\nsub run() { auto ref = \\(int a); }"},
        {"reference-local", "%modern\nsub run() { int a; auto ref = \\a; }"},
        {"lvalue-path", "%modern\nsub run() { hash<auto> h; h{\"a\"}{\"b\"}{\"c\"} = 1; }"},
        {"list-assignment-path", "%modern\nsub run() { hash<auto> h; (h{\"a\"}{\"b\"}, int b) = (1, 2); }"},
        {"nested-list", nested("(", ",)", 32)},
        {"nested-hash", nested("{\"k\":", "}", 32)},
        {"nested-closure", nested("auto sub () { return ", "; }", 16)},
    };
    if (argc > 2) {
        return 2;
    }
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    unsigned failures = 0;
    unsigned ran = 0;
    for (Case& test : cases) {
        if (argc == 2 && std::strcmp(test.name, argv[1])) {
            continue;
        }
        ++ran;
        pthread_attr_t attr;
        int rc = pthread_attr_init(&attr);
        if (rc) {
            ++failures;
            break;
        }
        pthread_t thread;
        rc = pthread_attr_setstacksize(&attr, STACK_SIZE);
        if (!rc) {
            rc = pthread_create(&thread, &attr, work, &test);
        }
        pthread_attr_destroy(&attr);
        if (rc || pthread_join(thread, nullptr)) {
            std::fprintf(stderr, "%s: cannot run the foreign thread\n", test.name);
            ++failures;
            break;
        }
        if (test.failed || !test.accepted || !test.rejected) {
            ++failures;
        }
        std::printf("%s: %u accepted, %u rejected\n", test.name, test.accepted, test.rejected);
    }
    qore_cleanup();
    if (failures || !ran) {
        return 1;
    }
    std::puts("parse-stack-boundary: ok");
    return 0;
}
