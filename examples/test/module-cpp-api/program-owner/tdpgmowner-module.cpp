/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file tdpgmowner-module.cpp test binary module that owns Programs, deleted when the module is deleted

    Test scaffolding for the module teardown order (see QoreModuleManager::cleanup() and "Module Teardown Order" in
    design/qore-module-structure.md): the module \c tdpgmowner is built into the build tree but never installed.  It
    stands for a native binary module like jni, which creates a Program of its own, imports modules into it, and
    deletes it only when the module is deleted:
    - <tt>TdPgmOwner::own(string source)</tt> parses and runs the given code in a new Program that the module owns
      until it is deleted; the code can import modules and keep objects of their classes in global and class static
      variables, which are released when the module deletes the Program
    - <tt>TdPgmOwner::Token</tt> is a class whose objects must be destroyed before the module is deleted: destroying
      one afterwards raises \c TD-TOKEN-ERROR; a module that requires this module and holds a token in a constant
      must therefore be deleted before it

    The module's name begins with a lower-case letter, so it sorts after the names of the test modules, as the names
    of most native binary modules sort after those of user modules (\c jni after \c QorusClientBase).

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

#include <atomic>
#include <cassert>
#include <vector>

namespace {
//! the namespace of the module
QoreNamespace* module_ns = nullptr;
//! the Programs that the module owns, in the order of their creation
/** Allocated in the module's initialization and freed in its deletion, so nothing of it is left for the destruction
    of static objects
*/
std::vector<QoreProgram*>* owned_programs = nullptr;
//! true once the module has been deleted
std::atomic<bool> module_deleted{false};

//! the private data of a token: destroying it after the module has been deleted is an error
class TokenData : public AbstractPrivateData {
public:
    virtual void deref(ExceptionSink* xsink) override {
        if (ROdereference()) {
            if (module_deleted.load()) {
                xsink->raiseException("TD-TOKEN-ERROR", "a TdPgmOwner::Token object is destroyed after the "
                    "tdpgmowner module was deleted");
            }
            delete this;
        }
    }
};

void token_constructor(const QoreMethod& method, const void*, QoreObject* self, const QoreListNode*, RuntimeConfig&,
        ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new TokenData);
}

void token_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    data->deref(xsink);
}

//! own(string source): parses and runs the source in a new Program that the module owns until it is deleted
QoreValue f_own(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(source, const QoreStringNode, args, 0);
    assert(owned_programs);
    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE);
    // the Program is owned from here on, also if it cannot be parsed or run, and deleted with the module
    owned_programs->push_back(pgm);
    QoreString label("tdpgmowner owned Program");
    pgm->parse(source, &label, xsink, nullptr, 0);
    if (*xsink) {
        return QoreValue();
    }
    pgm->run(xsink);
    return QoreValue();
}

//! owned_count(): returns the number of Programs the module owns
QoreValue f_owned_count(const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    assert(owned_programs);
    return static_cast<int64>(owned_programs->size());
}
}

static void tdpgmowner_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    owned_programs = new std::vector<QoreProgram*>;
    module_ns = new QoreNamespace("TdPgmOwner");
    module_ns->addBuiltinVariant("own", f_own, QCF_NO_FLAGS, QDOM_EMBEDDED_LOGIC, nothingTypeInfo, 1,
        stringTypeInfo, QORE_PARAM_NO_ARG, "source");
    module_ns->addBuiltinVariant("owned_count", f_owned_count, QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo);
    // QoreClass has a protected destructor; the namespace takes ownership
    QoreClass* token = new QoreClass("Token", "Qore::TdPgmOwner");
    token->addConstructor(nullptr, token_constructor);
    token->setDestructor(token_destructor);
    module_ns->addSystemClass(token);
}

static void tdpgmowner_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(module_ns->copy());
}

//! deletes the Programs that the module owns, then marks the module deleted
/** The exceptions raised by the destructors that run when the Programs are deleted are reported here, while the
    library is still fully initialized, and make the exit status of the process non-zero
*/
static void tdpgmowner_module_delete() {
    {
        ExceptionSink xsink;
        for (QoreProgram* pgm : *owned_programs) {
            pgm->waitForTerminationAndDeref(&xsink);
        }
    }
    delete owned_programs;
    owned_programs = nullptr;
    module_deleted.store(true);
    delete module_ns;
    module_ns = nullptr;
}

extern "C" DLLEXPORT void tdpgmowner_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "tdpgmowner";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module that owns Programs, which it deletes when it is deleted";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = tdpgmowner_module_init;
    mod_info.ns_init = tdpgmowner_module_ns_init;
    mod_info.del = tdpgmowner_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
