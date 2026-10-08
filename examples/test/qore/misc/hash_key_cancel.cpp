// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include <qore/intern/QoreHashKeyHelper.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

static unsigned checks = 0;

static void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct CancelReset {
    ~CancelReset() {
        qore_clear_thread_cancel();
    }
};

static bool takeError(ExceptionSink& xsink, const char* expected) {
    QoreValue code = xsink.getExceptionErr();
    bool cancelled = code.getType() == NT_STRING
        && !std::strcmp(code.get<const QoreStringNode>()->c_str(), expected);
    xsink.clear();
    return cancelled;
}

static bool takeCancelled(ExceptionSink& xsink) {
    return takeError(xsink, "THREAD-CANCELLED");
}

static void scanBoundaries() {
    for (size_t size : {size_t(0), size_t(1), size_t(99), size_t(100), size_t(101), size_t(199),
            size_t(200), size_t(201), size_t(1024 * 1024)}) {
        ExceptionSink xsink;
        std::string bytes(size, 'a');
        require(QoreHashKeyHelper::usableAsIs(QCS_ISO_8859_1, bytes.data(), bytes.size(), &xsink)
            && !xsink, "ASCII scan failed at chunk boundary");
        for (size_t position : {size_t(0), size_t(99), size_t(100), size_t(199), size ? size - 1 : 0}) {
            if (position >= size) {
                continue;
            }
            bytes[position] = '\xe9';
            require(!QoreHashKeyHelper::usableAsIs(QCS_ISO_8859_1, bytes.data(), bytes.size(), &xsink)
                && !xsink, "non-ASCII byte was missed at chunk boundary");
            bytes[position] = 'a';
        }
        CancelReset reset;
        require(!qore_cancel_thread(q_gettid(), "ASCII scan boundary"), "cannot request scan cancellation");
        bool usable = QoreHashKeyHelper::usableAsIs(QCS_ISO_8859_1, bytes.data(), bytes.size(), &xsink);
        bool cancelled = takeCancelled(xsink);
        require(size ? !usable && cancelled : usable && !cancelled,
            "scan did not deliver cancellation, or empty scan invented a cancellation point");
        // The invariant-only overload must never deliver a language exception.
        require(QoreHashKeyHelper::usableAsIs(QCS_ISO_8859_1, bytes.data(), bytes.size()),
            "non-throwing invariant scan changed its result");
    }
}

static void publicLookups() {
    for (size_t size : {size_t(101), size_t(200), size_t(201), size_t(1024 * 1024)}) {
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
        std::string bytes(size, 'a');
        require(!hash->setKeyValue(bytes.c_str(), QoreValue(int64(73)), &xsink) && !xsink,
            "cannot populate hash fixture");
        QoreString key(bytes.data(), bytes.size(), QCS_ISO_8859_1);
        for (bool with_exists : {false, true}) {
            for (bool initial : {false, true}) {
                CancelReset reset;
                require(!qore_cancel_thread(q_gettid(), "public key lookup"), "cannot request lookup cancellation");
                bool exists = initial;
                QoreValue value = with_exists ? hash->getKeyValueExistence(key, exists, &xsink)
                    : hash->getKeyValue(key, &xsink);
                bool cancelled = takeCancelled(xsink);
                require(cancelled && value.isNothing(), "lookup returned a value after cancellation");
                require(!with_exists || !exists, "cancelled lookup retained the existence output");
                qore_clear_thread_cancel();
                value = hash->getKeyValueExistence(key, exists, &xsink);
                require(!xsink && exists && value.getAsBigInt() == 73, "lookup did not recover");
                require(key.size() == bytes.size() && !std::memcmp(key.c_str(), bytes.data(), bytes.size()),
                    "cancellation changed input key bytes");
            }
        }
        CancelReset reset;
        require(!qore_cancel_thread(q_gettid(), "deferred key lookup"), "cannot request deferred cancellation");
        {
            QoreCancelDeferralHelper defer;
            bool exists = false;
            QoreValue value = hash->getKeyValueExistence(key, exists, &xsink);
            require(!xsink && exists && value.getAsBigInt() == 73, "cleanup deferral did not permit lookup");
        }
        bool exists = true;
        QoreValue value = hash->getKeyValueExistence(key, exists, &xsink);
        bool cancelled = takeCancelled(xsink);
        require(cancelled && !exists && value.isNothing(), "deferred cancellation was lost");
    }
}

static void programInterruption() {
    ExceptionSink xsink;
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    require(!xsink, "cannot create sandbox Program");
    ReferenceHolder<QoreSandboxManager> manager(new QoreSandboxManager, &xsink);
    program->setSandboxManager(*manager);
    QoreProgramContextHelper context(*program);
    struct InterruptReset {
        QoreSandboxManager* manager;
        ~InterruptReset() {
            manager->clearInterrupt();
        }
    } reset{*manager};
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    std::string bytes(1024 * 1024, 'x');
    require(!hash->setKeyValue(bytes.c_str(), QoreValue(int64(29)), &xsink) && !xsink,
        "cannot create program-interruption fixture");
    QoreString key(bytes.data(), bytes.size(), QCS_ISO_8859_1);
    for (bool with_exists : {false, true}) {
        manager->requestInterrupt();
        bool exists = true;
        QoreValue value = with_exists ? hash->getKeyValueExistence(key, exists, &xsink)
            : hash->getKeyValue(key, &xsink);
        bool interrupted = takeError(xsink, "PROGRAM-INTERRUPTED");
        require(interrupted && value.isNothing() && (!with_exists || !exists),
            "lookup did not propagate program interruption");
        {
            QoreCancelDeferralHelper defer;
            value = hash->getKeyValueExistence(key, exists, &xsink);
            require(!xsink && exists && value.getAsBigInt() == 29, "program cleanup deferral failed");
        }
        value = hash->getKeyValueExistence(key, exists, &xsink);
        interrupted = takeError(xsink, "PROGRAM-INTERRUPTED");
        require(interrupted && !exists && value.isNothing(), "deferred program interruption was lost");
        manager->clearInterrupt();
        value = hash->getKeyValueExistence(key, exists, &xsink);
        require(!xsink && exists && value.getAsBigInt() == 29, "program did not recover after interruption");
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        scanBoundaries();
        publicLookups();
        programInterruption();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u hash key cancellation checks\n", checks);
    }
    return status;
}
