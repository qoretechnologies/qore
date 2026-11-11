/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    typed_hash_decl_private.h

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

#ifndef _QORE_INTERN_TYPED_HASH_DECL_PRIVATE_H

#define _QORE_INTERN_TYPED_HASH_DECL_PRIVATE_H

#include "qore/intern/QoreClassIntern.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

class typed_hash_decl_private;
class NamedScope;

DLLLOCAL const typed_hash_decl_private* parse_get_hashdecl_type_param_context();

class HashDeclMemberInfo : public QoreMemberInfoBase {
public:
    DLLLOCAL HashDeclMemberInfo(const QoreProgramLocation* loc, const QoreTypeInfo* n_typeInfo = nullptr,
            QoreParseTypeInfo* n_parseTypeInfo = nullptr, QoreValue e = QoreValue())
            : QoreMemberInfoBase(loc, n_typeInfo, n_parseTypeInfo, e) {
    }

    DLLLOCAL HashDeclMemberInfo(const HashDeclMemberInfo& old) : QoreMemberInfoBase(old), init(old.init) {
    }

    DLLLOCAL bool equal(const HashDeclMemberInfo& other) const;

    DLLLOCAL int parseInit(const char* name, bool priv);

    //! marks the member as already parse-initialized, so that parseInit() becomes a no-op
    /** used for members deserialized from an AOT binary: the type is already resolved and any
        default-value expression tree was reconstructed in its post-parse-init form by the AOT
        expression readers, which must not be parse-initialized a second time
    */
    DLLLOCAL void setParseInitDone() {
        assert(!init);
        assert(!parseTypeInfo);
        init = true;
    }

    DLLLOCAL HashDeclMemberInfo* instantiate(const QoreTypeInfo* receiver_type_info) const;

private:
    //! has parseInit() been called on this member?
    bool init = false;
};

typedef QoreMemberMapBase<HashDeclMemberInfo> HashDeclMemberMap;

class typed_hash_decl_private {
friend class typed_hash_decl_member_iterator;
friend class TypedHashDecl;
public:
    //! creates a hashdecl declared in the current Program, which is its source Program
    DLLLOCAL typed_hash_decl_private(const QoreProgramLocation* loc) : loc(loc), orig(this) {
        assignModule();
        setSourceProgram(getProgram());
    }

    DLLLOCAL typed_hash_decl_private(const QoreProgramLocation* loc, const char* n, const char* p,
            TypedHashDecl* thd) :
            loc(loc), name(n), path(p), thd(thd), orig(this),
            typeInfo(new QoreHashDeclTypeInfo(thd, n, p)),
            orNothingTypeInfo(new QoreHashDeclOrNothingTypeInfo(thd, n, p)) {
        assignModule();
    }

    DLLLOCAL typed_hash_decl_private(const typed_hash_decl_private& old, TypedHashDecl* thd);

    DLLLOCAL ~typed_hash_decl_private() {
        assert(!refs.reference_count());
        for (auto& i : parameterized_hashdecl_cache) {
            typed_hash_decl_private::get(*i.second)->deref();
        }
        // the next hashdecl allocated can reuse these addresses
        qore_purge_derived_types(thd, typeInfo, orNothingTypeInfo);
        // method dispatch caches keyed by these type pointers must miss from now on
        qore_bump_method_dispatch_epoch();
        delete typeInfo;
        delete orNothingTypeInfo;
        delete parse_parent;
    }

    DLLLOCAL TypedHashDecl* newTypedHashDecl(const char* n) {
        assert(name.empty());
        assert(!thd);
        name = n;
        path = get_ns_path(n);
        thd = new TypedHashDecl(this);
        return thd;
    }

