/* -*- indent-tabs-mode: nil -*- */
/*
    RuntimeConfig.h

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_RUNTIMECONFIG_H
#define _QORE_INTERN_RUNTIMECONFIG_H

#include <qore/RuntimeConfig.h>

/**
 * @brief Runtime configuration passed through the evaluation call chain.
 *
 * This class holds pointers to runtime context that would otherwise
 * be looked up from thread-local storage (TLS) on each access. By passing
 * this explicitly through evalImpl() calls, we can:
 * 1. Reduce TLS lookups (important for performance)
 * 2. Enable future JIT compilation where TLS access is expensive
 *
 * Fields may be nullptr in some contexts (e.g., during module loading before
 * thread-local program data is set up). Code using RuntimeConfig must handle
 * nullptr gracefully or fall back to TLS-based lookups when needed.
 */
class ExceptionSink;

class RuntimeConfig::Impl {
public:
    //! Current program being executed
    QoreProgram* pgm = nullptr;

    //! Current source location for error reporting
    const QoreProgramLocation* loc = nullptr;

    //! Current statement being executed
    const AbstractStatement* stmt = nullptr;

    //! Current parse options
    QoreParseOptions po;

    //! Thread-local program data (may be nullptr during initialization)
    ThreadLocalProgramData* tlpd = nullptr;

    //! Current object context (if in method)
    QoreObject* obj = nullptr;

    //! Current class context (if in method or class code)
    const qore_class_private* cls = nullptr;

    //! Current stack location for debugging
    const QoreStackLocation* stack_loc = nullptr;

    //! Current return type info
    const QoreTypeInfo* return_type_info = nullptr;

    //! Current runtime flags
    q_rt_flags_t rtflags = 0;

    //! Current implicit element offset ($#)
    int element = 0;

    //! Current closure environment
    const QoreClosureBase* closure_env = nullptr;

    //! Check if this RuntimeConfig has valid thread-local program data
    bool hasThreadLocalData() const {
        return tlpd != nullptr;
    }

    //! Check if this RuntimeConfig is valid for runtime operations
    bool isValid() const {
        return pgm != nullptr;
    }
};

/**
 * @brief Get the current runtime configuration from thread-local storage.
 *
 * This function captures the current runtime context from TLS into a
 * RuntimeConfig struct. It should be called at entry points (like
 * AbstractQoreNode::eval()) and the result passed through the call chain.
 *
 * @return RuntimeConfig populated with current TLS values
 */
DLLLOCAL RuntimeConfig rc_get_current();

/**
 * @brief Get a minimal RuntimeConfig for parse-time constant folding.
 *
 * This returns a RuntimeConfig with minimal initialization suitable for
 * evaluating constant expressions at parse time. Unlike rc_get_current(),
 * this does not access TLS and can be used safely during parsing.
 *
 * The returned RuntimeConfig has:
 * - pgm set to the current parse program (if available)
 * - Other fields set to nullptr or default values
 *
 * @return RuntimeConfig suitable for parse-time evaluation
 */
DLLLOCAL RuntimeConfig rc_get_parse_time();

/**
 * @brief Get a reference to the thread-local RuntimeConfig updated with current values.
 *
 * This version avoids stack allocation by using a thread-local RuntimeConfig.
 * Use this in hot paths where stack space is at a premium (e.g., deep recursion).
 *
 * @warning The returned reference is only valid until the next call to this function
 * on the same thread. Do not store the reference across calls.
 *
 * @return Reference to thread-local RuntimeConfig with current values
 */
DLLLOCAL RuntimeConfig& rc_get_current_ref();

/**
 * @brief Get a direct reference to the thread-local RuntimeConfig.
 *
 * This returns a reference to the authoritative TLS RuntimeConfig that stores
 * runtime location, statement, and parse options. Use this for direct updates
 * instead of calling separate TLS update functions.
 *
 * @return Reference to the thread-local RuntimeConfig
 */
DLLLOCAL RuntimeConfig& rc_get_tls_ref();

/**
 * @brief Sync runtime config changes back to thread-local storage.
 *
 * Call this when the RuntimeConfig has been modified and those changes
 * need to be reflected in TLS (e.g., for nested eval calls that don't
 * use RuntimeConfig).
 *
 * @param rc The RuntimeConfig to sync to TLS
 */
