/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ConstantList.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

    constants can only be defined when parsing
    constants values will be substituted during the 2nd parse phase

    reads and writes are (must be) wrapped under the program-level parse lock

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

#ifndef _QORE_CONSTANTLIST_H

#define _QORE_CONSTANTLIST_H

#include <qore/common.h>
#include "qore/intern/ParseNode.h"

#include <string>

class qore_ns_private;
class qore_class_private;

// tricky structure that holds 2 types of pointers and a flag in the space of 1 pointer
// the flag is in the low bit since memory has to be aligned anyway we have at a few bits of space for flags
struct ClassNs {
    // if the low bit is set, then ptr is a qore_ns_priv*, otherwise it's a qore_class_private
    size_t ptr;

    DLLLOCAL ClassNs(qore_class_private* qc) : ptr((size_t)qc) {
    }

    DLLLOCAL ClassNs(qore_ns_private* ns) {
        ptr = ((size_t)ns) | (size_t)1;
    }

    DLLLOCAL ClassNs(const ClassNs& old) : ptr(old.ptr) {
    }

    DLLLOCAL qore_class_private* getClass() const {
        return (!(ptr & (size_t)1)) ? (qore_class_private*)ptr : nullptr;
    }

    DLLLOCAL qore_ns_private* getNs() const {
        return (ptr & (size_t)1) ? (qore_ns_private*)(ptr & ~(size_t)1) : nullptr;
    }

    DLLLOCAL bool isNs() const {
        return (bool)(ptr & (size_t)1);
    }

#ifdef DEBUG
    DLLLOCAL const char* getType() const {
        return isNs() ? "namespace" : "class";
    }

    DLLLOCAL const char* getName() const;
#endif
};

class RuntimeConstantRefNode;

class ConstantEntry : public QoreReferenceCounter {
    friend class ConstantEntryInitHelper;
    friend class RuntimeConstantRefNode;
    friend class ConstantList;

public:
    const QoreProgramLocation* loc;
    ParseWarnOptions pwo;
    std::string name;
    const QoreTypeInfo* typeInfo;
    QoreValue val{};
    bool in_init : 1,     // being initialized
        pub : 1,          // public constant (modules only)
        init : 1,         // already initialized
        builtin : 1,      // builtin vs user
        delayed_eval : 1, // delayed evaluation
        has_init_expr : 1 // had a delayed-eval init expression (for AOT)
        ;

    DLLLOCAL ConstantEntry(const QoreProgramLocation* loc, const char* n, QoreValue v,
        const QoreTypeInfo* ti = nullptr, bool n_pub = false, bool n_init = false, bool n_builtin = false,
        ClassAccess n_access = Public);

