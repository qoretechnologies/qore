/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Parses nested code in foreign threads with a range of stack sizes: parse initialization stops where the stack
// runs short, and each parse must then fail with a parse exception, never crash (issue #6047).  In each kind of code,
// every level of nesting is an expression that inspects the expression it contains after parse-initializing it (a
// list assignment checks its lvalues, a cast to a hashdecl checks the keys of its hash), so wherever the stack runs
// short, the expression containing the one that was not parse-initialized inspects it.  With a module name and a
// stack size, it parses code that requires the module, whose source requires others: each module is parsed while
// the parse that requires it waits, so the parses nest in the thread's stack.
#include <qore/Qore.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <climits>
#include <pthread.h>
#include <unistd.h>

//! the smallest and the largest stack size tried, and the step between them
static constexpr size_t MIN_STACK_SIZE = 160 * 1024;
static constexpr size_t MAX_STACK_SIZE = 640 * 1024;
static constexpr size_t STACK_SIZE_STEP = 4 * 1024;

//! the code and the description of the parse error raised when parsing runs short of stack
static const char* stack_parse_err = "PARSE-STACK-LIMIT-EXCEEDED";
static const char* stack_parse_error = "the code is nested too deeply to be parsed within the stack of this thread";

static unsigned failures = 0;

//! repeats a string n times
static std::string repeat(const char* str, int n) {
    std::string rv;
    for (int i = 0; i < n; ++i) {
        rv += str;
    }
    return rv;
}

//! returns the start of n nested list assignments, each declaring variables of its own
static std::string list_assignments(int n) {
    std::string rv;
    for (int i = 0; i < n; ++i) {
        rv += "(auto a" + std::to_string(i) + ", auto b" + std::to_string(i) + ") = (";
    }
    return rv;
}

//! a kind of nested code
struct Kind {
    const char* name;
    std::string source;
    //! the number of sizes where parsing ran short of stack
    unsigned stack_errors = 0;
    //! the number of sizes where the code was parsed and run
    unsigned parsed = 0;
};

static Kind kinds[] = {
    // a list assignment that declares its lvalues, whose right-hand side is a list assignment: its lvalues are
    // checked after they are parse-initialized
    {"list assignment", "auto sub run() { " + list_assignments(200) + "1" + repeat(", 2)", 200)
        + "; return 1; }\n"},
    // a cast to a hashdecl of a hash whose value is a cast: the keys of the hash are checked after it is
    // parse-initialized; the hash is at every odd level of nesting here and at every even level below
    {"hashdecl cast", "hashdecl H { auto a; }\nauto sub run() { auto v = "
        + repeat("cast<hash<H>>({\"a\": ", 150) + "1" + repeat("})", 150) + "; return 1; }\n"},
    {"hash of a hashdecl cast", "hashdecl H { auto a; }\nauto sub run() { auto v = {\"a\": "
        + repeat("cast<hash<H>>({\"a\": ", 150) + "1" + repeat("})", 150) + "}; return 1; }\n"},
};

static std::string describe(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    return std::string(err.getType() == NT_STRING ? err.get<const QoreStringNode>()->c_str() : "?") + ": "
        + (desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "?");
}

//! returns true if the first exception is the parse stack error
static bool is_stack_error(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    return err.getType() == NT_STRING && desc.getType() == NT_STRING
        && !strcmp(err.get<const QoreStringNode>()->c_str(), stack_parse_err)
        // the load of each module that loads the one that could not be parsed adds its name to the description
        && !strncmp(desc.get<const QoreStringNode>()->c_str(), stack_parse_error, strlen(stack_parse_error));
}

//! the stack size of the thread running work()
static size_t current_size = 0;

