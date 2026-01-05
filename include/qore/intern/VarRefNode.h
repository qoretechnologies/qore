/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    VarRefNode.h

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

#ifndef _QORE_VARREFNODE_H
#define _QORE_VARREFNODE_H

#include "qore/intern/FunctionCallNode.h"

class VarRefNewObjectNode;
class LocalVar;
class LocalVarValue;
class Var;
struct ClosureVarValue;

class VarRefNode : public ParseNode {
public:
    union var_u {
        LocalVar* id;         // for local variables
        Var* var;             // for global variables
        ClosureVarValue* cvv; // for immediate values; used with references
    } ref;

    // takes over memory for "n"
    DLLLOCAL VarRefNode(const QoreProgramLocation* loc, char* n, qore_var_t t, bool n_has_effect = false)
            : ParseNode(loc, NT_VARREF, true, n_has_effect), name(n), type(t), new_decl(t == VT_LOCAL),
            explicit_scope(false) {
        if (type == VT_LOCAL)
            ref.id = nullptr;
        assert(type != VT_GLOBAL);
    }

    DLLLOCAL VarRefNode(const QoreProgramLocation* loc, char* n, LocalVar* n_id, bool in_closure)
            : ParseNode(loc, NT_VARREF, true, false), name(n), new_decl(false), explicit_scope(false) {
        ref.id = n_id;
        if (in_closure) {
            setClosureIntern();
        } else {
            type = VT_LOCAL;
        }
    }

    //! returns true if the two variable references refer to the same variable
    /** can be called only at parse time with the parse lock held
    */
    DLLLOCAL bool parseEqualTo(const VarRefNode& other) const;

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS)
            return ref.id->getTypeInfo();
        if (type == VT_GLOBAL || type == VT_THREAD_LOCAL)
            return ref.var->getTypeInfo();
        return 0;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const;

    DLLLOCAL virtual bool stayInTree() const {
        return !(type == VT_GLOBAL);
    }

    DLLLOCAL virtual bool parseIsDecl() const { return type != VT_UNRESOLVED; }
    DLLLOCAL virtual bool isDecl() const { return false; }
    DLLLOCAL bool explicitScope() const { return explicit_scope; }
    DLLLOCAL void setExplicitScope() { explicit_scope = true; }

    // will only be called on *VarRefNewObjectNode objects, but this is their common class
    DLLLOCAL virtual const char* parseGetTypeName() const {
        assert(false);
        return nullptr;
    }

    // for checking for new object calls
    DLLLOCAL virtual AbstractQoreNode* makeNewCall(QoreValue args);

    DLLLOCAL bool isGlobalDecl() const { return new_decl; }

    DLLLOCAL bool isGlobalVar() const { return type == VT_GLOBAL; }

    //DLLLOCAL VarRefNode* isOptimized(const QoreTypeInfo*& typeInfo) const;
    DLLLOCAL int getLValue(LValueHelper& lvh, bool for_remove) const;

    DLLLOCAL bool isRef() const {
        if (type == VT_LOCAL)
            return ref.id->isRef();
        if (type == VT_IMMEDIATE)
            return true;
        assert(type == VT_GLOBAL);
        return ref.var->isRef();
    }

    DLLLOCAL void remove(LValueRemoveHelper& lvrh);

    DLLLOCAL qore_var_t getType() const { return type; }
    DLLLOCAL const char* getName() const { return name.ostr; }
    // called when a list of variables is declared
    DLLLOCAL void makeLocal() {
        assert(type != VT_GLOBAL);
        type = VT_LOCAL;
        new_decl = true;
        ref.id = nullptr;
    }
    // called when a list of variables is declared
    DLLLOCAL virtual void makeGlobal(qore_var_t type);

    // takes the name - caller owns the memory
    DLLLOCAL char* takeName() {
        assert(name.ostr);
        return name.takeName();
    }

    DLLLOCAL void setThreadSafe() {
        if (type == VT_LOCAL)
            setThreadSafeIntern();
    }

    DLLLOCAL void setClosure() {
        if (type == VT_LOCAL || type == VT_LOCAL_TS)
            setClosureIntern();
    }

    DLLLOCAL void setPublic() {
        assert(type == VT_GLOBAL);
        ref.var->setPublic();
    }

    DLLLOCAL void parseAssigned() {
        assert(type != VT_IMMEDIATE);
        if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS)
            ref.id->parseAssigned();
    }

    DLLLOCAL bool scanMembers(RSetHelper& rsh);