    DLLLOCAL ConstantEntry(const ConstantEntry& old);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            del(xsink);
            delete this;
        }
    }

    DLLLOCAL void deref(QoreListNode& l) {
        if (ROdereference()) {
            del(l);
            delete this;
        }
    }

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL ConstantEntry* refSelf() const {
        ref();
        return const_cast<ConstantEntry*>(this);
    }

    DLLLOCAL QoreValue getReferencedValue() const;

    DLLLOCAL const QoreValue getValue() const;

    DLLLOCAL int parseInit(ClassNs ptr);

    DLLLOCAL int parseCommitRuntimeInit();

    DLLLOCAL QoreValue get(const QoreProgramLocation* loc, const QoreTypeInfo*& constantTypeInfo, ClassNs ptr) {
        if (in_init) {
            parse_error(*loc, "recursive constant reference found to constant '%s'", name.c_str());
            constantTypeInfo = nothingTypeInfo;
            return QoreValue();
        }

        if (!init && parseInit(ptr)) {
            constantTypeInfo = nothingTypeInfo;
            return QoreValue();
        }

        constantTypeInfo = typeInfo;
        return val;
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL bool isPublic() const {
        return pub;
    }

    DLLLOCAL bool isUserPublic() const {
        return pub && !builtin;
    }

    DLLLOCAL bool isSystem() const {
        return builtin;
    }

    DLLLOCAL bool isUser() const {
        return !builtin;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return access;
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    //! Returns true if this constant had a delayed-eval init expression
    DLLLOCAL bool hasInitExpr() const {
        return has_init_expr;
    }

    //! Returns the preserved init expression (for AOT lowering); NOTHING if not preserved
    DLLLOCAL const QoreValue getInitExpr() const {
        return aot_init_expr;
    }

    //! Discard the preserved init expression (call after AOT lowering is complete)
    DLLLOCAL void discardInitExpr(ExceptionSink* xsink) {
        aot_init_expr.discard(xsink);
    }

protected:
    QoreValue saved_val{};
    QoreValue aot_init_expr{};  //!< preserved init expression for AOT lowering
    ClassAccess access;
    std::string from_module;

    DLLLOCAL ~ConstantEntry() {
        assert(saved_val.isNothing());
        assert(aot_init_expr.isNothing());
        assert(val.isNothing());
    }

    DLLLOCAL void del(ExceptionSink* xsink);
    DLLLOCAL void del(QoreListNode& l);
};

class ConstantEntryInitHelper {
protected:
    ConstantEntry &ce;

public:
    DLLLOCAL ConstantEntryInitHelper(ConstantEntry& n_ce) : ce(n_ce) {
        assert(!ce.in_init);
        assert(!ce.init);
        ce.in_init = true;
        //printd(5, "ConstantEntryInitHelper::ConstantEntryInitHelper() '%s'\n", ce.getName());
    }

    DLLLOCAL ~ConstantEntryInitHelper() {
        ce.in_init = false;
        ce.init = true;
        //printd(5, "ConstantEntryInitHelper::~ConstantEntryInitHelper() '%s'\n", ce.getName());
    }
};

// we use a vector map as the number of constants is generally relatively small
// and lookups are only performed during parsing
#include <qore/vector_map>
typedef vector_map_t<const char*, ConstantEntry*> cnemap_t;
/*
#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, ConstantEntry*, qore_hash_str, eqstr> cnemap_t;
#else
#include <map>
typedef std::map<const char*, ConstantEntry*, ltstr> cnemap_t;
#endif
*/

class ConstantList {
    friend class ConstantListIterator;
    friend class ConstConstantListIterator;

private:
    // not implemented
    DLLLOCAL ConstantList& operator=(const ConstantList&);

    DLLLOCAL void clearIntern(ExceptionSink* xsink);

    size_t runtime_init_hwm = 0;  // high water mark for initialized constants

protected:
    // the object that owns the list (either a class or a namespace)
    ClassNs ptr;

public:
    vector_map_t<std::string, ConstantEntry*> new_cnemap;
    cnemap_t cnemap;

    DLLLOCAL ~ConstantList();

    DLLLOCAL ConstantList(ClassNs p) : ptr(p) {
        //printd(5, "ConstantList::ConstantList() this: %p cls: %p ns: %p\n", this, ptr.getClass(), ptr.getNs());
    }

    DLLLOCAL ConstantList(const ConstantList& old, const QoreParseOptions& po, ClassNs p);

    // do not delete the object returned by this function
    DLLLOCAL cnemap_t::iterator add(const char* name, QoreValue val, const QoreTypeInfo* typeInfo = nullptr,
            ClassAccess access = Public);

    DLLLOCAL cnemap_t::iterator parseAdd(const QoreProgramLocation* loc, const char* name, QoreValue val,
            const QoreTypeInfo* typeInfo = nullptr, bool pub = false, ClassAccess access = Public);

    //! Add a pre-created ConstantEntry (takes ownership)
    DLLLOCAL void addEntry(const char* name, ConstantEntry* ce) {
        cnemap.insert(cnemap_t::value_type(name, ce));
    }

    DLLLOCAL ConstantEntry* findEntry(const char* name);

    DLLLOCAL const ConstantEntry* findEntry(const char* name) const;

    DLLLOCAL QoreValue find(const char* name, const QoreTypeInfo*& constantTypeInfo, ClassAccess& access,
            bool& found);

    DLLLOCAL QoreValue find(const char* name, const QoreTypeInfo*& constantTypeInfo, bool& found) {
        ClassAccess access;
        return find(name, constantTypeInfo, access, found);
    }

    DLLLOCAL bool inList(const char* name) const;
    DLLLOCAL bool inList(const std::string& name) const;
    //DLLLOCAL ConstantList *copy();

    // assimilate the list without any duplicate checking
    DLLLOCAL void assimilate(ConstantList& n);

    // assimilate a constant list in a namespace with duplicate checking (also in pending list)
    DLLLOCAL void assimilate(ConstantList& n, const char* type, const char* name,
        std::vector<std::string>* pending_names = nullptr);

    // copy all user/public elements of the source list to the target, assuming no duplicates
    DLLLOCAL void mergeUserPublic(const ConstantList& src);

    DLLLOCAL int importSystemConstants(const ConstantList& src, ExceptionSink* xsink);

    // add a constant to a list with duplicate checking (pub & priv + pending)
    DLLLOCAL void parseAdd(const QoreProgramLocation* loc, const std::string& name, QoreValue val, ClassAccess access,
            const char* cname);

    DLLLOCAL int parseInit();
    DLLLOCAL int parseCommitRuntimeInit();

    DLLLOCAL QoreHashNode* getInfo();
    DLLLOCAL void parseDeleteAll();
    DLLLOCAL void clear(QoreListNode& l);
    DLLLOCAL void deleteAll(ExceptionSink* xsink);
    DLLLOCAL void reset();
    //! Removes a specific constant by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name, ExceptionSink* xsink);

    DLLLOCAL bool empty() const {
        return cnemap.empty();
    }

    DLLLOCAL cnemap_t::iterator end() {
        return cnemap.end();
    }

    DLLLOCAL cnemap_t::const_iterator end() const {
        return cnemap.end();
    }

    DLLLOCAL void setAccess(ClassAccess access) {
        for (auto& i : cnemap)
            i.second->access = access;
    }
};

class ConstantListIterator {
protected:
    cnemap_t& cl;
    cnemap_t::iterator i;

public:
    DLLLOCAL ConstantListIterator(ConstantList& n_cl) : cl(n_cl.cnemap), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end()) {
            i = cl.begin();
        } else {
            ++i;
        }
        return i != cl.end();
    }

    DLLLOCAL const std::string& getName() const {
        return i->second->getNameStr();
    }

    DLLLOCAL QoreValue getValue() const {
        return i->second->val;
    }

    DLLLOCAL ConstantEntry* getEntry() const {
        return i->second;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return i->second->getAccess();
    }

    DLLLOCAL bool isPublic() const {
        return i->second->isPublic();
    }

    DLLLOCAL bool isUserPublic() const {
        return i->second->isUserPublic();
    }
};