static void* work(void*) {
    QoreForeignThreadHelper foreign;
    for (Kind& kind : kinds) {
        ExceptionSink xsink;
        QoreProgramHelper program(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
        QoreExternalProgramContextHelper context(&xsink, *program);
        if (xsink) {
            std::fprintf(stderr, "%zuK %s: program context: %s\n", current_size / 1024, kind.name,
                describe(xsink).c_str());
            xsink.clear();
            ++failures;
            continue;
        }
        {
            ProgramRuntimeExternalParseContextHelper parse(*program);
            if (!parse) {
                std::fprintf(stderr, "%zuK %s: the program cannot be parsed\n", current_size / 1024, kind.name);
                ++failures;
                continue;
            }
            program->parse(kind.source.c_str(), kind.name, &xsink);
        }
        if (xsink) {
            std::string desc = describe(xsink);
            xsink.clear();
            if (desc == std::string(stack_parse_err) + ": " + stack_parse_error) {
                ++kind.stack_errors;
            } else {
                std::fprintf(stderr, "%zuK %s: unexpected parse error: %s\n", current_size / 1024, kind.name,
                    desc.c_str());
                ++failures;
            }
            continue;
        }
        ValueHolder rv(program->callFunction("run", nullptr, &xsink), &xsink);
        if (xsink) {
            // code that was parsed can still run short of stack
            std::string desc = describe(xsink);
            xsink.clear();
            if (desc.compare(0, strlen("STACK-LIMIT-EXCEEDED"), "STACK-LIMIT-EXCEEDED")) {
                std::fprintf(stderr, "%zuK %s: run: %s\n", current_size / 1024, kind.name, desc.c_str());
                ++failures;
            }
            continue;
        }
        if (rv->getAsBigInt() != 1) {
            std::fprintf(stderr, "%zuK %s: run() returned a wrong value\n", current_size / 1024, kind.name);
            ++failures;
            continue;
        }
        ++kind.parsed;
    }
    return nullptr;
}

//! The start routine of a thread that runs F: an exception that F lets escape (std::bad_alloc from a std::string, for
//! example) would call std::terminate() at the end of the start routine, so it is reported as a failure instead
template <void* (*F)(void*)>
static void* guarded(void* arg) noexcept {
    try {
        return F(arg);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%zuK: exception: %s\n", current_size / 1024, e.what());
    } catch (...) {
        std::fprintf(stderr, "%zuK: unknown exception\n", current_size / 1024);
    }
    ++failures;
    return nullptr;
}

//! runs work() in a thread with the given stack size; returns 0 if the thread ran
static int run_in_thread(size_t size, void* (*work)(void*)) {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) {
        std::fprintf(stderr, "pthread_attr_init() failed\n");
        return -1;
    }
    pthread_t thread;
    int rc = pthread_attr_setstacksize(&attr, size);
    if (!rc) {
        rc = pthread_create(&thread, &attr, work, nullptr);
    }
    pthread_attr_destroy(&attr);
    if (rc || pthread_join(thread, nullptr)) {
        std::fprintf(stderr, "a thread with a %zu byte stack cannot be run\n", size);
        return -1;
    }
    return 0;
}

//! the module that load_module() requires
static const char* module_name = nullptr;

//! Parses code that requires module_name in a foreign thread: the module and the modules that it requires are parsed
//! from their sources in the thread's stack, each while the parse that requires it waits
static void* load_module(void*) {
    QoreForeignThreadHelper foreign;
    ExceptionSink xsink;
    QoreProgramHelper program(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    if (xsink) {
        std::fprintf(stderr, "%zuK modules: program context: %s\n", current_size / 1024, describe(xsink).c_str());
        xsink.clear();
        ++failures;
        return nullptr;
    }
    std::string code = std::string("%requires ") + module_name + "\n";
    {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        if (!parse) {
            std::fprintf(stderr, "%zuK modules: the program cannot be parsed\n", current_size / 1024);
            ++failures;
            return nullptr;
        }
        program->parse(code.c_str(), "modules", &xsink);
    }
    if (!xsink) {
        std::puts("parsed");
        return nullptr;
    }
    // the parse error of the module that cannot be parsed is the parse error of the code that requires it
    bool stack_error = is_stack_error(xsink);
    std::string desc = describe(xsink);
    xsink.clear();
    if (stack_error) {
        std::puts("stack error");
    } else {
        std::fprintf(stderr, "%zuK modules: unexpected parse error: %s\n", current_size / 1024, desc.c_str());
        ++failures;
    }
    return nullptr;
}

//! the file whose source load_source() parses
static const char* source_path = nullptr;

//! reads a file; returns false if it cannot be read
static bool read_file(const char* path, std::string& rv) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        rv.append(buf, n);
    }
    bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

