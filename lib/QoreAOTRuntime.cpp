/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTRuntime.cpp

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

#include "qore/intern/QoreAOT.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"

#include <cstring>
#include <string>
#include <unordered_map>

//! Walk a namespace tree and register pre-compiled AOT function pointers
/** Matches function names from the AOT function table against user function variants
    in the program's namespace tree. For each match, sets the cached_jit_fn and promotes
    the variant to TIER_JIT.
*/
static int registerAOTFunctionsInNamespace(qore_ns_private* ns,
        const std::unordered_map<std::string, uint64_t (*)(ExceptionSink*)>& func_map,
        int& registered) {
    // Walk functions in this namespace
    for (auto i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        // Check all variants
        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!uvb || !uvb->hasBody()) {
                continue;
            }

            // Build the name that attemptIRLowering would use
            const char* fname = func->getName();
            auto it = func_map.find(fname);
            if (it != func_map.end()) {
                uvb->registerPrecompiledFunction(it->second);
                ++registered;
                printd(2, "AOT: registered pre-compiled function '%s'\n", fname);
            }
        }
    }

    // Walk classes in this namespace
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);
        const char* class_name = qc->getName();

        // Walk instance methods
        for (auto mi = qcp->hm.begin(), me = qcp->hm.end(); mi != me; ++mi) {
            QoreMethod* meth = mi->second;
            if (!meth->isUser()) {
                continue;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }

                // Method name format: "ClassName::methodName"
                std::string method_name = std::string(class_name) + "::" + meth->getName();
                auto it = func_map.find(method_name);
                if (it != func_map.end()) {
                    uvb->registerPrecompiledFunction(it->second);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled method '%s'\n", method_name.c_str());
                }
            }
        }

        // Walk static methods
        for (auto mi = qcp->shm.begin(), me = qcp->shm.end(); mi != me; ++mi) {
            QoreMethod* meth = mi->second;
            if (!meth->isUser()) {
                continue;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }

                std::string method_name = std::string(class_name) + "::" + meth->getName();
                auto it = func_map.find(method_name);
                if (it != func_map.end()) {
                    uvb->registerPrecompiledFunction(it->second);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled static method '%s'\n", method_name.c_str());
                }
            }
        }
    }

    // Walk child namespaces recursively
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            registerAOTFunctionsInNamespace(qore_ns_private::get(*child_ns), func_map, registered);
        }
    }

    return 0;
}

extern "C" int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
) {
    // Check for -b flag (disable signal handling, useful for valgrind)
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
            break;
        }
    }

    // Set up ARGV from command-line arguments before qore_init (matches normal qore binary order)
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime with MIT license
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    {
        ExceptionSink xsink;
        ExceptionSink wsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            qore_cleanup();
            return 2;
        }

        // Set JIT execution mode so functions without pre-compiled code will JIT on demand
        qpgm->setExecMode(QEM_JIT);

        // Parse the embedded source
        QoreString src_str(source, source_len);
        qpgm->parse(src_str.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);

        // Display any warnings
        if (wsink.isException()) {
            wsink.handleWarnings();
        }

        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
        } else {
            // Register pre-compiled function pointers
            if (num_functions > 0 && functions) {
                // Build lookup map
                std::unordered_map<std::string, uint64_t (*)(ExceptionSink*)> func_map;
                for (int i = 0; i < num_functions; ++i) {
                    if (functions[i].name && functions[i].fn_ptr) {
                        func_map[functions[i].name] = functions[i].fn_ptr;
                    }
                }

                // Walk namespace tree and register
                qore_program_private* pp = qore_program_private::get(**qpgm);
                int registered = 0;
                qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
                registerAOTFunctionsInNamespace(root_ns, func_map, registered);

                // Also try to register the _toplevel function for pre-compiled top-level code
                auto toplevel_it = func_map.find("_toplevel");
                if (toplevel_it != func_map.end()) {
                    pp->sb.registerPrecompiledTopLevel(toplevel_it->second);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled _toplevel function\n");
                }

                printd(1, "AOT: registered %d/%d pre-compiled functions\n", registered, num_functions);
            }

            // Run the program
            QoreValue rv = qpgm->run(&xsink);
            rc = rv.getAsBigInt();
            rv.discard(&xsink);

            if (xsink.isException()) {
                rc = 3;
            }
        }

        xsink.handleExceptions();
    }

    qore_cleanup();
    return rc;
}