class ConstConstantListIterator {
protected:
    const cnemap_t& cl;
    cnemap_t::const_iterator i;

public:
    DLLLOCAL ConstConstantListIterator(const ConstantList& n_cl) : cl(n_cl.cnemap), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end())
            i = cl.begin();
        else
            ++i;
        return i != cl.end();
    }

    DLLLOCAL const std::string& getName() const {
        return i->second->getNameStr();
    }

    DLLLOCAL const QoreValue getValue() const {
        return i->second->val;
    }

    DLLLOCAL const ConstantEntry* getEntry() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const {
        return i->second->isPublic();
    }

    DLLLOCAL bool isUserPublic() const {
        return i->second->isUserPublic();
    }
};

class RuntimeConstantRefNode : public ParseNode {
protected:
    ConstantEntry* ce;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = ce->typeInfo;
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return ce->typeInfo;
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return ce->saved_val.eval(needs_deref, xsink);
    }

    DLLLOCAL ~RuntimeConstantRefNode() {
    }

public:
    DLLLOCAL RuntimeConstantRefNode(const QoreProgramLocation* loc, ConstantEntry* n_ce) : ParseNode(loc,
            NT_RTCONSTREF, true, false), ce(n_ce) {
        assert(ce->saved_val);
    }

    DLLLOCAL ConstantEntry* getConstantEntry() const {
        return ce;
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        assert(ce->saved_val);
        return ce->saved_val.getAsString(str, foff, xsink);
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        assert(ce->saved_val);
        return ce->saved_val.getAsString(del, foff, xsink);
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return ce->saved_val.getTypeName();
    }
};

#endif // _QORE_CONSTANTLIST_H
