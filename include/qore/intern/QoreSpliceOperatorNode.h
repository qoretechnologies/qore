/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSpliceOperatorNode.h

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

#ifndef _QORE_QORESPLICEOPERATORNODE_H

#define _QORE_QORESPLICEOPERATORNODE_H

class QoreSpliceOperatorNode : public LValueOperatorNode {
public:
    DLLLOCAL QoreSpliceOperatorNode(const QoreProgramLocation* loc,
                                    QoreValue n_lvalue_exp, QoreValue n_offset_exp,
                                    QoreValue n_length_exp, QoreValue n_new_exp) :
        LValueOperatorNode(loc),
        lvalue_exp(n_lvalue_exp),
        offset_exp(n_offset_exp),
        length_exp(n_length_exp),
        new_exp(n_new_exp) {
    }

    DLLLOCAL virtual QoreString *getAsString(bool &del, int foff, ExceptionSink *xsink) const;

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink *xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char *getTypeName() const {
        return splice_str.getBuffer();
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        ValueHolder n_lv(copy_value_and_resolve_lvar_refs(lvalue_exp, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ValueHolder n_of(copy_value_and_resolve_lvar_refs(offset_exp, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ValueHolder n_ln(copy_value_and_resolve_lvar_refs(length_exp, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ValueHolder n_nw(copy_value_and_resolve_lvar_refs(new_exp, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        return new QoreSpliceOperatorNode(get_runtime_location(), n_lv.release(), n_of.release(), n_ln.release(),
            n_nw.release());
    }

protected:
    QoreValue lvalue_exp, offset_exp, length_exp, new_exp;
    const QoreTypeInfo* returnTypeInfo = nullptr;

    DLLLOCAL static QoreString splice_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL ~QoreSpliceOperatorNode() {
        lvalue_exp.discard(nullptr);
        offset_exp.discard(nullptr);
        length_exp.discard(nullptr);
        new_exp.discard(nullptr);
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo *getTypeInfo() const {
        return returnTypeInfo;
    }
};

#endif