protected:
    NamedScope name;
    qore_var_t type : 4;
    bool new_decl : 1;       // is this a new variable declaration
    bool explicit_scope : 1; // scope was explicitly provided

    DLLLOCAL ~VarRefNode() {
        //printd(5, "VarRefNode::~VarRefNode() deleting variable reference %p %s\n", this, name.ostr ? name.ostr : "<taken>");
        assert(type != VT_IMMEDIATE || !ref.cvv);
        assert(type != VT_IMMEDIATE || !ref.cvv);
    }

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        if (type == VT_IMMEDIATE) {
            assert(false);
            ref.cvv->deref(xsink);
#ifdef DEBUG
            ref.cvv = nullptr;
#endif
        }
        return true;
    }

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL void resolve(const QoreTypeInfo* typeInfo);
    DLLLOCAL int parseInitIntern(QoreParseContext& parse_context, bool is_new = false);
    DLLLOCAL VarRefNewObjectNode* globalMakeNewCall(QoreValue args);

    // initializes during parsing
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* parseGetTypeInfo() const {
        if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS)
            return ref.id->parseGetTypeInfo();
        if (type == VT_GLOBAL || type == VT_THREAD_LOCAL)
            return ref.var->parseGetTypeInfo();
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* parseGetTypeInfoForInitialAssignment() const {
        const QoreTypeInfo* rv;
        if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) {
            rv = ref.id->parseGetTypeInfoForInitialAssignment();
        } else if (type == VT_GLOBAL || type == VT_THREAD_LOCAL) {
            rv = ref.var->parseGetTypeInfoForInitialAssignment();
        } else {
            rv = nullptr;
        }
        if (rv && QoreTypeInfo::isReference(rv)) {
            return QoreTypeInfo::getHardReference(rv);
        }
        return rv;
    }

    DLLLOCAL void setThreadSafeIntern() {
        ref.id->setClosureUse();
        type = VT_LOCAL_TS;
    }

    DLLLOCAL void setClosureIntern() {
        ref.id->setClosureUse();
        type = VT_CLOSURE;
    }

    DLLLOCAL VarRefNode(const QoreProgramLocation* loc, char* n, ClosureVarValue* cvv)
            : ParseNode(loc, NT_VARREF, true, false), name(n), type(VT_IMMEDIATE), new_decl(false),
            explicit_scope(false) {
        ref.cvv = cvv;
        cvv->ref();
    }

    DLLLOCAL VarRefNode(const QoreProgramLocation* loc, char* n, Var* n_var, bool n_has_effect = false,
            bool n_new_decl = true, qore_var_t type = VT_GLOBAL) : ParseNode(loc, NT_VARREF, true, n_has_effect),
                name(n), type(type),
            new_decl(n_new_decl), explicit_scope(false) {
        ref.var = n_var;
    }
};

class GlobalVarRefNode : public VarRefNode {
protected:
public:
    DLLLOCAL GlobalVarRefNode(const QoreProgramLocation* loc, char* n, Var* v)
        : VarRefNode(loc, n, v, false, false, v && v->isThreadLocal() ? VT_THREAD_LOCAL : VT_GLOBAL) {
        explicit_scope = true;
    }

    DLLLOCAL GlobalVarRefNode(const QoreProgramLocation* loc, char* n, const QoreTypeInfo* typeInfo = nullptr,
        qore_var_t type = VT_GLOBAL);
    DLLLOCAL GlobalVarRefNode(const QoreProgramLocation* loc, char* n, QoreParseTypeInfo* parseTypeInfo,
        qore_var_t type = VT_GLOBAL);

