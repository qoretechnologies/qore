/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>
#include <qore/intern/QoreThreadList.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <array>
#include <atomic>
#include <thread>
#ifdef __linux__
#include <sys/resource.h>
#endif
#ifndef _Q_WINDOWS
#include <unistd.h>
#endif

// A native TLS destructor runs after q_run_thread has released its Qore TID.
// Keep it alive with a barrier so counter semantics can be checked without a race.
struct NativeCleanup {
    pthread_key_t key;
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool released = false;
    unsigned destructors = 0;

    NativeCleanup() {
        assert(!pthread_key_create(&key, cleanup));
    }

    ~NativeCleanup() {
        assert(!pthread_key_delete(key));
    }

    static void cleanup(void* arg) {
        auto& state = *static_cast<NativeCleanup*>(arg);
        std::unique_lock<std::mutex> lock(state.mutex);
        state.entered = true;
        state.condition.notify_all();
        assert(state.condition.wait_for(lock, std::chrono::seconds(30), [&state]() {
            return state.released;
        }));
        // POSIX must finish repeated destructor passes too, not just the first callback.
        if (++state.destructors < 3) {
            assert(!pthread_setspecific(state.key, &state));
        }
    }

    static void worker(ExceptionSink*, void* arg) {
        auto& state = *static_cast<NativeCleanup*>(arg);
        assert(!pthread_setspecific(state.key, &state));
    }

    static void blockedWorker(ExceptionSink*, void* arg) {
        auto& state = *static_cast<NativeCleanup*>(arg);
        std::unique_lock<std::mutex> lock(state.mutex);
        state.entered = true;
        state.condition.notify_all();
        assert(state.condition.wait_for(lock, std::chrono::seconds(30), [&state]() {
            return state.released;
        }));
    }

    void waitForDestructor() {
        std::unique_lock<std::mutex> lock(mutex);
        assert(condition.wait_for(lock, std::chrono::seconds(30), [this]() {
            return entered;
        }));
    }

    void release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }
};

static void checkNativeCleanup(bool custom_stack) {
    NativeCleanup state;
    ExceptionSink xsink;
    int tid = custom_stack
        ? q_start_thread(&xsink, NativeCleanup::worker, &state, size_t(1024 * 1024), QTF_EXTERNAL_LIFECYCLE)
        : q_start_thread(&xsink, NativeCleanup::worker, &state, QTF_EXTERNAL_LIFECYCLE);
    assert(tid > 0 && !xsink);
    state.waitForDestructor();
    assert(tp_thread_counter.getCount() == 1);
    assert(thread_counter.getCount() == 0);
    state.release();
    assert(!tp_thread_counter.waitForZero(&xsink, 30000) && !xsink);
    assert(state.destructors == 3);
}

static void checkConcurrentCreation() {
    // All creators reach a native barrier before racing to start the first
    // external thread. This also exercises simultaneous reaper initialization.
    std::array<NativeCleanup, 16> states;
    std::array<std::thread, 16> creators;
    std::mutex mutex;
    std::condition_variable condition;
    unsigned ready = 0;
    for (unsigned i = 0; i < creators.size(); ++i) {
        creators[i] = std::thread([&, i]() {
            QoreForeignThreadHelper registration;
            assert(registration);
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++ready;
                condition.notify_all();
                assert(condition.wait_for(lock, std::chrono::seconds(30), [&]() {
                    return ready == creators.size();
                }));
            }
            ExceptionSink xsink;
            assert(q_start_thread(&xsink, NativeCleanup::worker, &states[i], QTF_EXTERNAL_LIFECYCLE) > 0);
            assert(!xsink);
        });
    }
    for (auto& creator : creators) {
        creator.join();
    }
    for (auto& state : states) {
        state.waitForDestructor();
    }
    assert(tp_thread_counter.getCount() == static_cast<int>(states.size()));
    for (auto& state : states) {
        state.release();
    }
    ExceptionSink xsink;
    assert(!tp_thread_counter.waitForZero(&xsink, 30000) && !xsink);
    for (auto& state : states) {
        assert(state.destructors == 3);
    }
}