    DLLLOCAL bool equal(const typed_hash_decl_private& other) const {
        // Imported/copied hashdecls keep the same original declaration pointer.
        // Treat that as identity before structural comparison so AOT/module load
        // ordering cannot make copies of the same declaration temporarily diverge.
        if (orig == other.orig) {
            return true;
        }
        if (name != other.name || members.size() != other.members.size())
            return false;

        for (HashDeclMemberMap::const_iterator ti = members.member_list.begin(),
            oi = other.members.member_list.begin(),
            te = members.member_list.end(); ti != te; ++ti, ++oi) {
            // if the member's name is different, return false
            if (strcmp(oi->first, ti->first)) {
               return false;
            }
            // if the member's type is different, return false
            if (!ti->second->equal(*oi->second)) {
               return false;
            }
        }

        return true;
    }

    DLLLOCAL bool parseEqual(const typed_hash_decl_private& other) const {
        const_cast<typed_hash_decl_private*>(this)->parseInit();
        return equal(other);
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo(bool or_nothing = false) const {
        return or_nothing ? orNothingTypeInfo : typeInfo;
    }

    DLLLOCAL bool isPublic() const {
        return pub;
    }

    DLLLOCAL bool isUserPublic() const {
        return pub && !sys;
    }

    DLLLOCAL void setPublic() {
        assert(!pub);
        pub = true;
    }

    DLLLOCAL void setPrivate() {
        assert(pub);
        pub = false;
    }

    DLLLOCAL bool isSystem() const {
        return sys;
    }

    DLLLOCAL bool isReexport() const {
        return reexport;
    }

    DLLLOCAL void setReexport(bool v = true) {
        reexport = v;
    }

    DLLLOCAL const typed_hash_decl_private* getOrig() const {
        return orig;
    }

    DLLLOCAL void ref() const {
        refs.ROreference();
    }

    DLLLOCAL bool deref() {
        if (!thd) {
            if (refs.ROdereference()) {
                delete this;
            }
            return true;
        }
        if (refs.ROdereference()) {
            assert(thd->priv == this);
            delete thd;
            return true;
        }
        return false;
    }

    DLLLOCAL int parseInit();

    DLLLOCAL int parseInitHashDeclInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            QoreParseListNode* args, bool& runtime_check) const;

    DLLLOCAL int parseCheckHashDeclInitialization(const QoreProgramLocation* loc, const QoreTypeInfo* expTypeInfo,
            QoreValue exp, const char* context_action, bool& runtime_check, bool strict_check = true) const;

    DLLLOCAL int parseCheckHashDeclAssignment(const QoreProgramLocation* loc, const typed_hash_decl_private& hd,
            const char* context, bool& needs_runtime_check, bool strict_check = true) const;

    DLLLOCAL int parseCheckHashDeclAssignment(const QoreProgramLocation* loc, QoreValue n, const char* context,
            bool& needs_runtime_check, bool strict_check = true) const;

    DLLLOCAL int parseCheckComplexHashAssignment(const QoreProgramLocation* loc, const QoreTypeInfo* vti) const;

    DLLLOCAL QoreHashNode* newHash(const QoreParseListNode* args, bool runtime_check, ExceptionSink* xsink) const;

    DLLLOCAL QoreHashNode* newHash(const QoreHashNode* init, bool runtime_check, ExceptionSink* xsink, QoreHashNode* rv = nullptr) const;

    DLLLOCAL QoreHashNode* newHash(const QoreHashNode* init, const QoreHashNode* overrides, bool runtime_check,
            ExceptionSink* xsink) const;

    DLLLOCAL QoreHashNode* newHashFromTemporary(QoreHashNode* init, bool runtime_check,
            ExceptionSink* xsink, bool values_prechecked = false,
            bool layout_prechecked = false) const;

    //! Initializes the members of a hash of the hashdecl
    /** A member initializer that is code of a Program that must be entered (see needsInitEntry()) is evaluated with
        that Program kept; the usual case, a hashdecl that needs nothing, is tested with one load
    */
    DLLLOCAL int initHash(QoreHashNode* h, const QoreHashNode* init, ExceptionSink* xsink) const {
        if (__builtin_expect(!hier_code_init.load(std::memory_order_relaxed), 1)) {
            return initHashDirect(h, init, xsink);
        }
        return initHashChecked(h, init, nullptr, false, false, xsink);
    }

