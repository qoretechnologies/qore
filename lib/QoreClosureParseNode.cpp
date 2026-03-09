/*
    QoreClosureParseNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
    return new QoreClosureNode(this, nullptr, runtime_get_class());
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
    cvv_vec_t* cvv = thread_get_all_closure_vars();

    if (in_method) {
        QoreObject* o;
        const qore_class_private* c_ctx;
        runtime_get_object_and_class(o, c_ctx);
        return new QoreObjectClosureNode(o, c_ctx, this, cvv);
    }

    return new QoreClosureNode(this, cvv, runtime_get_class());
}