static void checkFailedCreation() {
    ExceptionSink xsink;
    // An invalid stack must neither run the callback nor strand a counter entry.
    assert(q_start_thread(&xsink, NativeCleanup::worker, nullptr, size_t(1), QTF_EXTERNAL_LIFECYCLE) == -1);
    assert(xsink);
    QoreStringValueHelper error(xsink.getExceptionErr());
    assert(!strcmp(error->c_str(), "THREAD-CREATION-FAILURE"));
    xsink.clear();
    assert(tp_thread_counter.getCount() == 0);
    checkNativeCleanup(false);
}

#ifdef __linux__
class NativeCreationLimit {
public:
    NativeCreationLimit() {
        assert(!getrlimit(RLIMIT_NPROC, &original));
        struct rlimit limited = original;
        limited.rlim_cur = 1;
        assert(!setrlimit(RLIMIT_NPROC, &limited));
    }

    ~NativeCreationLimit() {
        assert(!setrlimit(RLIMIT_NPROC, &original));
    }

private:
    struct rlimit original;
};

static void checkNativeCreationFailure() {
    // RLIMIT_NPROC is per-process configuration; reducing this process's soft
    // limit leaves other developers' processes and resource limits unchanged.
    // Run this explicit mode as an unprivileged user on Linux.
    assert(geteuid() != 0);
    ExceptionSink xsink;
    int tid;
    {
        NativeCreationLimit limit;
        tid = q_start_thread(&xsink, NativeCleanup::worker, nullptr, QTF_EXTERNAL_LIFECYCLE);
    }
    assert(tid == -1 && xsink);
    QoreStringValueHelper error(xsink.getExceptionErr());
    assert(!strcmp(error->c_str(), "THREAD-CREATION-FAILURE"));
    xsink.clear();
    assert(tp_thread_counter.getCount() == 0);
    checkNativeCleanup(false);
}
#endif

static std::atomic<bool> signal_tls_finished{false};
static pthread_key_t signal_tls_key;
static NativeCleanup* native_signal_state = nullptr;

static void checkManagedSignalExit(const char* mode) {
    // The cleanup hook installs native TLS on the signal thread. The atexit check must observe
    // its destructor, proving that explicit exit joined the pthread, not merely stopped its loop.
    assert(!pthread_key_create(&signal_tls_key, [](void*) {
        signal_tls_finished.store(true);
    }));
    tclist.push([](void*) {
        assert(!pthread_setspecific(signal_tls_key, &signal_tls_key));
    }, nullptr);
    assert(!atexit([]() {
        assert(signal_tls_finished.load());
        fputs("signal joined\n", stdout);
    }));
    fputs("buffered output\n", stdout);

    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN | PO_ENABLE_DEBUG, xsink);
#ifndef _Q_WINDOWS
    if (native_signal_state) {
        assert(!kill(getpid(), SIGINT));
        native_signal_state->waitForDestructor();
    }
#endif
    const char* source = !strcmp(mode, "--exit-managed-blocked")
        ? "%modern\nQueue ready(); Queue release();"
          "set_signal_handler(SIGUSR1, sub(int sig) { ready.push(True); release.get(); });"
          "kill(getpid(), SIGUSR1); ready.get(); exit(17);"
        : !strcmp(mode, "--exit-managed-replaced")
        ? "%modern\nQueue ready(); Queue release();"
          "set_signal_handler(SIGUSR1, sub(int sig) { ready.push(True); release.get(); });"
          "kill(getpid(), SIGUSR1); ready.get();"
          "set_signal_handler(SIGUSR1, sub(int sig) {}); exit(17);"
        : !strcmp(mode, "--exit-managed-handler")
        ? "%modern\nset_signal_handler(SIGUSR1, sub(int sig) {}); exit(17);"
        : "%modern\nexit(17);";
    pgm->parse(source, "managed-signal-exit", &xsink);
    if (!xsink) {
        pgm->run(&xsink).discard(&xsink);
    }
    xsink.handleExceptions();
    abort();
}

