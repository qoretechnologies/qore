/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    host.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    A C++ host for AOTStaticModuleShadowing.qtest: the host or the code of its Programs registers the modules that are
    statically linked into it, or loads the .qmod files of the same modules by path, and the host reports what
    happened.

    usage: host <static|file> <scenario> <qmod dir> <library module dir>

    Each step prints "<step>: loaded", "<step>: refused" or "<step>: value=<v>"; exceptions are printed to stderr.

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
#include <qore/QoreSandboxManager.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

// the register functions and the descriptor functions that qcc emits for the linked .qo modules
extern "C" void qore_ShadowLib_register(QoreProgram*);
extern "C" void qore_TenantMod_register(QoreProgram*);
extern "C" void qore_ProbeMod_register(QoreProgram*);
extern "C" void qore_LiftMod_register(QoreProgram*);
extern "C" void ProbeMod_qore_module_desc(QoreModuleInfo&);

namespace {
//! true to register the statically linked modules, false to load their .qmod files
bool use_static = false;
//! the directory of the .qmod files
std::string qmod_dir;
//! the directory of the library modules (in the module path)
std::string lib_dir;
//! the Program that reg() registers into when its second argument is true
QoreProgram* other_target = nullptr;
//! the Program that the library module's initialization registers LiftMod into (see liftlib-module.cpp)
QoreProgram* lift_target = nullptr;

//! returns true if the Program has the module's feature, binary or user
bool has_feature(QoreProgram* pgm, const char* name) {
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> features(pgm->getFeatureList(), &xsink);
    if (!features) {
        return false;
    }
    ConstListIterator i(*features);
    while (i.next()) {
        const QoreValue v = i.getValue();
        if (v.getType() == NT_STRING && !strcmp(v.get<const QoreStringNode>()->c_str(), name)) {
            return true;
        }
    }
    return false;
}

//! registers the statically linked module (with its register function) or loads its .qmod by path
/** returns true if the Program has the module then; the error is printed, as the register function prints it
*/
bool register_module(const char* name, QoreProgram* pgm) {
    assert(pgm);
    if (use_static) {
        if (!strcmp(name, "ShadowLib")) {
            qore_ShadowLib_register(pgm);
        } else if (!strcmp(name, "TenantMod")) {
            qore_TenantMod_register(pgm);
        } else if (!strcmp(name, "ProbeMod")) {
            qore_ProbeMod_register(pgm);
        } else {
            assert(!strcmp(name, "LiftMod"));
            qore_LiftMod_register(pgm);
        }
    } else {
        ExceptionSink lsink;
        std::string path = qmod_dir + "/" + name + ".qmod";
        ModuleManager::runTimeLoadModule(&lsink, path.c_str(), pgm);
        lsink.handleExceptions();
    }
    return has_feature(pgm, name);
}

//! reg(string name, bool other): registers the module for the calling code
/** into the calling Program, or into other_target
*/
QoreValue f_reg(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    const char* name = args->retrieveEntry(0).get<const QoreStringNode>()->c_str();
    return register_module(name, args->retrieveEntry(1).getAsBool() ? other_target : getProgram());
}

//! api_reg(): registers ProbeMod with ModuleManager::registerAOTStaticModule() into the calling Program
/** with a path label in the directory of the library modules; returns true if the Program has the module then
*/
QoreValue f_api_reg(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    assert(pgm);
    ExceptionSink lsink;
    std::string path = lib_dir + "/ProbeMod.qmod";
    ModuleManager::registerAOTStaticModule(&lsink, pgm, ProbeMod_qore_module_desc, path.c_str());
    lsink.handleExceptions();
    return has_feature(pgm, "ProbeMod");
}

const char* Code = "bool sub load(string n, bool other) { return reg(n, other); }\n"
    "bool sub api_load() { return api_reg(); }\n"
    "nothing sub lift() { load_module(\"LiftLib\"); }\n"
    "string sub use(string n) { return call_function(n + \"::value\"); }\n";

//! releases a Program
void release(QoreProgram* pgm) {
    ExceptionSink xsink;
    pgm->waitForTerminationAndDeref(&xsink);
    xsink.handleExceptions();
}

//! creates a Program with the functions of Code, and with the given code before them
/** @param sm the sandbox of the Program, if any
*/
QoreProgram* make_program(const QoreParseOptions& po, QoreSandboxManager* sm = nullptr,
        const std::string& prefix = std::string()) {
    QoreProgram* pgm = new QoreProgram(po);
    if (sm) {
        pgm->setSandboxManager(sm);
    }
    // each parameter is given by its type, its default value and its name
    pgm->getRootNS()->addBuiltinVariant("reg", f_reg, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo, 2, stringTypeInfo,
        QORE_PARAM_NO_ARG, "name", boolTypeInfo, QORE_PARAM_NO_ARG, "other");
    pgm->getRootNS()->addBuiltinVariant("api_reg", f_api_reg, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo, 0);
    ExceptionSink xsink;
    pgm->parse((prefix + Code).c_str(), "host", &xsink);
    if (xsink) {
        xsink.handleExceptions();
        release(pgm);
        return nullptr;
    }
    return pgm;
}

//! prints the result of a step
void report(const char* step, const char* result) {
    printf("%s: %s\n", step, result);
    fflush(stdout);
}

//! calls a function of the Code of a Program; returns its value, or nothing if it raised an exception
QoreValue call(QoreProgram* pgm, const char* func, const char* name, ExceptionSink& xsink, bool other = false) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    if (name) {
        args->push(new QoreStringNode(name), &xsink);
        if (!strcmp(func, "load")) {
            args->push(other, &xsink);
        }
    }
    return pgm->callFunction(func, *args, &xsink);
}

