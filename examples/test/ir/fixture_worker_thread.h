/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
//! Runs the work of a native test fixture in a foreign thread with a stack as large as a Qore thread's
/** A fixture whose foreign thread parses Qore code cannot use the C library's default thread stack (for example with
    std::thread): parsing needs QORE_STACK_GUARD (80KB on aarch64 and x86_64) plus QORE_PARSE_STACK_RESERVE (64KB)
    above the bottom of the stack and raises "the code is nested too deeply to be parsed within the stack of this
    thread" with less, even for code that is not nested at all, and musl's default thread stack is only 128KB.
*/
#ifndef QORE_TEST_IR_FIXTURE_WORKER_THREAD_H
#define QORE_TEST_IR_FIXTURE_WORKER_THREAD_H

#include <pthread.h>

#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>

//! The stack size of the worker thread: the default stack size of a thread that Qore starts
constexpr size_t FIXTURE_WORKER_STACK_SIZE = 8 * 1024 * 1024;

//! Runs a function in a new native thread with a stack of FIXTURE_WORKER_STACK_SIZE bytes and waits for it
/** An exception that the function throws is rethrown in the calling thread
*/
static void run_in_fixture_worker(const std::function<void()>& fn) {
    struct Work {
        const std::function<void()>& fn;
        std::exception_ptr error;

        static void* run(void* arg) {
            Work& work = *static_cast<Work*>(arg);
            try {
                work.fn();
            } catch (...) {
                work.error = std::current_exception();
            }
            return nullptr;
        }
    } work{fn, nullptr};

    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    assert(!rc);
    rc = pthread_attr_setstacksize(&attr, FIXTURE_WORKER_STACK_SIZE);
    pthread_t thread;
    if (!rc) {
        rc = pthread_create(&thread, &attr, Work::run, &work);
    }
    pthread_attr_destroy(&attr);
    assert(!rc);
    rc = pthread_join(thread, nullptr);
    assert(!rc);
    if (work.error) {
        std::rethrow_exception(work.error);
    }
}

#endif
