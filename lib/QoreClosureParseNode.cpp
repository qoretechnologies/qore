/*
    QoreClosureParseNode.cpp

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
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"

QoreClosureParseNode::QoreClosureParseNode(const QoreProgramLocation* loc, UserClosureFunction* n_uf, bool n_lambda)
        : ParseNode(loc, NT_CLOSURE), uf(n_uf), lambda(n_lambda), in_method(false) {
    set_effect_as_root(false);
}

QoreClosureParseNode::~QoreClosureParseNode() {
    if (is_deferred) {
        QoreProgram* pgm = getProgram();
        if (pgm) {
            qore_program_private::get(*pgm)->removeDeferredCode(this);
        }
    }
    delete uf;
}

QoreClosureNode* QoreClosureParseNode::evalClosure() const {
    // Before creating the closure, ensure all variables it needs are on cvstack.
    // Capture variables currently on cvstack (like evalBackground does).
    fprintf(stderr, "[DEBUG-CLOSURE] evalClosure called, capturing all cvstack vars\n");
    cvv_vec_t* cvv = thread_get_all_closure_vars();
    fprintf(stderr, "[DEBUG-CLOSURE] captured %zu vars from cvstack (cvv=%p)\n", cvv ? cvv->size() : 0, cvv);

    // Additionally, for variables in the closure's vlist that are NOT on cvstack,
    // find them on lvstack and instantiate them on cvstack. Also ensure all closure-used
    // variables are in the cvv list passed to the constructor.
    const LVarSet* vlist = getVList();
    if (vlist) {
        fprintf(stderr, "[DEBUG-CLOSURE] closure vlist size: %zu\n", vlist->size());
        for (const LocalVar* lv : *vlist) {
            if (lv) {
                ClosureVarValue* existing_cvv = thread_find_closure_var(lv->getName());
                fprintf(stderr, "[DEBUG-CLOSURE]   var '%s': on_cvstack=%d (cvv=%p)\n", lv->getName(), existing_cvv != nullptr, existing_cvv);

                if (existing_cvv) {
                    fprintf(stderr, "[DEBUG-CLOSURE]   in existing_cvv branch\n");
                    // Variable is on cvstack - ensure it's in the captured list
                    if (!cvv) {
                        // If cvv was null, create it now
                        cvv = new cvv_vec_t;
                        fprintf(stderr, "[DEBUG-CLOSURE]   created new cvv list\n");
                    }
                    // Check if variable is already in cvv list
                    fprintf(stderr, "[DEBUG-CLOSURE]   checking if '%s' is in cvv list (%zu items)\n", lv->getName(), cvv->size());
                    bool found = false;
                    for (const auto& v : *cvv) {
                        if (v == existing_cvv) {
                            found = true;
                            fprintf(stderr, "[DEBUG-CLOSURE]   found in cvv at index\n");
                            break;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "[DEBUG-CLOSURE]   adding '%s' to cvv list\n", lv->getName());
                        cvv->push_back(existing_cvv);
                    } else {
                        fprintf(stderr, "[DEBUG-CLOSURE]   '%s' already in cvv list\n", lv->getName());
                    }
                } else {
                    // Variable not on cvstack - check if it's on lvstack and instantiate on cvstack
                    bool needs_deref = false;
                    QoreValue val = lv->eval(needs_deref, nullptr);
                    fprintf(stderr, "[DEBUG-CLOSURE]   val type: %s\n", val.getTypeName());
                    if (val) {
                        // Instantiate on cvstack with the current lvstack value
                        fprintf(stderr, "[DEBUG-CLOSURE]   instantiating on cvstack...\n");
                        thread_instantiate_closure_var(lv->getName(), lv->getTypeInfo(), val, true);
                        // Add to captured variables list
                        if (!cvv) {
                            cvv = new cvv_vec_t;
                            fprintf(stderr, "[DEBUG-CLOSURE]   created new cvv list\n");
                        }
                        ClosureVarValue* cvv_entry = thread_find_closure_var(lv->getName());
                        if (cvv_entry) {
                            fprintf(stderr, "[DEBUG-CLOSURE]   added '%s' to cvv list\n", lv->getName());
                            cvv->push_back(cvv_entry);
                        }
                        // Clean up if needed
                        if (needs_deref) {
                            val.discard(nullptr);
                        }
                    } else {
                        fprintf(stderr, "[DEBUG-CLOSURE]   val is null\n");
                    }
                }
            }
        }
    } else {
        fprintf(stderr, "[DEBUG-CLOSURE] vlist is null\n");
    }

    return new QoreClosureNode(this, cvv, runtime_get_class());
}

QoreObjectClosureNode* QoreClosureParseNode::evalObjectClosure() const {
    QoreObject* o;
    const qore_class_private* c_ctx;
    runtime_get_object_and_class(o, c_ctx);
    return new QoreObjectClosureNode(o, c_ctx, this);
}

QoreValue QoreClosureParseNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return in_method ? (AbstractQoreNode*)evalObjectClosure() : (AbstractQoreNode*)evalClosure();
}

int QoreClosureParseNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("parsed closure (%slambda, %p)", lambda ? "" : "non-", this);
    return 0;
}

QoreString* QoreClosureParseNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

int QoreClosureParseNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (parse_context.oflag) {
        in_method = true;
        uf->setClassType(parse_context.oflag->getTypeInfo());
    }
    int err = 0;
    if (parse_context.pflag & PF_CONST_EXPRESSION) {
        qore_program_private::get(*parse_context.pgm)->deferCodeInitialization(this);
        is_deferred = true;

        parse_qc = parse_get_class_priv();
        parse_ns = parse_get_ns();
        //printd(5, "QoreClosureParseNode::parseInitImpl() this: %p set deferred\n", this);
        // Use generic closure type for deferred initialization
        parse_context.typeInfo = runTimeClosureTypeInfo;
    } else {
        err = uf->parseInit(nullptr);
        uf->parseCommit();

        // After parseCommit, the signature is available - create a typed callable type
        AbstractFunctionSignature* sig = uf->getUniqueSignature();
        if (sig) {
            parse_context.typeInfo = qore_get_complex_code_type_from_signature(sig);
        } else {
            parse_context.typeInfo = runTimeClosureTypeInfo;
        }
    }
    return err;
}

int QoreClosureParseNode::parseInitDeferred() {
    assert(is_deferred);
    assert(uf);
    is_deferred = false;
    int rc;
    {
        const qore_class_private* old_qc;
        qore_ns_private* old_ns;
        thread_set_class_and_ns(parse_qc, parse_ns, old_qc, old_ns);
        rc = uf->parseInit(nullptr);
        thread_set_class_and_ns(old_qc, old_ns);
    }
    uf->parseCommit();
    //printd(5, "QoreClosureParseNode::parseInitDeferred() this: %p\n", this);
    return rc;
}

const char* QoreClosureParseNode::getTypeName() const {
    return getStaticTypeName();
}

QoreValue QoreClosureParseNode::exec(const QoreClosureBase& closure_base, QoreProgram* pgm, const QoreListNode* args,
        QoreObject* self, const qore_class_private* class_ctx, ExceptionSink* xsink) const {
    assert(!is_deferred);
    assert(uf);
    return uf->evalClosure(closure_base, pgm, args, self, class_ctx, xsink);
}

QoreClosureBase* QoreClosureParseNode::evalBackground(ExceptionSink* xsink) const {
    // Always use thread_get_all_closure_vars() to properly share/sync closure variables
    // with background threads. The optimization using thread_get_closure_vars_for_vlist()
    // breaks variable capture by not properly transferring current values to the new thread.
    // Closure variables are designed to be thread-safe and shared, so we need the proven
    // approach that ensures the background thread sees current values.
    cvv_vec_t* cvv = thread_get_all_closure_vars();

    if (in_method) {
        QoreObject* o;
        const qore_class_private* c_ctx;
        runtime_get_object_and_class(o, c_ctx);
        return new QoreObjectClosureNode(o, c_ctx, this, cvv);
    }

    return new QoreClosureNode(this, cvv, runtime_get_class());
}