//! the code of the Program loads the module
bool load(const char* step, const char* name, QoreProgram* pgm, bool other = false) {
    ExceptionSink xsink;
    ValueHolder v(call(pgm, "load", name, xsink, other), &xsink);
    const bool ok = !xsink && v->getAsBool();
    xsink.handleExceptions();
    report(step, ok ? "loaded" : "refused");
    return ok;
}

//! the code of the Program registers ProbeMod with the C++ API
bool api_load(const char* step, QoreProgram* pgm) {
    ExceptionSink xsink;
    ValueHolder v(call(pgm, "api_load", nullptr, xsink), &xsink);
    const bool ok = !xsink && v->getAsBool();
    xsink.handleExceptions();
    report(step, ok ? "loaded" : "refused");
    return ok;
}

//! the code of the Program loads the library module, whose initialization registers LiftMod into the Program
bool lift(const char* step, QoreProgram* pgm) {
    lift_target = pgm;
    ExceptionSink xsink;
    ValueHolder v(call(pgm, "lift", nullptr, xsink), &xsink);
    lift_target = nullptr;
    const bool ok = !xsink && has_feature(pgm, "LiftMod");
    xsink.handleExceptions();
    report(step, ok ? "loaded" : "refused");
    return ok;
}

//! the host registers the module into the Program, with no code of a Program running
bool direct(const char* step, const char* name, QoreProgram* pgm) {
    const bool ok = register_module(name, pgm);
    report(step, ok ? "loaded" : "refused");
    return ok;
}

//! the code of the Program calls the module
void use(const char* step, const char* name, QoreProgram* pgm) {
    ExceptionSink xsink;
    ValueHolder v(call(pgm, "use", name, xsink), &xsink);
    if (!xsink && v->getType() == NT_STRING) {
        std::string result = std::string("value=") + v->get<const QoreStringNode>()->c_str();
        report(step, result.c_str());
        return;
    }
    xsink.handleExceptions();
    report(step, "refused");
}

//! the host, which is trusted, requires the module by name in a new Program, whose code then calls it
void require(const char* step, const char* name) {
    QoreProgram* pgm = make_program(QoreParseOptions(PO_NEW_STYLE), nullptr,
        std::string("%requires ") + name + "\n");
    if (!pgm) {
        report(step, "refused");
        return;
    }
    use(step, name, pgm);
    release(pgm);
}

//! creates a sandbox that restricts nothing that the scenarios do
QoreSandboxManager* make_sandbox() {
    QoreSandboxManager* sm = new QoreSandboxManager;
    sm->filesystem().setDefaultPolicy(true);
    sm->network().setDefaultPolicy(true);
    return sm;
}
}

//! called by the initialization of the library module LiftLib (see liftlib-module.cpp)
extern "C" __attribute__((visibility("default"))) void aot_static_shadowing_lift_hook(ExceptionSink* xsink) {
    if (!lift_target) {
        xsink->raiseException("HOST-ERROR", "no Program to register LiftMod into");
        return;
    }
    register_module("LiftMod", lift_target);
}