    //! see initHash(QoreHashNode*, const QoreHashNode*, ExceptionSink*)
    DLLLOCAL int initHash(QoreHashNode* h, const QoreHashNode* init, const QoreHashNode* overrides,
            ExceptionSink* xsink) const {
        if (__builtin_expect(!hier_code_init.load(std::memory_order_relaxed), 1)) {
            return initHashDirect(h, init, overrides, xsink);
        }
        return initHashChecked(h, init, overrides, false, false, xsink);
    }

    //! see initHash(QoreHashNode*, const QoreHashNode*, ExceptionSink*); the values given are those in the hash
    DLLLOCAL int initHashInPlace(QoreHashNode* h, ExceptionSink* xsink, bool values_prechecked = false) const {
        if (__builtin_expect(!hier_code_init.load(std::memory_order_relaxed), 1)) {
            return initHashInPlaceDirect(h, xsink, values_prechecked);
        }
        return initHashChecked(h, nullptr, nullptr, true, values_prechecked, xsink);
    }

    DLLLOCAL int runtimeAssignKey(const char* key, ValueHolder& val, ExceptionSink* xsink) const {
        const HashDeclMemberInfo* mem = findMember(key);
        if (!mem) {
            if (xsink) {
                xsink->raiseException("HASHDECL-KEY-ERROR", "cannot assign unknown key '%s' to hashdecl '%s'", key,
                    name.c_str());
            }
            return -1;
        }
        // the outcome must reflect only this type check; see ScopedTypeCheckSink
        ScopedTypeCheckSink ts(xsink);
        QoreTypeInfo::acceptInputKey(mem->getTypeInfo(), key, *val, *ts);
        if (ts.raised()) {
            (*ts)->appendLastDescription(" (while assigning to hashdecl '%s')", name.c_str());
            return -1;
        }
        return 0;
    }

    DLLLOCAL int parseCheckMemberAccess(const QoreProgramLocation* loc, const char* mem,
            const QoreTypeInfo*& memberTypeInfo, int pflag) const;

    //! Calls the function for each member, including inherited members, parent members first
    /** @param f called with the member name and info; returning non-zero stops the iteration

        @return the non-zero value returned by \a f, or 0 if every member was visited
    */
    template <typename F>
    DLLLOCAL int forEachMember(F&& f) const {
        if (parentHashDecl) {
            if (int rc = get(*parentHashDecl)->forEachMember(f)) {
                return rc;
            }
        }
        for (auto& i : members.member_list) {
            if (int rc = f(i.first, i.second)) {
                return rc;
            }
        }
        return 0;
    }

    DLLLOCAL const HashDeclMemberInfo* findMember(const char* m) const {
        const HashDeclMemberInfo* mi = members.find(m);
        if (mi) {
            return mi;
        }
        if (parentHashDecl) {
            return get(*parentHashDecl)->findMember(m);
        }
        return nullptr;
    }

    DLLLOCAL bool matchesLiteralMemberOrder(
            const std::vector<std::string>& keys) const;

    //! Return a stable direct-member ordinal, or -1 for inherited/unknown members.
    DLLLOCAL int getDirectMemberOffset(const char* key) const;

    DLLLOCAL void parseAdd(std::pair<char*, HashDeclMemberInfo*> pair) {
        members.addNoCheck(pair);
    }

    DLLLOCAL void addTypeParameter(const char* param, const char* default_type = nullptr) {
        type_params.emplace_back(param, default_type);
    }

    DLLLOCAL void addTypeParameter(const char* param, const char* default_type, const char* bound_type) {
        type_params.emplace_back(param, default_type, bound_type);
    }

    DLLLOCAL bool hasTypeParams() const {
        return !type_params.empty();
    }

    DLLLOCAL size_t getTypeParamCount() const {
        return type_params.size();
    }

    DLLLOCAL const char* getTypeParamName(size_t i) const {
        assert(i < type_params.size());
        return type_params[i].name.c_str();
    }