DLLLOCAL void rc_sync_to_thread(const RuntimeConfig& rc);

/**
 * @brief RAII helper to manage RuntimeConfig and sync on destruction.
 */
class RuntimeConfigHelper {
public:
    DLLLOCAL RuntimeConfigHelper();
    DLLLOCAL ~RuntimeConfigHelper();

    DLLLOCAL RuntimeConfig& get() { return config; }
    DLLLOCAL const RuntimeConfig& get() const { return config; }

    //! Mark that changes need to be synced back to TLS
    DLLLOCAL void markDirty() { needs_sync = true; }

private:
    RuntimeConfig config;
    bool needs_sync;
};

/**
 * @brief RAII helper to temporarily change location in RuntimeConfig.
 */
class RuntimeConfigLocationHelper {
public:
    DLLLOCAL RuntimeConfigLocationHelper(RuntimeConfig& rc,
        const QoreProgramLocation* new_loc,
        const AbstractStatement* new_stmt = nullptr,
        bool has_po = false,
        const QoreParseOptions& new_po = QoreParseOptions(),
        ExceptionSink* xsink = nullptr);
    DLLLOCAL ~RuntimeConfigLocationHelper();

private:
    RuntimeConfig& rc;
    const QoreProgramLocation* old_loc;
    const AbstractStatement* old_stmt;
    QoreParseOptions old_po;
    const QoreProgramLocation* tls_old_loc;
    const AbstractStatement* tls_old_stmt;
    QoreParseOptions tls_old_po;
    bool restore_po;
    bool used_swap;
};

/**
 * @brief RAII helper to temporarily change object/class context in RuntimeConfig.
 */
class RuntimeConfigObjectHelper {
public:
    DLLLOCAL RuntimeConfigObjectHelper(RuntimeConfig& rc,
        QoreObject* new_obj,
        const qore_class_private* new_cls);
    DLLLOCAL ~RuntimeConfigObjectHelper();

private:
    RuntimeConfig& rc;
    QoreObject* old_obj;
    const qore_class_private* old_cls;
};

/**
 * @brief RAII helper to temporarily change closure environment in RuntimeConfig.
 */
class RuntimeConfigClosureHelper {
public:
    DLLLOCAL RuntimeConfigClosureHelper(RuntimeConfig& rc,
        const QoreClosureBase* new_closure_env);
    DLLLOCAL ~RuntimeConfigClosureHelper();

private:
    RuntimeConfig& rc;
    const QoreClosureBase* old_closure_env;
};

/**
 * @brief RAII helper to temporarily change implicit element in RuntimeConfig.
 *
 * This helper updates both the passed RuntimeConfig and tl_runtime_config
 * (the authoritative TLS source) so that code using either mechanism will
 * see the correct element value. This is necessary because method calls may
 * create a new RuntimeConfig via rc_get_current() which reads from TLS.
 */
class RuntimeConfigElementHelper {
public:
    DLLLOCAL RuntimeConfigElementHelper(RuntimeConfig& rc, int new_element);
    DLLLOCAL ~RuntimeConfigElementHelper();

private:
    RuntimeConfig& rc;
    int old_element;
};

/**
 * @brief RAII helper to push/pop stack location in RuntimeConfig and sync to TLS.
 *
 * This helper maintains the call stack in both RuntimeConfig (for fast access)
 * and TLS (for cross-thread stack access via get_all_thread_call_stacks()).
 *
 * Benefits:
 * - Exception reporting can use rc.getStackLocation() directly (avoids TLS read)
 * - Future JIT can keep stack_loc in register
 * - Cross-thread stack access still works via TLS sync
 */
class RuntimeConfigStackHelper {
public:
    /**
     * @brief Push a new stack location.
     *
     * @param rc RuntimeConfig to update
     * @param stack_loc The stack location to push (must outlive this helper)
     */
    DLLLOCAL RuntimeConfigStackHelper(RuntimeConfig& rc, QoreStackLocation* stack_loc);

    /**
     * @brief Pop the stack location and restore previous.
     */
    DLLLOCAL ~RuntimeConfigStackHelper();

private:
    RuntimeConfig& rc;
    const QoreStackLocation* old_stack_loc;
};

#endif // _QORE_INTERN_RUNTIMECONFIG_H
