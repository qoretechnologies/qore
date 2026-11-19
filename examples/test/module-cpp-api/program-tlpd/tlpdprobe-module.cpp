/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file tlpdprobe-module.cpp a test binary module that runs code of a Program with and without entering it

    Test scaffolding for qore#6185 and qore#6537: checks that code of a Program runs with the thread-local data of
    that Program when a binary module makes the Program current or enters it.  It is built into the build tree and
    never installed.

    The module has, in the namespace \c TlpdProbe:
    - <tt>auto callSwitched(Program pgm, code c)</tt>: makes @p pgm current with QoreProgramContextHelper, and calls
      @p c
    - <tt>auto callEntered(Program pgm, code c)</tt>: enters @p pgm with QoreExternalProgramContextHelper, and calls
      @p c
    - <tt>auto callNested(Program pgm, int depth, code c)</tt>: makes @p pgm and the calling Program current in turn
      with nested QoreProgramContextHelper objects, @p depth levels deep (an odd number: the innermost level makes
      @p pgm current), then enters @p pgm with QoreExternalProgramContextHelper and calls @p c
    - <tt>hash<auto> switchDeleteEnter(Program pgm, code deleter)</tt>: checks that an entry into a Program deleted
      while it is current fails; returns \c entered (True if the entry succeeded) and \c error (the name of the
      exception of the entry, if any)
    - <tt>*string lastCommandResult()</tt>: the result of the last module command <tt>call <function></tt>
    - <tt>nothing switchOnly(Program pgm)</tt>: switches the current Program to @p pgm with QoreProgramContextHelper
      and back, running nothing, as for a metadata lookup: the switch must not create the thread's data in the
      Program or run its thread initialization code (qore#6537)
    - the module command <tt>call <function></tt>: enters the Program that the command is issued to with
      QoreExternalProgramContextHelper, calls the function there and records its result as a string
      (\c "NOTHING" for no value) or the error, as <tt>error: <err></tt>
    - the namespace initialization in a Program that defines <tt>tlpdprobe_on_load()</tt> calls it there and records
      its result as for the command, including when the module is loaded into the Program at run time
      (Program::loadModule())

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
#include <cstring>
#include <mutex>
#include <string>

namespace {
//! the namespace of the module, copied into each Program that loads it
QoreNamespace ProbeNs("TlpdProbe");

//! the lock of the result of the last module command
std::mutex cmd_result_lock;
//! the result of the last module command, if any
std::string cmd_result;
//! true once a module command has recorded a result
bool cmd_result_set = false;

//! records the result of a module command
void set_cmd_result(std::string result) {
    std::lock_guard<std::mutex> lck(cmd_result_lock);
    cmd_result = std::move(result);
    cmd_result_set = true;
}

//! returns the Program of the first argument with a reference, or nullptr with an exception
QoreProgram* get_program_arg(const QoreListNode* args, ExceptionSink* xsink) {
    QoreObject* obj = HARD_QORE_VALUE_OBJECT(args, 0);
    assert(obj);
    return static_cast<QoreProgram*>(obj->getReferencedPrivateData(CID_PROGRAM, xsink));
}

//! returns the code of the second argument
const ResolvedCallReferenceNode* get_code_arg(const QoreListNode* args) {
    const ResolvedCallReferenceNode* c = get_hard_value_param(args, 1).get<const ResolvedCallReferenceNode>();
    assert(c);
    return c;
}

QoreValue f_callSwitched(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreProgram> pgm(get_program_arg(args, xsink), xsink);
    if (!pgm) {
        return QoreValue();
    }
    QoreProgramContextHelper pch(*pgm);
    return get_code_arg(args)->invoke(nullptr, xsink);
}

//! one level of callNested(): odd levels make \a inner current, even levels \a outer
QoreValue nested_call(QoreProgram* outer, QoreProgram* inner, int64 level, int64 depth,
        const ResolvedCallReferenceNode* c, ExceptionSink* xsink) {
    QoreProgramContextHelper pch((level & 1) ? inner : outer);
    if (level < depth) {
        return nested_call(outer, inner, level + 1, depth, c, xsink);
    }
    QoreExternalProgramContextHelper epch(xsink, inner);
    if (*xsink) {
        return QoreValue();
    }
    return c->invoke(nullptr, xsink);
}

QoreValue f_callNested(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreProgram> pgm(get_program_arg(args, xsink), xsink);
    if (!pgm) {
        return QoreValue();
    }
    int64 depth = get_hard_value_param(args, 1).getAsBigInt();
    if (depth < 1 || !(depth & 1) || depth > 1000) {
        xsink->raiseException("TLPDPROBE-ERROR", "the depth must be an odd number from 1 to 999; got " QLLD,
            depth);
        return QoreValue();
    }
    const ResolvedCallReferenceNode* c = get_hard_value_param(args, 2).get<const ResolvedCallReferenceNode>();
    assert(c);
    QoreProgram* outer = getProgram();
    assert(outer);
    return nested_call(outer, *pgm, 1, depth, c, xsink);
}

QoreValue f_switchDeleteEnter(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreProgram> pgm(get_program_arg(args, xsink), xsink);
    if (!pgm) {
        return QoreValue();
    }
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    QoreProgramContextHelper pch(*pgm);
    {
        ValueHolder dv(get_code_arg(args)->invoke(nullptr, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
    }
    ExceptionSink xs;
    bool entered;
    {
        QoreExternalProgramContextHelper epch(&xs, *pgm);
        entered = !xs;
    }
    rv->setKeyValue("entered", entered, xsink);
    if (xs) {
        QoreStringValueHelper err(xs.getExceptionErr(), QCS_DEFAULT, xsink);
        rv->setKeyValue("error", new QoreStringNode(err->c_str()), xsink);
        xs.clear();
    }
    return rv.release();
}

QoreValue f_callEntered(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreProgram> pgm(get_program_arg(args, xsink), xsink);
    if (!pgm) {
        return QoreValue();
    }
    // a runtime entry into the Program
    QoreExternalProgramContextHelper epch(xsink, *pgm);
    if (*xsink) {
        return QoreValue();
    }
    return get_code_arg(args)->invoke(nullptr, xsink);
}

QoreValue f_switchOnly(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreProgram> pgm(get_program_arg(args, xsink), xsink);
    if (!pgm) {
        return QoreValue();
    }
    // a switch for a metadata lookup: no code of the Program runs under it
    QoreProgramContextHelper pch(*pgm);
    return QoreValue();
}

QoreValue f_lastCommandResult(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lck(cmd_result_lock);
    if (!cmd_result_set) {
        return QoreValue();
    }
    return new QoreStringNode(cmd_result);
}

//! returns the string form of a value for a command result
std::string cmd_value_string(const QoreValue& v, ExceptionSink* xsink) {
    if (v.isNothing()) {
        return "NOTHING";
    }
    QoreStringValueHelper str(v, QCS_DEFAULT, xsink);
    return *xsink ? std::string() : std::string(str->c_str());
}

//! the module command handler: <tt>call <function></tt>
void tlpdprobe_module_parse_cmd(const QoreString& cmd, ExceptionSink* xsink) {
    static const char prefix[] = "call ";
    const size_t prefix_len = sizeof(prefix) - 1;
    if (cmd.size() <= prefix_len || strncmp(cmd.c_str(), prefix, prefix_len)) {
        xsink->raiseException("TLPDPROBE-ERROR", "unknown command '%s'", cmd.c_str());
        return;
    }
    const char* fname = cmd.c_str() + prefix_len;
    QoreProgram* pgm = getProgram();
    assert(pgm);
    // the function's result, or its error, is recorded; the command itself succeeds
    ExceptionSink xs;
    std::string result;
    {
        // a module whose command runs code of the Program enters it
        QoreExternalProgramContextHelper epch(&xs, pgm);
        if (!xs) {
            ValueHolder rv(pgm->callFunction(fname, nullptr, &xs), &xs);
            if (!xs) {
                result = cmd_value_string(*rv, &xs);
            }
        }
    }
    if (xs) {
        QoreStringValueHelper err(xs.getExceptionErr(), QCS_DEFAULT, xsink);
        result = std::string("error: ") + err->c_str();
        xs.clear();
    }
    set_cmd_result(std::move(result));
}

void tlpdprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // the module's functions are created once; each Program gets a copy of the namespace
    ProbeNs.addBuiltinVariant(nullptr, "callSwitched", f_callSwitched, QCF_NO_FLAGS, QDOM_DEFAULT, anyTypeInfo, 2,
        QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm", codeTypeInfo, QORE_PARAM_NO_ARG, "c");
    ProbeNs.addBuiltinVariant(nullptr, "callEntered", f_callEntered, QCF_NO_FLAGS, QDOM_DEFAULT, anyTypeInfo, 2,
        QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm", codeTypeInfo, QORE_PARAM_NO_ARG, "c");
    ProbeNs.addBuiltinVariant(nullptr, "callNested", f_callNested, QCF_NO_FLAGS, QDOM_DEFAULT, anyTypeInfo, 3,
        QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm", bigIntTypeInfo, QORE_PARAM_NO_ARG, "depth",
        codeTypeInfo, QORE_PARAM_NO_ARG, "c");
    ProbeNs.addBuiltinVariant(nullptr, "switchDeleteEnter", f_switchDeleteEnter, QCF_NO_FLAGS, QDOM_DEFAULT,
        hashTypeInfo, 2, QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm", codeTypeInfo, QORE_PARAM_NO_ARG,
        "deleter");
    ProbeNs.addBuiltinVariant(nullptr, "lastCommandResult", f_lastCommandResult, QCF_NO_FLAGS, QDOM_DEFAULT,
        stringOrNothingTypeInfo);
    ProbeNs.addBuiltinVariant(nullptr, "switchOnly", f_switchOnly, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1,
        QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm");
}

void tlpdprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProbeNs.copy());

    // the namespace is initialized in the Program that the module is added to, which can be a Program that other
    // code loads the module into at run time (Program::loadModule()): its function tlpdprobe_on_load(), if it has
    // one, is called there; the result is recorded as for a module command
    QoreProgram* pgm = getProgram();
    if (!pgm || !pgm->findFunction("tlpdprobe_on_load")) {
        return;
    }
    ExceptionSink xs;
    std::string result;
    {
        QoreExternalProgramContextHelper epch(&xs, pgm);
        if (!xs) {
            ValueHolder rv(pgm->callFunction("tlpdprobe_on_load", nullptr, &xs), &xs);
            if (!xs) {
                result = cmd_value_string(*rv, &xs);
            }
        }
    }
    if (xs) {
        QoreStringValueHelper err(xs.getExceptionErr(), QCS_DEFAULT, &xsink);
        result = std::string("error: ") + err->c_str();
        xs.clear();
    }
    set_cmd_result(std::move(result));
}

void tlpdprobe_module_delete() {
}
}

extern "C" DLLEXPORT void tlpdprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "tlpdprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module that runs code of a Program with and without entering it";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = tlpdprobe_module_init;
    mod_info.ns_init = tlpdprobe_module_ns_init;
    mod_info.del = tlpdprobe_module_delete;
    mod_info.parse_cmd = tlpdprobe_module_parse_cmd;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
