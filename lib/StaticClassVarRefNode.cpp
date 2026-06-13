/*
    StaticClassVarRefNode.cpp

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
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_program_private.h"
#include <qore/ReferenceNode.h>

static QoreVarInfo* qore_resolve_static_class_var(const QoreClass* qc, const char* name,
        const QoreClass*& owner_qc) {
    if (!qc || !name || !*name) {
        return nullptr;
    }

    owner_qc = qc;
    const QoreExternalStaticMember* m = qc->findLocalStaticMember(name);
    if (!m) {
        QoreClassHierarchyIterator hi(*qc);
        while (hi.next()) {
            const QoreClass& pqc = hi.get();
            m = pqc.findLocalStaticMember(name);
            if (m) {
                owner_qc = &pqc;
                break;
            }
        }
    }
    return m ? const_cast<QoreVarInfo*>(reinterpret_cast<const QoreVarInfo*>(m)) : nullptr;
}

static const QoreExternalConstant* qore_resolve_static_class_constant(const QoreClass* qc, const char* name) {
    if (!qc || !name || !*name) {
        return nullptr;
    }

    const QoreExternalConstant* c = qc->findConstant(name);
    if (!c) {
        QoreClassHierarchyIterator hi(*qc);
        while (hi.next()) {
            c = hi.get().findConstant(name);
            if (c) {
                break;
            }
        }
    }
    return c;
}

static std::string qore_deferred_static_member_path(const std::string& class_path,
        const std::string& member_name) {
    if (class_path.empty()) {
        return member_name;
    }

    std::string full_path = class_path;
    full_path += "::";
    full_path += member_name;
    return full_path;
}

StaticClassVarRefNode::StaticClassVarRefNode(const QoreProgramLocation* loc, const char* c_str, const QoreClass& n_qc,
        QoreVarInfo& n_vi) : ParseNode(loc, NT_CLASS_VARREF), qc(n_qc), vi(n_vi), str(c_str) {
}

StaticClassVarRefNode::~StaticClassVarRefNode() {
}

int StaticClassVarRefNode::getAsString(QoreString &qstr, int foff, ExceptionSink* xsink) const {
    qstr.sprintf("reference to static class variable %s::%s", qc.getName(), str.c_str());
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *StaticClassVarRefNode::getAsString(bool &del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char* StaticClassVarRefNode::getTypeName() const {
    return "static class variable reference";
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue StaticClassVarRefNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(vi.getReferencedValue(str.c_str(), xsink), xsink);
    // the value here must always require a dereference
    return val->needsEval() ? val->eval(xsink) : val.release();
}

int StaticClassVarRefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    printd(5, "StaticClassVarRefNode::parseInit() '%s::%s'\n", qc.getName(), str.c_str());
    ClassOnlySubstitutionHelper cosh(qore_class_private::get(qc));
    vi.parseInit(str.c_str());
    parse_context.typeInfo = vi.getTypeInfo();
    return 0;
}

int StaticClassVarRefNode::getLValue(LValueHelper& lvh) const {
    return vi.getLValue(lvh, str.c_str());
}

void StaticClassVarRefNode::remove(LValueRemoveHelper& lvrh) {
    QoreAutoVarRWWriteLocker sl(vi.rwl);
    lvrh.doRemove((QoreLValueGeneric&)vi.val, vi.getTypeInfo());
}

const QoreTypeInfo *StaticClassVarRefNode::getTypeInfo() const {
    return vi.getTypeInfo();
}

DeferredStaticClassMemberRefNode::DeferredStaticClassMemberRefNode(const QoreProgramLocation* loc,
        const char* n_class_path, const char* n_member_name)
        : ParseNode(loc, NT_CLASS_VARREF), class_path(n_class_path ? n_class_path : ""),
        member_name(n_member_name ? n_member_name : "") {
}

int DeferredStaticClassMemberRefNode::getAsString(QoreString& qstr, int foff, ExceptionSink* xsink) const {
    if (class_path.empty()) {
        qstr.sprintf("deferred reference to symbol %s", member_name.c_str());
    } else {
        qstr.sprintf("deferred reference to static class member %s::%s", class_path.c_str(), member_name.c_str());
    }
    return 0;
}

QoreString* DeferredStaticClassMemberRefNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

const char* DeferredStaticClassMemberRefNode::getTypeName() const {
    return "deferred static class member reference";
}

QoreValue DeferredStaticClassMemberRefNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    std::string full_path = qore_deferred_static_member_path(class_path, member_name);

    QoreProgram* pgm = getProgram();
    if (pgm) {
        qore_program_private* pp = qore_program_private::get(*pgm);
        const qore_ns_private* found_ns = nullptr;
        if (Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, full_path.c_str(), found_ns)) {
            QoreValue v = var->eval();
            AbstractQoreNode* n = v.getInternalNode();
            if (n && n->getType() == NT_REFERENCE) {
                ReferenceNode* r = reinterpret_cast<ReferenceNode*>(n);
                bool nd = true;
                QoreValue nv = r->eval(nd, xsink);
                if (needs_deref) {
                    discard(v.getInternalNode(), xsink);
                }
                needs_deref = nd;
                return nv;
            }
            return v;
        }

        found_ns = nullptr;
        if (const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(*pp->RootNS,
                full_path.c_str(), found_ns)) {
            return ce->getReferencedValue();
        }
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), class_path.c_str(), false);
    if (!qc) {
        if (qore_aot_source_parse_active()) {
            xsink->raiseException("AOT-PENDING-CONSTANT",
                "cannot evaluate deferred static class member '%s' during AOT source parse", full_path.c_str());
            return QoreValue();
        }
        xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve scoped reference '%s'", full_path.c_str());
        return QoreValue();
    }

    if (class_path.empty()) {
        if (qore_aot_source_parse_active()) {
            xsink->raiseException("AOT-PENDING-CONSTANT",
                "cannot evaluate deferred symbol '%s' during AOT source parse", member_name.c_str());
            return QoreValue();
        }
        xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve symbol '%s'", member_name.c_str());
        return QoreValue();
    }

    const QoreClass* owner_qc = qc;
    if (QoreVarInfo* vi = qore_resolve_static_class_var(qc, member_name.c_str(), owner_qc)) {
        ValueHolder val(vi->getReferencedValue(member_name.c_str(), xsink), xsink);
        return val->needsEval() ? val->eval(xsink) : val.release();
    }

    if (const QoreExternalConstant* c = qore_resolve_static_class_constant(qc, member_name.c_str())) {
        return c->getReferencedValue();
    }

    if (qore_aot_source_parse_active()) {
        xsink->raiseException("AOT-PENDING-CONSTANT",
            "cannot evaluate deferred static class member '%s::%s' during AOT source parse",
            class_path.c_str(), member_name.c_str());
        return QoreValue();
    }

    xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve static class member '%s::%s'",
        class_path.c_str(), member_name.c_str());
    return QoreValue();
}

int DeferredStaticClassMemberRefNode::getLValue(LValueHelper& lvh) const {
    std::string full_path = qore_deferred_static_member_path(class_path, member_name);

    QoreProgram* pgm = getProgram();
    if (pgm) {
        qore_program_private* pp = qore_program_private::get(*pgm);
        const qore_ns_private* found_ns = nullptr;
        if (Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, full_path.c_str(), found_ns)) {
            return var->getLValue(lvh, false);
        }
    }

    if (class_path.empty()) {
        lvh.vl.xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve variable '%s'", member_name.c_str());
        return -1;
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), class_path.c_str(), false);
    if (!qc) {
        lvh.vl.xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve scoped reference '%s'",
            full_path.c_str());
        return -1;
    }

    const QoreClass* owner_qc = qc;
    if (QoreVarInfo* vi = qore_resolve_static_class_var(qc, member_name.c_str(), owner_qc)) {
        return vi->getLValue(lvh, member_name.c_str());
    }

    if (qore_resolve_static_class_constant(qc, member_name.c_str())) {
        lvh.vl.xsink->raiseException("CONSTANT-ERROR", "cannot assign to constant '%s::%s'", class_path.c_str(),
            member_name.c_str());
        return -1;
    }

    lvh.vl.xsink->raiseException("STATIC-MEMBER-ERROR", "cannot resolve static class variable '%s::%s'",
        class_path.c_str(), member_name.c_str());
    return -1;
}

void DeferredStaticClassMemberRefNode::remove(LValueRemoveHelper& lvrh) {
    std::string full_path = qore_deferred_static_member_path(class_path, member_name);

    QoreProgram* pgm = getProgram();
    if (pgm) {
        qore_program_private* pp = qore_program_private::get(*pgm);
        const qore_ns_private* found_ns = nullptr;
        if (Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, full_path.c_str(), found_ns)) {
            var->remove(lvrh);
            return;
        }
    }

    if (class_path.empty()) {
        lvrh.getExceptionSink()->raiseException("STATIC-MEMBER-ERROR", "cannot resolve variable '%s'",
            member_name.c_str());
        return;
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), class_path.c_str(), false);
    if (!qc) {
        lvrh.getExceptionSink()->raiseException("STATIC-MEMBER-ERROR", "cannot resolve scoped reference '%s'",
            full_path.c_str());
        return;
    }

    const QoreClass* owner_qc = qc;
    if (QoreVarInfo* vi = qore_resolve_static_class_var(qc, member_name.c_str(), owner_qc)) {
        QoreAutoVarRWWriteLocker sl(vi->rwl);
        lvrh.doRemove((QoreLValueGeneric&)vi->val, vi->getTypeInfo());
        return;
    }

    if (qore_resolve_static_class_constant(qc, member_name.c_str())) {
        lvrh.getExceptionSink()->raiseException("CONSTANT-ERROR", "cannot remove constant '%s::%s'",
            class_path.c_str(), member_name.c_str());
        return;
    }

    lvrh.getExceptionSink()->raiseException("STATIC-MEMBER-ERROR",
        "cannot resolve static class variable '%s::%s'", class_path.c_str(), member_name.c_str());
}

int DeferredStaticClassMemberRefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (parse_context.pflag & PF_CONST_EXPRESSION) {
        if (qore_aot_source_parse_active()) {
            parse_context.typeInfo = autoTypeInfo;
            return 0;
        }
        parseException(*loc, "ILLEGAL-VARIABLE-REFERENCE", "static class member reference '%s::%s' used illegally "
            "in an expression executed at parse time to initialize a constant value", class_path.c_str(),
            member_name.c_str());
        parse_context.typeInfo = nothingTypeInfo;
        return -1;
    }
    parse_context.typeInfo = autoTypeInfo;
    return 0;
}

const QoreTypeInfo* DeferredStaticClassMemberRefNode::getTypeInfo() const {
    return autoTypeInfo;
}