//! Parses the source of source_path in a foreign thread and calls its run() function, which returns an integer:
//! prints "parsed <value>", "stack error" if the parse or the run (a module that it loads at run time) failed with
//! the parse stack error as the first exception, or the parse with STACK-LIMIT-EXCEEDED (raised by the code that ran
//! while a module was loaded), or "run stack error" if the run raised STACK-LIMIT-EXCEEDED
static void* load_source(void*) {
    QoreForeignThreadHelper foreign;
    std::string code;
    if (!read_file(source_path, code)) {
        std::fprintf(stderr, "%s: cannot be read\n", source_path);
        ++failures;
        return nullptr;
    }
    ExceptionSink xsink;
    QoreProgramHelper program(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    if (xsink) {
        std::fprintf(stderr, "%zuK source: program context: %s\n", current_size / 1024, describe(xsink).c_str());
        xsink.clear();
        ++failures;
        return nullptr;
    }
    {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        if (!parse) {
            std::fprintf(stderr, "%zuK source: the program cannot be parsed\n", current_size / 1024);
            ++failures;
            return nullptr;
        }
        program->parse(code.c_str(), source_path, &xsink);
    }
    bool parsed = !xsink;
    if (parsed) {
        ValueHolder rv(program->callFunction("run", nullptr, &xsink), &xsink);
        if (!xsink) {
            std::printf("parsed %lld\n", (long long)rv->getAsBigInt());
            return nullptr;
        }
    }
    std::string desc = describe(xsink);
    bool stack_limit = !desc.compare(0, strlen("STACK-LIMIT-EXCEEDED:"), "STACK-LIMIT-EXCEEDED:");
    if (is_stack_error(xsink) || (!parsed && stack_limit)) {
        // the parse ran short of stack, or the code that ran while a module was loaded (its constant initializers)
        std::puts("stack error");
    } else if (stack_limit) {
        // code that was parsed can still run short of stack
        std::puts("run stack error");
    } else {
        std::fprintf(stderr, "%zuK source: unexpected error: %s\n", current_size / 1024, desc.c_str());
        ++failures;
    }
    xsink.clear();
    return nullptr;
}

//! Returns the smallest stack size that a thread can be created with on this platform, 0 if there is no minimum:
//! glibc refuses a smaller one (EINVAL from pthread_attr_setstacksize()), and its minimum depends on the platform
//! (131072 bytes on aarch64, 16384 on x86_64), and on glibc 2.34 and later also on the system (_SC_THREAD_STACK_MIN)
static size_t min_stack_size() {
    size_t rv = 0;
#ifdef PTHREAD_STACK_MIN
    rv = static_cast<size_t>(PTHREAD_STACK_MIN);
#endif
#ifdef _SC_THREAD_STACK_MIN
    long sys_min = sysconf(_SC_THREAD_STACK_MIN);
    if (sys_min > 0 && static_cast<size_t>(sys_min) > rv) {
        rv = static_cast<size_t>(sys_min);
    }
#endif
    return rv;
}

//! Tears the library down and returns the exit status, or ends the process with it when it must not run exit()
/** A background JIT compile still in LLVM when qore_cleanup() stops the compiler is abandoned (the module code that
    the fixture loads is %modern, so its functions are compiled in the background), and exit() would destroy the
    static objects of LLVM under it: an embedding program that returns from main() calls qore_fork_child_quick_exit()
    after qore_cleanup(), which then flushes the standard I/O streams and ends the process with _exit()
*/
//! true with --plain-exit: the fixture returns from main() after qore_cleanup(), without qore_fork_child_quick_exit()
/** For the test of the debug library's check of a process that ends so while a test hook holds an abandoned compile
    (QoreJIT::dbgExitIfAbandonedCompileHeld(), issue #6296); never used otherwise
*/
static bool plain_exit = false;

static int end_process(int rc) {
    qore_cleanup();
    if (!plain_exit) {
        qore_fork_child_quick_exit(rc);
    }
    return rc;
}

//! parse_stack_boundary: parses the nested code with each stack size
//! parse_stack_boundary <module> <stack KB>: parses code that requires the module with the given stack size, and
//! prints "parsed" or "stack error" (a process can load a module once, so each size runs in a process of its own)
//! parse_stack_boundary --source <file> <stack KB>...: parses the file in a new Program with each stack size in turn,
//! in a thread of its own, and runs its run() function (see load_source())
//! parse_stack_boundary --plain-exit ...: as without it, but ends with exit() after qore_cleanup() (see plain_exit)
//! parse_stack_boundary --min-stack: prints the smallest stack size in bytes that a thread can be created with (see
//! min_stack_size())
int main(int argc, char** argv) {
    if (argc == 2 && !strcmp(argv[1], "--min-stack")) {
        std::printf("%zu\n", min_stack_size());
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--plain-exit")) {
        plain_exit = true;
        --argc;
        ++argv;
    }
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    if (argc > 3 && !strcmp(argv[1], "--source")) {
        source_path = argv[2];
        // each size in a thread of its own, one after the other in this process, which loads each module once
        for (int i = 3; i < argc; ++i) {
            current_size = strtoul(argv[i], nullptr, 10) * 1024;
            if (run_in_thread(current_size, guarded<load_source>)) {
                ++failures;
            }
        }
        return end_process(failures ? 1 : 0);
    }
    if (argc > 2) {
        module_name = argv[1];
        current_size = strtoul(argv[2], nullptr, 10) * 1024;
        if (run_in_thread(current_size, guarded<load_module>)) {
            ++failures;
        }
        return end_process(failures ? 1 : 0);
    }
    for (current_size = MIN_STACK_SIZE; current_size <= MAX_STACK_SIZE; current_size += STACK_SIZE_STEP) {
        if (run_in_thread(current_size, guarded<work>)) {
            ++failures;
            break;
        }
    }
    // each kind of code ran short of stack at some size, and was parsed and run with a larger one
    for (const Kind& kind : kinds) {
        if (!kind.stack_errors || !kind.parsed) {
            std::fprintf(stderr, "%s: %u stack errors, parsed %u times: the sizes do not cover the boundary\n",
                kind.name, kind.stack_errors, kind.parsed);
            ++failures;
        }
    }
    if (!failures) {
        std::puts("parse-stack-boundary: ok");
    }
    return end_process(failures ? 1 : 0);
}
