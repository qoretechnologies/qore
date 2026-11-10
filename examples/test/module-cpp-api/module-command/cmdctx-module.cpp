/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file cmdctx-module.cpp test binary modules with a module command handler

    Test scaffolding for QoreModuleCommandContext (issue #5699): this file is built twice, as the modules
    \c cmdctxaware, whose descriptor sets QoreModuleInfo::cmd_caller_aware, and \c cmdctxlegacy, which does not (as a
    module built with module API 2.0).  Neither is installed: both are built into the build tree.

    The handler accepts these commands:
    - \c record: appends one entry (the origin, the restricting parse options, whether the issuing code is sandboxed,
      whether the issuing code runs in the target Program, and whether the target is the handler's own Program) to the
      module's history
    - \c log: raises \c CMDCTX-LOG with the history, which it clears (the way tests read it back)
    - \c code: checks that the issuing code may run code (PO_NO_UNCONTROLLED_APIS, PO_NO_EMBEDDED_LOGIC, sandbox)
    - \c path: checks that the issuing code may change a search path (PO_NO_FILESYSTEM, sandbox)
    - \c thread: copies the restrictions and checks them on another thread; raises \c CMDCTX-THREAD with the result
    - \c nested: issues \c record to the same Program as a command of its own
    - \c outside: raises \c CMDCTX-OUTSIDE with whether QoreModuleCommandContext::get() is null on another thread
    Anything else is an error.

    The module also has the function CmdCtxAware::caller() (CmdCtxLegacy::caller()), which returns the description of
    QoreModuleCommandRestrictions::getCallerRestrictions() ("po=none|restricted sandboxed=0|1").

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
*/
#include <qore/Qore.h>

#include <cassert>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if !defined(CMDCTX_ID) || !defined(CMDCTX_NAME) || !defined(CMDCTX_AWARE)
#error CMDCTX_ID, CMDCTX_NAME and CMDCTX_AWARE must be defined
#endif

#define CMDCTX_CAT2(a, b) a ## b
#define CMDCTX_CAT(a, b) CMDCTX_CAT2(a, b)
#define CMDCTX_DESC_FUNC CMDCTX_CAT(CMDCTX_ID, _qore_module_desc)

namespace {
//! the history that the \c record command writes
std::mutex history_lock;
std::vector<std::string> history;

//! describes a context as a history entry
std::string describe(const QoreModuleCommandContext& ctx) {
    char buf[256];
    QoreParseOptions po = ctx.getParseOptions();
    snprintf(buf, sizeof(buf), "origin=%d po=%s sandboxed=%d issuer_is_target=%d target_is_handler=%d",
        static_cast<int>(ctx.getOrigin()), po ? "restricted" : "none", ctx.isSandboxed() ? 1 : 0,
        ctx.getIssuerProgram() == ctx.getTargetProgram() ? 1 : 0, ctx.getTargetProgram() == getProgram() ? 1 : 0);
    return buf;
}

void cmdctx_module_parse_cmd(const QoreString& cmd, ExceptionSink* xsink) {
    const QoreModuleCommandContext* ctx = QoreModuleCommandContext::get();
    // the context is always set for a command
    assert(ctx);
    if (!ctx) {
        xsink->raiseException("CMDCTX-ERROR", "no module command context");
        return;
    }

    if (cmd == "record") {
        std::lock_guard<std::mutex> lck(history_lock);
        history.push_back(describe(*ctx));
        return;
    }

    if (cmd == "log") {
        std::string desc;
        {
            std::lock_guard<std::mutex> lck(history_lock);
            for (const std::string& entry : history) {
                if (!desc.empty()) {
                    desc += ";";
                }
                desc += entry;
            }
            history.clear();
        }
        xsink->raiseException("CMDCTX-LOG", "%s", desc.c_str());
        return;
    }

    if (cmd == "code") {
        ctx->check(PO_NO_UNCONTROLLED_APIS | PO_NO_EMBEDDED_LOGIC, true, "CMDCTX-DENIED", "code", xsink);
        return;
    }

    if (cmd == "path") {
        ctx->check(PO_NO_FILESYSTEM, true, "CMDCTX-DENIED", "path", xsink);
        return;
    }

    if (cmd == "thread") {
        // the context is valid only here: another thread gets a copy
        QoreModuleCommandRestrictions copy = ctx->copy();
        bool denied = false;
        // a thread that Qore does not know raises no exception (check() does): it asks the copy
        std::thread t([&copy, &denied]() {
            denied = copy.isSandboxed() || copy.hasAny(PO_NO_UNCONTROLLED_APIS);
        });
        t.join();
        xsink->raiseException("CMDCTX-THREAD", denied ? "denied" : "allowed");
        return;
    }

    if (cmd == "nested") {
        QoreString record("record");
        MM.issueRuntimeCmd(CMDCTX_NAME, getProgram(), record, xsink);
        return;
    }

    if (cmd == "outside") {
        bool is_null = false;
        std::thread t([&is_null]() {
            is_null = !QoreModuleCommandContext::get();
        });
        t.join();
        xsink->raiseException("CMDCTX-OUTSIDE", is_null ? "null" : "set");
        return;
    }

    xsink->raiseException("CMDCTX-ERROR", "unknown command '%s'", cmd.c_str());
}

void cmdctx_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
}

//! the function CmdCtxAware::caller(): describes the restrictions of the code that calls it
QoreValue f_cmdctx_caller(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreModuleCommandRestrictions r = QoreModuleCommandRestrictions::getCallerRestrictions();
    char buf[64];
    snprintf(buf, sizeof(buf), "po=%s sandboxed=%d", r.getParseOptions() ? "restricted" : "none",
        r.isSandboxed() ? 1 : 0);
    return new QoreStringNode(buf);
}

void cmdctx_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    QoreNamespace* ns = new QoreNamespace(CMDCTX_AWARE ? "CmdCtxAware" : "CmdCtxLegacy");
    ns->addBuiltinVariant("caller", f_cmdctx_caller, QCF_NO_FLAGS, QDOM_DEFAULT, stringTypeInfo);
    qns->addNamespace(ns);
}

void cmdctx_module_delete() {
}
}

extern "C" DLLEXPORT void CMDCTX_DESC_FUNC(QoreModuleInfo& mod_info) {
    mod_info.name = CMDCTX_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module with a module command handler";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = cmdctx_module_init;
    mod_info.ns_init = cmdctx_module_ns_init;
    mod_info.del = cmdctx_module_delete;
    mod_info.parse_cmd = cmdctx_module_parse_cmd;
    mod_info.cmd_caller_aware = CMDCTX_AWARE;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