    DLLLOCAL const char* getTypeParamDefaultType(size_t i) const {
        assert(i < type_params.size());
        return type_params[i].getDefaultType();
    }

    DLLLOCAL const char* getTypeParamBoundType(size_t i) const {
        assert(i < type_params.size());
        return type_params[i].getBoundType();
    }

    DLLLOCAL size_t getTypeParamRequiredCount() const {
        for (size_t i = 0, e = type_params.size(); i < e; ++i) {
            if (type_params[i].hasDefault()) {
                return i;
            }
        }
        return type_params.size();
    }

    DLLLOCAL bool isParameterizedHashDecl() const {
        return parameterized_base;
    }

    DLLLOCAL const TypedHashDecl* getParameterizedBase() const {
        return parameterized_base;
    }

    DLLLOCAL const std::vector<const QoreTypeInfo*>& getTypeArgs() const {
        return type_args;
    }

    DLLLOCAL const TypedHashDecl* getParameterizedHashDecl(
            const std::vector<const QoreTypeInfo*>& args) const;

    DLLLOCAL const QoreTypeInfo* getParameterizedTypeInfo(const std::vector<const QoreTypeInfo*>& args,
            bool or_nothing = false) const {
        const TypedHashDecl* hd = getParameterizedHashDecl(args);
        return hd ? hd->getTypeInfo(or_nothing) : nullptr;
    }

    DLLLOCAL bool hasMember(const char* name) const {
        if (members.inList(name)) {
            return true;
        }
        if (parentHashDecl) {
            return get(*parentHashDecl)->hasMember(name);
        }
        return false;
    }