static void checkExplicitExit(const char* mode) {
    // Both markers stay in stdio's buffer until exit() flushes it. _Exit() must
    // skip the atexit callback and flushing while native workers are still active.
    assert(!atexit([]() { fputs("atexit\n", stdout); }));
    fputs("buffered output\n", stdout);

    NativeCleanup state;
    ExceptionSink xsink;
    bool signal = !strncmp(mode, "--exit-signal", 13);
    bool tls = strstr(mode, "tls");
    bool active = !strcmp(mode, "--exit-active");
    if (tls || active) {
        auto worker = active ? NativeCleanup::blockedWorker : NativeCleanup::worker;
        int tid = strstr(mode, "custom")
            ? q_start_thread(&xsink, worker, &state, size_t(1024 * 1024), QTF_EXTERNAL_LIFECYCLE)
            : q_start_thread(&xsink, worker, &state, QTF_EXTERNAL_LIFECYCLE);
        assert(tid > 0 && !xsink);
        state.waitForDestructor();
        assert(tp_thread_counter.getCount() == 1);
        // The barrier stays closed: explicit exit must not wait for this worker.
    } else if (strcmp(mode, "--exit-empty")) {
        checkNativeCleanup(false);
        checkNativeCleanup(true);
        assert(tp_thread_counter.getCount() == 0);
    }

    QoreProgramHelper pgm(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES, xsink);
    const char* source = signal
        ? "set_signal_handler(SIGUSR1, sub(int sig) { if (gettid() != 0) { abort(); } exit(17); });"
          "kill(getpid(), SIGUSR1); while (True) { usleep(1000); }"
        : active ? "if (num_threads() != 2) { abort(); } exit(17);"
        : "if (num_threads() != 1) { abort(); } exit(17);";
    pgm->parse(source, "explicit-exit", &xsink);
    if (xsink) {
        xsink.handleExceptions();
        abort();
    }
    pgm->run(&xsink).discard(&xsink);
    xsink.handleExceptions();
    abort(); // exit() must never return or throw
}

int main(int argc, char** argv) {
    // Force buffering even when the native helper is run directly from a terminal.
    static char output_buffer[BUFSIZ];
    assert(!setvbuf(stdout, output_buffer, _IOFBF, sizeof(output_buffer)));
    bool managed_signal = argc > 1 && !strncmp(argv[1], "--exit-managed-", 15);
    bool signal = managed_signal || (argc > 1 && !strncmp(argv[1], "--exit-signal", 13));
    if (managed_signal && !strcmp(argv[1], "--exit-managed-native-blocked")) {
        static NativeCleanup state;
        native_signal_state = &state;
        assert(std::signal(SIGINT, [](int) {
            NativeCleanup::blockedWorker(nullptr, native_signal_state);
        }) != SIG_ERR);
    }
    qore_init(QL_MIT, "UTF-8", true, signal ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);
    if (managed_signal) {
        checkManagedSignalExit(argv[1]);
    }
    if (argc > 1 && !strcmp(argv[1], "--return-with-idle-reaper")) {
        checkNativeCleanup(false);
        assert(tp_thread_counter.getCount() == 0);
        puts("Returning without qore_cleanup");
        return 0;
    }
    if (argc > 1 && !strncmp(argv[1], "--exit-", 7)) {
        checkExplicitExit(argv[1]);
    }
#ifdef __linux__
    if (argc > 1 && !strcmp(argv[1], "--creation-failure")) {
        // First fail reaper startup, then fail a worker after the reaper exists.
        checkNativeCreationFailure();
        checkNativeCreationFailure();
    }
#endif
    if (argc == 1 || strcmp(argv[1], "--empty")) {
        checkConcurrentCreation();
        checkConcurrentCreation();
        checkNativeCleanup(false);
        checkNativeCleanup(true);
        checkFailedCreation();
    }
    qore_cleanup();
    puts("Passed native thread cleanup checks");
    return 0;
}
