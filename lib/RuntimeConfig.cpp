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

RuntimeConfig rc_get_current() {
    RuntimeConfig rc;
    rc.pgm = getProgram();
    rc.loc = get_runtime_location();
    rc.stmt = get_runtime_statement();
    rc.po = runtime_get_parse_options();
    rc.tlpd = get_thread_local_program_data();
    runtime_get_object_and_class(rc.obj, rc.cls);
    rc.stack_loc = get_runtime_stack_location();
    rc.return_type_info = getReturnTypeInfo();
    rc.element = get_implicit_element();
    rc.closure_env = thread_get_runtime_closure_env();
    return rc;
}

RuntimeConfig rc_get_parse_time() {
    RuntimeConfig rc;
    // For parse-time, we only need the program for parse options access
    // Other fields are left as defaults (nullptr/0)
    rc.pgm = getProgram();
    rc.po = runtime_get_parse_options();
    // tlpd may be available during parsing, but we don't require it
    rc.tlpd = get_thread_local_program_data();
    return rc;
}

// Thread-local RuntimeConfig for use when we want to avoid stack allocation
static thread_local RuntimeConfig tl_runtime_config;

RuntimeConfig& rc_get_current_ref() {
    tl_runtime_config.pgm = getProgram();
    tl_runtime_config.loc = get_runtime_location();
    tl_runtime_config.stmt = get_runtime_statement();
    tl_runtime_config.po = runtime_get_parse_options();
    tl_runtime_config.tlpd = get_thread_local_program_data();
    runtime_get_object_and_class(tl_runtime_config.obj, tl_runtime_config.cls);
    tl_runtime_config.stack_loc = get_runtime_stack_location();
    tl_runtime_config.return_type_info = getReturnTypeInfo();
    tl_runtime_config.element = get_implicit_element();
    tl_runtime_config.closure_env = thread_get_runtime_closure_env();
    return tl_runtime_config;
}

void rc_sync_to_thread(const RuntimeConfig& rc) {
    // Update runtime location/statement if changed
    update_runtime_statement_location(rc.stmt, rc.loc, rc.po);
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
        int64_t new_po)
    : rc(rc), old_loc(rc.loc), old_stmt(rc.stmt), old_po(rc.po), restore_po(new_po >= 0) {
    rc.loc = new_loc;
    if (new_stmt) {
        rc.stmt = new_stmt;
    }
    if (new_po >= 0) {
        rc.po = new_po;
    }
}

RuntimeConfigLocationHelper::~RuntimeConfigLocationHelper() {
    rc.loc = old_loc;
    rc.stmt = old_stmt;
    if (restore_po) {
        rc.po = old_po;
    }
}

RuntimeConfigObjectHelper::RuntimeConfigObjectHelper(RuntimeConfig& rc,
        QoreObject* new_obj,
        const qore_class_private* new_cls)
    : rc(rc), old_obj(rc.obj), old_cls(rc.cls) {
    rc.obj = new_obj;
    rc.cls = new_cls;
}

RuntimeConfigObjectHelper::~RuntimeConfigObjectHelper() {
    rc.obj = old_obj;
    rc.cls = old_cls;
}

RuntimeConfigClosureHelper::RuntimeConfigClosureHelper(RuntimeConfig& rc,
        const QoreClosureBase* new_closure_env)
    : rc(rc), old_closure_env(rc.closure_env) {
    rc.closure_env = new_closure_env;
}

RuntimeConfigClosureHelper::~RuntimeConfigClosureHelper() {
    rc.closure_env = old_closure_env;
}

RuntimeConfigElementHelper::RuntimeConfigElementHelper(RuntimeConfig& rc, int new_element)
    : rc(rc), old_element(rc.element) {
    rc.element = new_element;
}

RuntimeConfigElementHelper::~RuntimeConfigElementHelper() {
    rc.element = old_element;
}

RuntimeConfigStackHelper::RuntimeConfigStackHelper(RuntimeConfig& rc, QoreStackLocation* stack_loc)
    : rc(rc), old_stack_loc(rc.stack_loc) {
    // Link new stack location to the previous one
    stack_loc->setNext(old_stack_loc);

    // Update RuntimeConfig (fast, no lock needed)
    rc.stack_loc = stack_loc;

    // Sync to TLS for cross-thread stack access
    // Read lock is needed because other threads may be reading the stack
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    set_thread_stack_location(stack_loc);
}

RuntimeConfigStackHelper::~RuntimeConfigStackHelper() {
    // Restore RuntimeConfig (fast, no lock needed)
    rc.stack_loc = old_stack_loc;

    // Sync to TLS for cross-thread stack access
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    set_thread_stack_location(old_stack_loc);
}
