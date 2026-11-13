/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    host.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    A C++ host for AOTStaticModuleShadowing.qtest: the code of its Programs registers the modules that are statically
    linked into it, or loads the .qmod files of the same modules by path, and the host reports what happened.

    usage: host <static|file> <scenario> <qmod dir>

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

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" void qore_ShadowLib_register(QoreProgram*);
extern "C" void qore_TenantMod_register(QoreProgram*);

namespace {
//! true to register the statically linked modules, false to load their .qmod files
bool use_static = false;
//! the directory of the .qmod files
std::string qmod_dir;
//! the Program that reg() registers into when its second argument is true
QoreProgram* other_target = nullptr;

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

//! reg(string name, bool other): registers the statically linked module or loads its .qmod by path
/** into the calling Program, or into other_target; returns true if that Program has the module then.  Called by
    the code of a Program, so the registration is made for that code.
*/
QoreValue f_reg(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    const char* name = args->retrieveEntry(0).get<const QoreStringNode>()->c_str();
    QoreProgram* pgm = args->retrieveEntry(1).getAsBool() ? other_target : getProgram();
    assert(pgm);
    if (use_static) {
        // the register function reports an error itself
        if (!strcmp(name, "ShadowLib")) {
            qore_ShadowLib_register(pgm);
        } else {
            qore_TenantMod_register(pgm);
        }
    } else {
        // the error is reported as the register function reports it
        ExceptionSink lsink;
        std::string path = qmod_dir + "/" + name + ".qmod";
        ModuleManager::runTimeLoadModule(&lsink, path.c_str(), pgm);
        lsink.handleExceptions();
    }
    return has_feature(pgm, name);
}

//! releases a Program
void release(QoreProgram* pgm) {
    ExceptionSink xsink;
    pgm->waitForTerminationAndDeref(&xsink);
    xsink.handleExceptions();
}

const char* Code = "bool sub load(string n, bool other) { return reg(n, other); }\n"
    "string sub use(string n) { return call_function(n + \"::value\"); }\n";

//! creates a Program with reg(), load() and use(), and with the given code before them
QoreProgram* make_program(const QoreParseOptions& po, const std::string& prefix = std::string()) {
    QoreProgram* pgm = new QoreProgram(po);
    // each parameter is given by its type, its default value and its name
    pgm->getRootNS()->addBuiltinVariant("reg", f_reg, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo, 2, stringTypeInfo,
        QORE_PARAM_NO_ARG, "name", boolTypeInfo, QORE_PARAM_NO_ARG, "other");
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

//! the code of the Program loads the module
bool load(const char* step, const char* name, QoreProgram* pgm, bool other = false) {
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(new QoreStringNode(name), &xsink);
    args->push(other, &xsink);
    ValueHolder v(pgm->callFunction("load", *args, &xsink), &xsink);
    const bool ok = !xsink && v->getAsBool();
    xsink.handleExceptions();
    report(step, ok ? "loaded" : "refused");
    return ok;
}

//! the code of the Program calls the module
void use(const char* step, const char* name, QoreProgram* pgm) {
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(new QoreStringNode(name), &xsink);
    ValueHolder v(pgm->callFunction("use", *args, &xsink), &xsink);
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
    QoreProgram* pgm = make_program(QoreParseOptions(PO_NEW_STYLE), std::string("%requires ") + name + "\n");
    if (!pgm) {
        report(step, "refused");
        return;
    }
    use(step, name, pgm);
    release(pgm);
}
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <static|file> <scenario> <qmod dir>\n", argv[0]);
        return 2;
    }
    use_static = !strcmp(argv[1], "static");
    const std::string scenario = argv[2];
    qmod_dir = argv[3];
    qore_init(QL_GPL, "UTF-8", true);
    int rc = 0;
    {
        // the code of a Program whose parse options restrict a functional domain is not trusted
        QoreProgram* restricted = make_program(QoreParseOptions(PO_NEW_STYLE) | PO_NO_NETWORK);
        QoreProgram* trusted = make_program(QoreParseOptions(PO_NEW_STYLE));
        QoreProgram* other = make_program(QoreParseOptions(PO_NEW_STYLE));
        other_target = other;
        if (!restricted || !trusted || !other) {
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
        } else if (scenario == "own-trusted") {
            if (load("register", "TenantMod", trusted)) {
                use("use", "TenantMod", trusted);
            }
            // a module that trusted code chose is given to any code
            load("other-register", "TenantMod", restricted);
            use("other-use", "TenantMod", restricted);
            require("other-require", "TenantMod");
        } else {
            fprintf(stderr, "unknown scenario '%s'\n", scenario.c_str());
            rc = 2;
        }
        for (QoreProgram* pgm : {restricted, trusted, other}) {
            if (pgm) {
                release(pgm);
            }
        }
    }
    qore_cleanup();
    return rc;
}