    DLLLOCAL void reg();
};

class RSetHelper;

class VarRefDeclNode : public VarRefNode {
public:
    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, qore_var_t t, const QoreTypeInfo* n_typeInfo) :
        VarRefNode(loc, n, t), parseTypeInfo(nullptr), typeInfo(n_typeInfo) {
        //printd(5, "VarRefDeclNode::VarRefDeclNode() typeInfo: %p %s type: %d (%s)\n", typeInfo, n, n_qt,
        //  getBuiltinTypeName(n_qt));
    }

    // takes over ownership of class_name
    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, qore_var_t t, char* class_name) :
        VarRefNode(loc, n, t), parseTypeInfo(new QoreParseTypeInfo(class_name)), typeInfo(0) {
        //printd(5, "VarRefDeclNode::VarRefDeclNode() this: %p typeInfo: %p %s type: %d class: %s\n", this, typeInfo,
        //  n, type, class_name);
    }

    // takes over ownership of QoreParseTypeInfo ptr
    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, qore_var_t t, QoreParseTypeInfo* n_parseTypeInfo) :
        VarRefNode(loc, n, t), parseTypeInfo(n_parseTypeInfo), typeInfo(0) {
        //printd(5, "VarRefDeclNode::VarRefDeclNode() this: %p typeInfo: %p %s type: %d class: %s\n", this, typeInfo,
        //  n, type, class_name);
    }

    // takes over ownership of QoreParseTypeInfo ptr
    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, qore_var_t t, const QoreTypeInfo* n_typeInfo,
            QoreParseTypeInfo* n_parseTypeInfo) :
            VarRefNode(loc, n, t), parseTypeInfo(n_parseTypeInfo), typeInfo(n_typeInfo) {
        //printd(5, "VarRefDeclNode::VarRefDeclNode() typeInfo: %p %s type: %d (%s)\n", typeInfo, n, n_qt,
        //  getBuiltinTypeName(n_qt));
    }

    DLLLOCAL ~VarRefDeclNode() {
        delete parseTypeInfo;
    }
    DLLLOCAL virtual bool parseIsDecl() const {
        return true;
    }
    DLLLOCAL virtual bool isDecl() const {
        return true;
    }

    // for checking for new object calls
    DLLLOCAL virtual AbstractQoreNode* makeNewCall(QoreValue args);

    DLLLOCAL QoreParseTypeInfo* takeParseTypeInfo() {
        QoreParseTypeInfo* ti = parseTypeInfo;
        parseTypeInfo = nullptr;
        return ti;
    }
    DLLLOCAL QoreParseTypeInfo* getParseTypeInfo() {
        return parseTypeInfo;
    }
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        assert(!parseTypeInfo);
        return typeInfo;
    }
    DLLLOCAL virtual void makeGlobal(qore_var_t type);

    DLLLOCAL int parseInitCommon(QoreParseContext& parse_context, bool is_new = false);

protected:
    QoreParseTypeInfo* parseTypeInfo;
    const QoreTypeInfo* typeInfo;

    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, qore_var_t t, const QoreTypeInfo* n_typeInfo,
            QoreParseTypeInfo* n_parseTypeInfo, bool n_has_effect) :
        VarRefNode(loc, n, t, n_has_effect), parseTypeInfo(n_parseTypeInfo), typeInfo(n_typeInfo) {
        //printd(5, "VarRefDeclNode::VarRefDeclNode() typeInfo: %p %s type: %d (%s)\n", typeInfo, n, n_qt,
        //  getBuiltinTypeName(n_qt));
    }

    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, Var* var, const QoreTypeInfo* n_typeInfo,
            QoreParseTypeInfo* n_parseTypeInfo) :
        VarRefNode(loc, n, var, true), parseTypeInfo(n_parseTypeInfo), typeInfo(n_typeInfo) {
    }

    // initializes during parsing
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL VarRefDeclNode(const QoreProgramLocation* loc, char* n, ClosureVarValue* cvv,
            const QoreTypeInfo* n_typeInfo) : VarRefNode(loc, n, cvv), parseTypeInfo(0), typeInfo(n_typeInfo) {
    }
};

