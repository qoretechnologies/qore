/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreParseListNode.h

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

#ifndef _QORE_QOREPARSELISTNODE_H

#define _QORE_QOREPARSELISTNODE_H

#include <qore/Qore.h>

#include <deque>
#include <string>

DLLLOCAL QoreValue copy_value_and_resolve_lvar_refs(const QoreValue& n, ExceptionSink* xsink);

class QoreParseListNode : public ParseNode {
public:
    typedef std::deque<QoreValue> nvec_t;

    DLLLOCAL QoreParseListNode(const QoreProgramLocation* loc) : ParseNode(loc, NT_PARSE_LIST, true) {
    }

    DLLLOCAL QoreParseListNode(const QoreParseListNode& old, ExceptionSink* xsink) : ParseNode(old),
            vtype(old.vtype), typeInfo(old.typeInfo), finalized(old.finalized), vlist(old.vlist) {
        for (unsigned i = 0; i < old.values.size(); ++i) {
            add(copy_value_and_resolve_lvar_refs(old.values[i], xsink), old.lvec[i]);
            if (*xsink)
                return;
        }
    }

    DLLLOCAL ~QoreParseListNode() {
        for (auto& i : values)
            i.discard(nullptr);
    }

    DLLLOCAL void add(QoreValue v, const QoreProgramLocation* loc) {
        values.push_back(v);
        lvec.push_back(loc);

        if (!size()) {
            if (v.hasNode() && v.hasEffect()) {
                set_effect(true);
            }
        } else if (has_effect() && !v.hasEffect()) {
            set_effect(false);
        }
    }

    DLLLOCAL QoreValue shift() {
        if (!values.size()) {
            return QoreValue();
        }
        assert(vtypes.empty());
        QoreValue rv = values[0];
        values.pop_front();
        lvec.pop_front();
        return rv;
    }

    DLLLOCAL QoreValue pop() {
        if (!values.size()) {
            return QoreValue();
        }
        assert(vtypes.empty());
        QoreValue rv = values.back();
        values.pop_back();
        lvec.pop_back();
        return rv;
    }

    DLLLOCAL QoreValue get(size_t i) {
        assert(i < values.size());
        return values[i];
    }

    DLLLOCAL const QoreValue get(size_t i) const {
        assert(i < values.size());
        return values[i];
    }

    DLLLOCAL QoreValue& getReference(size_t i) {
        assert(i < values.size());
        return values[i];
    }

    DLLLOCAL const QoreProgramLocation* getLocation(size_t i) const {
        assert(i < lvec.size());
        return lvec[i];
    }

    DLLLOCAL QoreValue swap(size_t i, QoreValue v) {
        QoreValue rv = values[i];
        values[i] = v;
        return rv;
    }

    DLLLOCAL size_t size() const {
        return values.size();
    }

    DLLLOCAL bool empty() const {
        return values.empty();
    }

    DLLLOCAL void setFinalized() {
        if (!finalized)
            finalized = true;
    }

    DLLLOCAL bool isFinalized() const {
        return finalized;
    }

    DLLLOCAL bool isVariableList() const {
        return vlist;
    }

    DLLLOCAL void setVariableList() {
        assert(!vlist);
        vlist = true;
    }

    DLLLOCAL void finalizeBlock(int start_line, int end_line);

    DLLLOCAL nvec_t& getValues() {
        return values;
    }

    DLLLOCAL const nvec_t& getValues() const {
        return values;
    }

    DLLLOCAL const type_vec_t& getValueTypes() const {
        return vtypes;
    }

    DLLLOCAL QoreParseListNode* listRefSelf() const {
        ref();
        return const_cast<QoreParseListNode*>(this);
    }

    DLLLOCAL int initArgs(QoreParseContext& parse_context, type_vec_t& arg_types, QoreListNode*& args);

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual const char* getTypeName() const {
        return "list";
    }

    DLLLOCAL QoreParseListNode* copyBlank() const {
        QoreParseListNode* n = new QoreParseListNode(loc);
        n->vtype = vtype;

        n->finalized = finalized;
        n->vlist = vlist;
        return n;
    }

    DLLLOCAL int parseInitIntern(bool& needs_eval, QoreParseContext& parse_context);

protected:
    typedef std::deque<const QoreProgramLocation*> lvec_t;
    nvec_t values;
    type_vec_t vtypes;
    lvec_t lvec;
    // common value type, if any
    const QoreTypeInfo* vtype = nullptr;
    // node type info (derivative of list)
    const QoreTypeInfo* typeInfo = nullptr;
    // flag for a list expression in curly brackets for the list version of the map operator
    bool finalized = false;
    // is this a variable list declaration?
    bool vlist = false;

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
};

#endif