int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <static|file> <scenario> <qmod dir> <library module dir>\n", argv[0]);
        return 2;
    }
    use_static = !strcmp(argv[1], "static");
    const std::string scenario = argv[2];
    qmod_dir = argv[3];
    lib_dir = argv[4];
    qore_init(QL_GPL, "UTF-8", true);
    int rc = 0;
    {
        QoreSandboxManager* sm1 = make_sandbox();
        QoreSandboxManager* sm2 = make_sandbox();
        // the code of a Program whose parse options restrict a functional domain is not trusted
        QoreProgram* restricted = make_program(QoreParseOptions(PO_NEW_STYLE) | PO_NO_NETWORK);
        QoreProgram* trusted = make_program(QoreParseOptions(PO_NEW_STYLE));
        QoreProgram* other = make_program(QoreParseOptions(PO_NEW_STYLE));
        // the code of a Program with a sandbox is not trusted either; two Programs with the same sandbox, and one
        // with another
        QoreProgram* sb1 = make_program(QoreParseOptions(PO_NEW_STYLE), sm1);
        QoreProgram* sb1b = make_program(QoreParseOptions(PO_NEW_STYLE), sm1);
        QoreProgram* sb2 = make_program(QoreParseOptions(PO_NEW_STYLE), sm2);
        sm1->deref(nullptr);
        sm2->deref(nullptr);
        other_target = other;
        if (!restricted || !trusted || !other || !sb1 || !sb1b || !sb2) {
            fprintf(stderr, "cannot create the Programs\n");
            rc = 1;
        } else if (scenario == "shadow-restricted") {
            if (load("register", "ShadowLib", restricted)) {
                use("use", "ShadowLib", restricted);
            }
            // the library module is still the one that other code gets
            require("library", "ShadowLib");
        } else if (scenario == "shadow-restricted-other") {
            // the restricted code registers the module into a Program that it was handed
            if (load("register", "ShadowLib", restricted, true)) {
                use("use", "ShadowLib", other);
            }
            require("library", "ShadowLib");
        } else if (scenario == "shadow-sandboxed") {
            if (load("register", "ShadowLib", sb1)) {
                use("use", "ShadowLib", sb1);
            }
            require("library", "ShadowLib");
        } else if (scenario == "shadow-library-loaded") {
            // the library module is loaded first: the restricted code gets it, as a load of the feature does
            require("library", "ShadowLib");
            if (load("register", "ShadowLib", restricted)) {
                use("use", "ShadowLib", restricted);
            }
        } else if (scenario == "shadow-trusted") {
            if (load("register", "ShadowLib", trusted)) {
                use("use", "ShadowLib", trusted);
            }
        } else if (scenario == "own-restricted") {
            if (load("register", "TenantMod", restricted)) {
                use("use", "TenantMod", restricted);
            }
            // the module that the restricted code chose is not given to other code
            load("other-register", "TenantMod", trusted);
            require("other-require", "TenantMod");
        } else if (scenario == "own-sandboxed") {
            if (load("register", "TenantMod", sb1)) {
                use("use", "TenantMod", sb1);
            }
            // code with the same sandbox gets the module, code with another sandbox and other code do not
            if (load("same-sandbox-register", "TenantMod", sb1b)) {
                use("same-sandbox-use", "TenantMod", sb1b);
            }
            load("other-sandbox-register", "TenantMod", sb2);
            load("other-register", "TenantMod", trusted);
            require("other-require", "TenantMod");
        } else if (scenario == "own-trusted") {
            if (load("register", "TenantMod", trusted)) {
                use("use", "TenantMod", trusted);
            }
            // a module that trusted code chose is given to any code
            load("other-register", "TenantMod", restricted);
            use("other-use", "TenantMod", restricted);
            require("other-require", "TenantMod");
        } else if (scenario == "direct-shadow-sandboxed") {
            // the host registers the module into a Program with a sandbox, with no code of a Program running
            if (direct("register", "ShadowLib", sb1)) {
                use("use", "ShadowLib", sb1);
            }
            require("library", "ShadowLib");
        } else if (scenario == "direct-own-sandboxed") {
            if (direct("register", "TenantMod", sb1)) {
                use("use", "TenantMod", sb1);
            }
            if (load("same-sandbox-register", "TenantMod", sb1b)) {
                use("same-sandbox-use", "TenantMod", sb1b);
            }
            load("other-sandbox-register", "TenantMod", sb2);
            require("other-require", "TenantMod");
        } else if (scenario == "direct-own-restricted") {
            // a Program whose parse options restrict a functional domain, but no sandbox: the host is the caller
            if (direct("register", "TenantMod", restricted)) {
                use("use", "TenantMod", restricted);
            }
            require("other-require", "TenantMod");
        } else if (scenario == "api-path-sandboxed") {
            // registered with the C++ API with a path label in the module path: not library code
            if (api_load("register", sb1)) {
                use("use", "ProbeMod", sb1);
            }
            load("other-sandbox-register", "ProbeMod", sb2);
            require("other-require", "ProbeMod");
        } else if (scenario == "lift-sandboxed") {
            // the sandboxed code chooses TenantMod, which LiftMod requires
            load("dep-register", "TenantMod", sb1);
            // LiftMod is registered while the library module that the sandboxed code loaded is initialized: it is
            // governed by the sandbox of that code, so it gets the module that the code chose
            if (lift("register", sb1)) {
                use("use", "LiftMod", sb1);
            }
            load("other-sandbox-register", "LiftMod", sb2);
        } else {
            fprintf(stderr, "unknown scenario '%s'\n", scenario.c_str());
            rc = 2;
        }
        for (QoreProgram* pgm : {restricted, trusted, other, sb1, sb1b, sb2}) {
            if (pgm) {
                release(pgm);
            }
        }
    }
    qore_cleanup();
    return rc;
}