class VarRefImmediateNode : public VarRefDeclNode {
public:
    DLLLOCAL VarRefImmediateNode(const QoreProgramLocation* loc, char* n, ClosureVarValue* cvv,
            const QoreTypeInfo* n_typeInfo) : VarRefDeclNode(loc, n, cvv, n_typeInfo) {
        //printd(5, "VarRefImmediateNode::VarRefImmediateNode() this: %p '%s' cvv: %p\n", this, name.ostr, cvv);
    }

    DLLLOCAL virtual ~VarRefImmediateNode() {
        //printd(5, "VarRefImmediateNode::~VarRefImmediateNode() this: %p '%s'\n", this, name.ostr);
        assert(!ref.cvv);
    }

protected:
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        //printd(5, "VarRefImmediateNode::derefImpl() this: %p '%s' cvv: %p\n", this, name.ostr, ref.cvv);
        ref.cvv->deref(xsink);
#ifdef DEBUG
        ref.cvv = nullptr;
#endif
        return true;
    }

private:
    DLLLOCAL void deref() {
        assert(false);
        if (ROdereference()) {
            delete this;
        }
    }
};

// special thread-local variables with global scope used to handle module loading errors
/** created by the %try-module parse directive if module loading fails
*/
class VarRefTryModuleErrorNode : public VarRefDeclNode {
public:
    DLLLOCAL VarRefTryModuleErrorNode(const QoreProgramLocation* loc, char* n)
        : VarRefDeclNode(loc, n, VT_LOCAL, hashTypeInfo) {
    }

    DLLLOCAL virtual ~VarRefTryModuleErrorNode() {
    }
};

class VarRefNewObjectNode : public VarRefDeclNode, public FunctionCallBase {
public:
    DLLLOCAL VarRefNewObjectNode(const QoreProgramLocation* loc, char* n, const QoreTypeInfo* n_typeInfo,
            QoreParseTypeInfo* n_parseTypeInfo, QoreParseListNode* n_args, qore_var_t t) :
            VarRefDeclNode(loc, n, t, n_typeInfo, n_parseTypeInfo, true), FunctionCallBase(n_args) {
    }

    DLLLOCAL VarRefNewObjectNode(const QoreProgramLocation* loc, char* n, Var* var, QoreParseListNode* n_args,
            const QoreTypeInfo* n_typeInfo, QoreParseTypeInfo* n_parseTypeInfo) :
            VarRefDeclNode(loc, n, var, n_typeInfo, n_parseTypeInfo), FunctionCallBase(n_args) {
    }

    DLLLOCAL virtual ~VarRefNewObjectNode() {
        //printd(5, "VarRefNewObjectNode::~VarRefNewObjectNode() this: %p (%s)\n", this, getName());
        new_args.discard(nullptr);
    }

    DLLLOCAL virtual bool stayInTree() const {
        return true;
    }

    DLLLOCAL const char* parseGetTypeName() const {
        return typeInfo ? QoreTypeInfo::getName(typeInfo) : parseTypeInfo->cscope->getIdentifier();
    }

    DLLLOCAL QoreParseListNode* takeParseArgs() {
        QoreParseListNode* rv = parse_args;
        parse_args = nullptr;
        return rv;
    }

protected:
    enum vrn_type_e : unsigned char {
        VRN_NONE = 0,
        VRN_OBJECT = 1,
        VRN_HASHDECL = 2,
        VRN_COMPLEXHASH = 3,
        VRN_COMPLEXLIST = 4,
    } vrn_type = VRN_NONE;
    QoreValue new_args{};
    bool runtime_check = false;

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    // initializes during parsing
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL int parseInitConstructorCall(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            const QoreClass* qc);

    DLLLOCAL int parseInitHashDeclInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            const TypedHashDecl* hd);

    DLLLOCAL int parseInitComplexHashInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context);

    DLLLOCAL int parseInitComplexListInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context);
};

#endif
