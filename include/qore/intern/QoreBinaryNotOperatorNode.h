/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBinaryNotOperatorNode.h

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

#ifndef _QORE_QOREBINARYNOTOPERATORNODE_H

#define _QORE_QOREBINARYNOTOPERATORNODE_H

class QoreBinaryNotOperatorNode : public QoreSingleExpressionOperatorNode<QoreOperatorNode> {
protected:
    DLLLOCAL static QoreString BinaryNot_str;

    //! true if the operator resolved a plugin operation at parse time, whose result type is plugin_type_info
    bool plugin_operation = false;
    //! the result type of the plugin operation that the operator resolved at parse time
    const QoreTypeInfo* plugin_type_info = nullptr;

    //! Sets the result type of the plugin operation that the operator resolved at parse time
    DLLLOCAL void setPluginTypeInfo(const QoreTypeInfo* type_info) {
        plugin_operation = true;
        plugin_type_info = type_info;
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

public:
    DLLLOCAL QoreBinaryNotOperatorNode(const QoreProgramLocation* loc, QoreValue exp)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return BinaryNot_str.getBuffer();
    }

    //! Returns the result type: that of the plugin operation that the operator resolved at parse time, if any, or int
    /** The IR lowering selects the operations that use the operator's value by the operator's type (qore#6141)
    */
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return plugin_operation ? plugin_type_info : bigIntTypeInfo;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        QoreBinaryNotOperatorNode* rv = copyBackgroundExplicit<QoreBinaryNotOperatorNode>(xsink);
        if (rv) {
            rv->plugin_operation = plugin_operation;
            rv->plugin_type_info = plugin_type_info;
        }
        return rv;
    }
};

#endif
