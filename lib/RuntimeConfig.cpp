/* -*- indent-tabs-mode: nil -*- */
/*
    RuntimeConfig.cpp

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

#include <qore/Qore.h>
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/QoreLibIntern.h"

RuntimeConfig::RuntimeConfig() : impl(new Impl()), owned(true) {
}

RuntimeConfig::RuntimeConfig(Impl* impl, bool owned) : impl(impl), owned(owned) {
}

RuntimeConfig::RuntimeConfig(const RuntimeConfig& other) : impl(nullptr), owned(false) {
    if (other.impl) {
        impl = new Impl(*other.impl);
        owned = true;
    }
}

RuntimeConfig::RuntimeConfig(RuntimeConfig&& other) noexcept : impl(other.impl), owned(other.owned) {
    other.impl = nullptr;
    other.owned = false;
}

RuntimeConfig& RuntimeConfig::operator=(const RuntimeConfig& other) {
    if (this == &other) {
        return *this;
    }
    if (owned && impl) {
        delete impl;
    }
    impl = nullptr;
    owned = false;
    if (other.impl) {
        impl = new Impl(*other.impl);
        owned = true;
    }
    return *this;
}

RuntimeConfig& RuntimeConfig::operator=(RuntimeConfig&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (owned && impl) {
        delete impl;
    }
    impl = other.impl;
    owned = other.owned;
    other.impl = nullptr;
    other.owned = false;
    return *this;
}

RuntimeConfig::~RuntimeConfig() {
    if (owned && impl) {
        delete impl;
    }
}

RuntimeConfig::Impl& RuntimeConfig::implRef() {
    if (!impl) {
        impl = new Impl();
        owned = true;
    }
    return *impl;
}

const RuntimeConfig::Impl& RuntimeConfig::implRef() const {
    if (!impl) {
        static const Impl empty;
        return empty;
    }
    return *impl;
}

QoreProgram* RuntimeConfig::getProgram() const {
    return implRef().pgm;
}

void RuntimeConfig::setProgram(QoreProgram* pgm) {
    implRef().pgm = pgm;
}

const QoreProgramLocation* RuntimeConfig::getLocation() const {
    return implRef().loc;
}

void RuntimeConfig::setLocation(const QoreProgramLocation* loc) {
    implRef().loc = loc;
}

const AbstractStatement* RuntimeConfig::getStatement() const {
    return implRef().stmt;
}

void RuntimeConfig::setStatement(const AbstractStatement* stmt) {
    implRef().stmt = stmt;
}

int64_t RuntimeConfig::getParseOptions() const {
    return implRef().po;
}

void RuntimeConfig::setParseOptions(int64_t po) {
    implRef().po = po;
}

ThreadLocalProgramData* RuntimeConfig::getThreadLocalProgramData() const {
    return implRef().tlpd;
}

void RuntimeConfig::setThreadLocalProgramData(ThreadLocalProgramData* tlpd) {
    implRef().tlpd = tlpd;
}

QoreObject* RuntimeConfig::getObject() const {
    return implRef().obj;
}

void RuntimeConfig::setObject(QoreObject* obj) {
    implRef().obj = obj;
}

const qore_class_private* RuntimeConfig::getClass() const {
    return implRef().cls;
}

void RuntimeConfig::setClass(const qore_class_private* cls) {
    implRef().cls = cls;
}

const QoreStackLocation* RuntimeConfig::getStackLocation() const {
    return implRef().stack_loc;
}

void RuntimeConfig::setStackLocation(const QoreStackLocation* stack_loc) {
    implRef().stack_loc = stack_loc;
}

const QoreTypeInfo* RuntimeConfig::getReturnTypeInfo() const {
    return implRef().return_type_info;
}

void RuntimeConfig::setReturnTypeInfo(const QoreTypeInfo* return_type_info) {
    implRef().return_type_info = return_type_info;
}

q_rt_flags_t RuntimeConfig::getRuntimeFlags() const {
    return implRef().rtflags;
}

void RuntimeConfig::setRuntimeFlags(q_rt_flags_t flags) {
    implRef().rtflags = flags;
}

int RuntimeConfig::getElement() const {
    return implRef().element;
}

void RuntimeConfig::setElement(int element) {
    implRef().element = element;
}

const QoreClosureBase* RuntimeConfig::getClosureEnv() const {
    return implRef().closure_env;
}

void RuntimeConfig::setClosureEnv(const QoreClosureBase* closure_env) {
    implRef().closure_env = closure_env;
}

bool RuntimeConfig::hasThreadLocalData() const {
    return implRef().hasThreadLocalData();
}

bool RuntimeConfig::isValid() const {
    return implRef().isValid();
}

RuntimeConfig rc_get_current() {
    // Get loc, stmt, po, element from the authoritative TLS RuntimeConfig
    RuntimeConfig& tls_rc = rc_get_tls_ref();
    RuntimeConfig rc;
    rc.setProgram(getProgram());
    rc.setLocation(tls_rc.getLocation());
    rc.setStatement(tls_rc.getStatement());
    rc.setParseOptions(tls_rc.getParseOptions());
    rc.setRuntimeFlags(tls_rc.getRuntimeFlags());
    rc.setElement(tls_rc.getElement());  // element is now stored in tl_runtime_config
    rc.setThreadLocalProgramData(get_thread_local_program_data());
    QoreObject* obj = nullptr;
    const qore_class_private* cls = nullptr;
    runtime_get_object_and_class(obj, cls);
    rc.setObject(obj);
    rc.setClass(cls);
    rc.setStackLocation(get_runtime_stack_location());
    rc.setReturnTypeInfo(getReturnTypeInfo());
    // closure_env is not directly accessible via getter - leave as nullptr
    return rc;
}

RuntimeConfig rc_get_parse_time() {
    RuntimeConfig rc;
    // For parse-time, we only need the program for parse options access
    // Other fields are left as defaults (nullptr/0)
    rc.setProgram(getProgram());
    rc.setParseOptions(rc_get_tls_ref().getParseOptions());
    rc.setRuntimeFlags(rc_get_tls_ref().getRuntimeFlags());
    // tlpd may be available during parsing, but we don't require it
    rc.setThreadLocalProgramData(get_thread_local_program_data());
    return rc;
}

// Get a reference to the TLS RuntimeConfig for direct updates
// Ensures loc is initialized to &loc_builtin if not yet set
RuntimeConfig& rc_get_tls_ref() {
    // Thread-local RuntimeConfig - the authoritative source for runtime context
    // Updated by RuntimeConfigLocationHelper and other helpers
    static thread_local RuntimeConfig::Impl tl_runtime_config_impl;
    static thread_local RuntimeConfig tl_runtime_config(&tl_runtime_config_impl, false);
    if (!tl_runtime_config.getLocation()) {
        tl_runtime_config.setLocation(&loc_builtin);
    }
    return tl_runtime_config;
}

RuntimeConfig& rc_get_current_ref() {
    // Update fields not managed by helpers from TLS
    RuntimeConfig& tls_rc = rc_get_tls_ref();
    if (!is_valid_qore_thread()) {
        // Shutdown or non-Qore threads: thread data is unavailable
        tls_rc.setProgram(nullptr);
        tls_rc.setThreadLocalProgramData(nullptr);
        tls_rc.setObject(nullptr);
        tls_rc.setClass(nullptr);
        tls_rc.setStackLocation(nullptr);
        tls_rc.setReturnTypeInfo(nullptr);
        return tls_rc;
    }
    tls_rc.setProgram(getProgram());
    tls_rc.setThreadLocalProgramData(get_thread_local_program_data());
    QoreObject* obj = nullptr;
    const qore_class_private* cls = nullptr;
    runtime_get_object_and_class(obj, cls);
    tls_rc.setObject(obj);
    tls_rc.setClass(cls);
    tls_rc.setStackLocation(get_runtime_stack_location());
    tls_rc.setReturnTypeInfo(getReturnTypeInfo());
    // closure_env is not directly accessible via getter - leave as nullptr
    // loc, stmt, po, element are managed by helpers - don't overwrite
    return tls_rc;
}

void rc_sync_to_thread(const RuntimeConfig& rc) {
    // Update runtime location/statement if changed
    update_runtime_statement_location(rc.getStatement(), rc.getLocation(), rc.getParseOptions());
    rc_get_tls_ref().setRuntimeFlags(rc.getRuntimeFlags());
    // Note: Other fields (pgm, tlpd, obj, cls) are typically managed via
    // RAII helpers like ProgramThreadCountContextHelper and ObjectSubstitutionHelper
    // and should not be modified directly here to avoid breaking invariants.
}

RuntimeConfigHelper::RuntimeConfigHelper() : config(rc_get_current()), needs_sync(false) {
}

RuntimeConfigHelper::~RuntimeConfigHelper() {
    if (needs_sync) {
        rc_sync_to_thread(config);
    }
}

RuntimeConfigLocationHelper::RuntimeConfigLocationHelper(RuntimeConfig& rc,
        const QoreProgramLocation* new_loc,
        const AbstractStatement* new_stmt,
        bool has_po,
        int64_t new_po,
        ExceptionSink* xsink)
    : rc(rc), old_loc(rc.getLocation()), old_stmt(rc.getStatement()), old_po(rc.getParseOptions()),
      tls_old_loc(rc.getLocation()), tls_old_stmt(rc.getStatement()), tls_old_po(rc.getParseOptions()),
      restore_po(has_po), used_swap(false) {
    rc.setLocation(new_loc);
    if (new_stmt) {
        rc.setStatement(new_stmt);
    }
    if (has_po) {
        rc.setParseOptions(new_po);
    }
    // Sync to TLS so code that falls back to TLS (like rc_get_current() from contexts
    // without RuntimeConfig access) gets the correct location
    if (xsink) {
        used_swap = true;
        swap_runtime_statement_location(xsink, rc.getStatement(), rc.getLocation(), rc.getParseOptions(),
            tls_old_stmt, tls_old_loc, tls_old_po);
    } else {
        update_runtime_statement_location(rc.getStatement(), rc.getLocation(), rc.getParseOptions());
    }
}

RuntimeConfigLocationHelper::~RuntimeConfigLocationHelper() {
    rc.setLocation(old_loc);
    rc.setStatement(old_stmt);
    if (restore_po) {
        rc.setParseOptions(old_po);
    }
    // Restore TLS to old values
    if (restore_po) {
        update_runtime_statement_location(tls_old_stmt, tls_old_loc, tls_old_po);
    } else if (used_swap) {
        update_runtime_statement_location(tls_old_stmt, tls_old_loc);
    } else {
        update_runtime_statement_location(old_stmt, old_loc, rc.getParseOptions());
    }
}

RuntimeConfigObjectHelper::RuntimeConfigObjectHelper(RuntimeConfig& rc,
        QoreObject* new_obj,
        const qore_class_private* new_cls)
    : rc(rc), old_obj(rc.getObject()), old_cls(rc.getClass()) {
    rc.setObject(new_obj);
    rc.setClass(new_cls);
}

RuntimeConfigObjectHelper::~RuntimeConfigObjectHelper() {
    rc.setObject(old_obj);
    rc.setClass(old_cls);
}

RuntimeConfigClosureHelper::RuntimeConfigClosureHelper(RuntimeConfig& rc,
        const QoreClosureBase* new_closure_env)
    : rc(rc), old_closure_env(rc.getClosureEnv()) {
    rc.setClosureEnv(new_closure_env);
}

RuntimeConfigClosureHelper::~RuntimeConfigClosureHelper() {
    rc.setClosureEnv(old_closure_env);
}

RuntimeConfigElementHelper::RuntimeConfigElementHelper(RuntimeConfig& rc, int new_element)
    : rc(rc), old_element(rc.getElement()) {
    rc.setElement(new_element);
    // Update tl_runtime_config directly - it's the authoritative source for element
    rc_get_tls_ref().setElement(new_element);
}

RuntimeConfigElementHelper::~RuntimeConfigElementHelper() {
    rc.setElement(old_element);
    rc_get_tls_ref().setElement(old_element);
}

RuntimeConfigStackHelper::RuntimeConfigStackHelper(RuntimeConfig& rc, QoreStackLocation* stack_loc)
    : rc(rc), old_stack_loc(rc.getStackLocation()) {
    // Link new stack location to the previous one
    stack_loc->setNext(old_stack_loc);

    // Update RuntimeConfig (fast, no lock needed)
    rc.setStackLocation(stack_loc);

    // Sync to TLS for cross-thread stack access
    update_runtime_stack_location(stack_loc);
}

RuntimeConfigStackHelper::~RuntimeConfigStackHelper() {
    // Restore RuntimeConfig (fast, no lock needed)
    rc.setStackLocation(old_stack_loc);

    // Sync to TLS for cross-thread stack access
    update_runtime_stack_location(old_stack_loc);
}