    DLLLOCAL void setSystemPublic() {
        assert(!sys);
        assert(!pub);
        sys = true;
        pub = true;
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const char* getPath() const {
        return path.c_str();
    }

    DLLLOCAL const TypedHashDecl* getHashDecl() const {
        return thd;
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL const QoreProgramLocation* getParseLocation() const {
        return loc;
    }

    DLLLOCAL static typed_hash_decl_private* get(TypedHashDecl& hashdecl) {
        return hashdecl.priv;
    }

    DLLLOCAL static const typed_hash_decl_private* get(const TypedHashDecl& hashdecl) {
        return hashdecl.priv;
    }

    DLLLOCAL void addMember(const char* name, const QoreTypeInfo* memberTypeInfo, QoreValue init_val) {
        assert(!members.find(name));
        members.addNoCheck(std::make_pair(strdup(name), new HashDeclMemberInfo(&loc_builtin, memberTypeInfo, nullptr, init_val)));
    }

    DLLLOCAL void setName(const char* n) {
        name = n;
    }

    DLLLOCAL const HashDeclMemberMap& getMembers() const {
        return members;
    }

    DLLLOCAL const HashDeclMemberInfo* findLocalMember(const char* name) const {
        return members.find(name);
    }

    DLLLOCAL HashDeclMemberInfo* findLocalMember(const char* name) {
        return members.find(name);
    }

    DLLLOCAL void setNamespace(qore_ns_private* n) {
        ns = n;
        // C++ modules may instantiate a generic before attaching it to a namespace. Namespace
        // publication (under parse ownership) must bind every cached instance as one operation;
        // cancellation cannot leave only part of the declaration family attached.
        std::lock_guard<std::mutex> lock(parameterized_hashdecl_cache_lock);
        for (auto& i : parameterized_hashdecl_cache) {
            get(*i.second)->setNamespace(n);
        }
    }

    DLLLOCAL const qore_ns_private* getNamespace() const {
        return ns;
    }

    //! Invalidates the parent-namespace back-pointer when that namespace is destroyed
    /** A hashdecl can outlive its namespace exactly as a class can (a hash<T> value keeps the
        hashdecl alive while the declaring Program's namespace tree is destroyed), so the
        back-pointer must be cleared rather than left dangling.

        @param n the namespace being destroyed; only cleared if the back-pointer points at it
    */
    DLLLOCAL void namespaceDeleted(const qore_ns_private* n) {
        if (ns == n) {
            ns = nullptr;
        }
        // Concrete generic declarations are owned by this cache, not by HashDeclList. They can
        // also survive in hash values after namespace teardown. Cleanup must finish even when
        // the Program has been cancelled, so this traversal is deliberately not cancellable.
        std::lock_guard<std::mutex> lock(parameterized_hashdecl_cache_lock);
        for (auto& i : parameterized_hashdecl_cache) {
            get(*i.second)->namespaceDeleted(n);
        }
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    //! Sets the Program that declared the hashdecl, whose code its member initializers are
    /** @param pgm the Program, or nullptr for a hashdecl whose initializers are no code of a Program (builtin
        hashdecls, hashdecls of C++ modules)
    */
    DLLLOCAL void setSourceProgram(QoreProgram* pgm) {
        src_pgm = pgm;
        src_pgm_id = pgm ? pgm->getProgramId() : 0;
    }

    //! Enters the Program that declared the hashdecl, so that it is not deleted while its member initializers run
    /** Called before the first member initializer of the hashdecl is evaluated; the initializers of a parent
        hashdecl are those of its own Program.  See QoreDeclaringProgramHelper.

        @param h the helper that enters the Program and leaves it when it is destroyed
        @param xsink for the \c PROGRAM-ERROR exception raised if the Program has been deleted; may be nullptr

        @return 0 for OK, -1 if the Program has been deleted, in which case the initializers must not be evaluated
    */
    DLLLOCAL int enterSourceProgram(QoreDeclaringProgramHelper& h, ExceptionSink* xsink) const {
        if (!src_pgm) {
            return 0;
        }
        return h.enter(src_pgm, src_pgm_id, src_pgm_module_lifetime, xsink);
    }

    //! returns true if a local member initializer of the hashdecl is code (see HashDeclMemberInfo::initIsCode())
    /** The members do not change once the hashdecl is in use, so the result is computed once
    */
    DLLLOCAL bool hasCodeInit() const {
        int8_t rv = code_init.load(std::memory_order_relaxed);
        if (rv < 0) {
            rv = 0;
            for (auto& i : members.member_list) {
                if (i.second && i.second->initIsCode()) {
                    rv = 1;
                    break;
                }
            }
            code_init.store(rv, std::memory_order_relaxed);
        }
        return rv;
    }

    //! returns true if the hashdecl or a parent has a member initializer that is code and a source Program
    DLLLOCAL bool hasHierarchyCodeInit() const {
        for (const typed_hash_decl_private* hd = this; hd;
                hd = hd->parentHashDecl ? get(*hd->parentHashDecl) : nullptr) {
            if (hd->src_pgm && hd->hasCodeInit()) {
                return true;
            }
        }
        return false;
    }

    //! returns true if the language's creation of a hash must enter the Programs that declared the hashdecls
    /** True if the hashdecl or a parent is a copy of another Program's hashdecl (one that a Program inherited or
        imported) with member initializers that are code, unless the Programs that declared them have module
        lifetime and it is in effect.  A Program's own hashdecl is created by the language only in the code of that
        Program, which the thread is in, so that the Program is not deleted; the API paths that can create a hash of
        it in another Program's code (the type of a value, with the reflection API or in C++ code) enter it in any
        case (see enterSourceProgramsForApi())

        The part that depends on the hashdecls is computed once
    */
    DLLLOCAL bool needsInitEntry() const {
        int8_t rv = hier_code_init.load(std::memory_order_relaxed);
        if (rv < 0) {
            rv = computeInitEntry();
        }
        return rv == 1 || (rv == 2 && !qore_module_lifetime_active());
    }

    //! computes and caches the part of needsInitEntry() that depends on the hashdecls
    /** @return 0 if no hashdecl needs its Program entered, 1 if one does, 2 if only hashdecls whose Programs have
        module lifetime do
    */
    DLLLOCAL int8_t computeInitEntry() const {
        int8_t rv = 0;
        for (const typed_hash_decl_private* hd = this; hd;
                hd = hd->parentHashDecl ? get(*hd->parentHashDecl) : nullptr) {
            if (!hd->copied || !hd->src_pgm || !hd->hasCodeInit()) {
                continue;
            }
            if (!hd->src_pgm_module_lifetime.load(std::memory_order_acquire)) {
                rv = 1;
                break;
            }
            rv = 2;
        }
        hier_code_init.store(rv, std::memory_order_relaxed);
        return rv;
    }

    //! Enters the Programs that declared the hashdecl and its parents for an API path (see needsInitEntry())
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int enterSourceProgramsForApi(QoreDeclaringProgramHelper& h, const QoreHashNode* init,
            const QoreHashNode* overrides, ExceptionSink* xsink) const {
        return hasHierarchyCodeInit() ? enterSourcePrograms(h, init, overrides, xsink) : 0;
    }

    //! Sets the parse-time parent hashdecl type to be resolved during parseInit
    DLLLOCAL void setParseParent(QoreParseTypeInfo* parent) {
        assert(!parse_parent);
        parse_parent = parent;
    }

    //! Resolves the parse-time parent hashdecl type, if any
    DLLLOCAL int resolveParseParent();

    //! Returns the resolved parent hashdecl or nullptr if none
    DLLLOCAL const TypedHashDecl* getParentHashDecl() const {
        return parentHashDecl;
    }

    //! Sets the parent hashdecl directly (for system hashdecls)
    DLLLOCAL void setParentHashDecl(const TypedHashDecl* parent) {
        parentHashDecl = parent;
        parentHashDeclName = parent ? get(*parent)->getPath() : "";
    }

    //! Returns the name of the parent hashdecl (for use when updating parent pointers after copying)
    //! Uses the stored name to avoid dereferencing potentially dangling parentHashDecl pointer
    DLLLOCAL const char* getParentHashDeclName() const {
        return parentHashDeclName.empty() ? nullptr : parentHashDeclName.c_str();
    }

    //! Returns true if this hashdecl is a descendant of the given hashdecl
    DLLLOCAL bool isDescendantOf(const typed_hash_decl_private& ancestor) const {
        const typed_hash_decl_private* current = this;
        while (current->parentHashDecl) {
            const typed_hash_decl_private* parent = get(*current->parentHashDecl);
            if (parent == &ancestor || parent->orig == ancestor.orig) {
                return true;
            }
            current = parent;
        }
        return false;
    }

protected:
    // references
    mutable QoreReferenceCounter refs;
    const QoreProgramLocation* loc;
    std::string name;
    std::string path;
    TypedHashDecl* thd = nullptr;
    // parent namespace
    qore_ns_private* ns = nullptr;
    // the module that defined this class, if any
    std::string from_module;

    //! the Program that declared the hashdecl, whose code its member initializers are, or nullptr if none did
    /** Not referenced: the hashdecl can outlive it (a hash<T> value or a Program that inherited or imported the
        hashdecl keeps the hashdecl), so it is identified with its ID; see enterSourceProgram()
    */
    QoreProgram* src_pgm = nullptr;
    //! the ID of \a src_pgm, which tells it from a later Program at the same address
    qore_program_id_t src_pgm_id = 0;
    //! true once the initializers have found that \a src_pgm has module lifetime; never cleared
    mutable std::atomic<bool> src_pgm_module_lifetime{false};
    //! the cached result of hasCodeInit(): -1 if not yet computed, 0 or 1
    mutable std::atomic<int8_t> code_init{-1};
    //! the cached result of computeInitEntry(): -1 if not yet computed
    mutable std::atomic<int8_t> hier_code_init{-1};
    //! true if the hashdecl is a copy of a hashdecl of \a src_pgm in another Program (inherited or imported)
    bool copied = false;

    // original declaration
    const typed_hash_decl_private* orig;

    // type information
    QoreHashDeclTypeInfo* typeInfo = nullptr;
    QoreHashDeclOrNothingTypeInfo* orNothingTypeInfo = nullptr;

    // member information
    HashDeclMemberMap members;

    // Source generic type parameters and cached concrete instantiations.
    std::vector<QoreGenericTypeParam> type_params;
    mutable std::mutex parameterized_hashdecl_cache_lock;
    mutable std::map<std::vector<const QoreTypeInfo*>, TypedHashDecl*> parameterized_hashdecl_cache;
    const TypedHashDecl* parameterized_base = nullptr;
    std::vector<const QoreTypeInfo*> type_args;

    // parent hashdecl (resolved after parseInit)
    const TypedHashDecl* parentHashDecl = nullptr;
    // parent hashdecl name (stored during copy to avoid dangling pointer dereference)
    std::string parentHashDeclName;
    // parse-time parent type (to be resolved during parseInit)
    QoreParseTypeInfo* parse_parent = nullptr;

    bool pub = false;
    bool sys = false;

    // re-export marker for cross-program imports.  Set by Program::importHashDecl(...
    // reexport=True) on the host side; consulted by qore_program_private::
    // inheritParseImports() in ModuleManager's child-Program creation paths to
    // decide whether the symbol propagates into a transitively-required module's
    // parse context.  Default false preserves the historical "imports stay
    // local" semantic and avoids diamond conflicts with namespace-merged
    // public symbols (e.g. classes brought in via %requires) that the host
    // never opted to expose to %requires children.
    bool reexport = false;

    bool parse_init_done = false;

    DLLLOCAL void assignModule() {
        const char* mod_name = get_module_context_name();
        if (mod_name) {
            from_module = mod_name;
        }
    }

    //! Enters the Programs that declared the hashdecl and its parents for the member initializers to evaluate
    /** @param h the helper that enters the Programs and leaves them when it is destroyed
        @param init the hash whose values the members are given, if any
        @param overrides the hash whose values take precedence over \a init, if any
        @param xsink for the \c PROGRAM-ERROR exception raised if a Program whose initializer would be evaluated has
        been deleted; may be nullptr

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int enterSourcePrograms(QoreDeclaringProgramHelper& h, const QoreHashNode* init,
            const QoreHashNode* overrides, ExceptionSink* xsink) const;

    //! Initializes a hash of a hashdecl that may need its Programs entered (see needsInitEntry() and initHash())
    /** Enters the Programs whose initializers are evaluated, unless they are the current Program
    */
    DLLLOCAL int initHashChecked(QoreHashNode* h, const QoreHashNode* init, const QoreHashNode* overrides,
            bool in_place, bool values_prechecked, ExceptionSink* xsink) const;

    //! initializes the members of a hash, with no Program entered
    DLLLOCAL int initHashDirect(QoreHashNode* h, const QoreHashNode* init, ExceptionSink* xsink) const;
    //! initializes the members of a hash, with no Program entered
    DLLLOCAL int initHashDirect(QoreHashNode* h, const QoreHashNode* init, const QoreHashNode* overrides,
            ExceptionSink* xsink) const;
    //! initializes the members of a hash in place, with no Program entered
    DLLLOCAL int initHashInPlaceDirect(QoreHashNode* h, ExceptionSink* xsink, bool values_prechecked) const;

    //! Enters the Program that declared the hashdecl if a member initializer that is code will be evaluated
    /** Called by enterSourcePrograms() for a hashdecl with member initializers that are code (see hasCodeInit())
        that is not declared by the current Program

        @return 0 for OK, -1 if the Program has been deleted and an initializer that is code would be evaluated
    */
    DLLLOCAL int enterSourceProgramForInitSlow(QoreDeclaringProgramHelper& h, const QoreHashNode* init,
            const QoreHashNode* overrides, ExceptionSink* xsink) const;

    DLLLOCAL int initHashIntern(QoreHashNode* h, const QoreHashNode* init, ExceptionSink* xsink) const;
    DLLLOCAL int initHashIntern(QoreHashNode* h, const QoreHashNode* init, const QoreHashNode* overrides,
            ExceptionSink* xsink) const;
    DLLLOCAL int initHashInternInPlace(QoreHashNode* h, ExceptionSink* xsink,
            bool values_prechecked) const;
};

#endif
