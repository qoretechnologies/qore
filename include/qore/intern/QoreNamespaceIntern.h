/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreNamespaceIntern.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORENAMESPACEINTERN_H
#define _QORE_QORENAMESPACEINTERN_H

#include "qore/intern/QoreClassList.h"
#include "qore/intern/HashDeclList.h"
#include "qore/intern/QoreNamespaceList.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/GlobalVariableList.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/vector_map"

#include <map>
#include <vector>
#include <list>

class qore_root_ns_private;
class qore_ns_private;

typedef std::list<const qore_ns_private*> nslist_t;

class qore_ns_private {
public:
    const QoreProgramLocation* loc;
    std::string name;
    // the fully-justified reference including the namespace name
    std::string path;

    QoreNamespace* ns = nullptr;

    QoreClassList classList;       // class map
    HashDeclList hashDeclList;     // hashdecl map
    ConstantList constant;         // constant map
    QoreNamespaceList nsl;         // namespace map
    FunctionList func_list;        // function map
    GlobalVariableList var_list;   // global variable map
    gvblist_t pend_gvblist;        // global variable declaration list

    // 0 = root namespace, ...
    unsigned depth = 0;

    bool root = false,   // is this the root namespace?
        pub,      // is this namespace public (inherited by child programs or programs importing user modules)
        builtin,  // is this namespace builtin?
        imported = false; // was this namespace imported?

    // pointer to parent namespace (nullptr if this is the root namespace or an unattached namespace)
    const qore_ns_private* parent = nullptr;
    q_ns_class_handler_t class_handler = nullptr;

    // namespace key-value store
    mutable QoreThreadLock kvlck;
    typedef std::map<std::string, QoreValue> kvmap_t;
    kvmap_t kvmap;

    // used with builtin namespaces
    DLLLOCAL qore_ns_private(QoreNamespace* n_ns, const char* n) : name(n), ns(n_ns), constant(this), pub(true),
            builtin(true) {
        size_t i = name.rfind("::");
        path = name;
        if (i == std::string::npos) {
            // add the root '::' prefix to the path
            path.insert(0, "::");
        } else {
            if (!i) {
                name.erase(0, 2);
            } else {
                name.erase(0, i + 2);
                // add the root '::' prefix to the path
                path.insert(0, "::");
            }
        }
        const char* mod_name = get_module_context_name();
        if (mod_name) {
            from_module = mod_name;
        }
    }

    // called when assimilating
    DLLLOCAL qore_ns_private(const char* n, const qore_ns_private& old)
        : name(n),
            path(old.path),
            ns(new QoreNamespace(this)),
            constant(this), pub(old.pub),
            builtin(false), from_module(old.from_module) {
    }

    // called when parsing
    DLLLOCAL qore_ns_private(const QoreProgramLocation* loc);

    DLLLOCAL qore_ns_private(const qore_ns_private& old, int64 po, QoreNamespace* ns)
        : name(old.name),
            path(old.path),
            ns(ns),
            classList(old.classList, po, this),
            hashDeclList(old.hashDeclList, po, this),
            constant(old.constant, po, this),
            nsl(old.nsl, po, *this),
            func_list(old.func_list, this, po),
            var_list(old.var_list, po),
            depth(old.depth),
            root(old.root),
            pub(old.builtin ? true : false),
            builtin(old.builtin),
            imported(old.imported),
            class_handler(old.class_handler) {
        if (!old.from_module.empty()) {
            from_module = old.from_module;
        }
    }

    DLLLOCAL ~qore_ns_private();

    // get the full namespace path with the leading "::"
    DLLLOCAL const char* getPath() const {
        assert(!path.empty());
        return path.c_str();
    }

    DLLLOCAL void getPath(std::string& str, bool anchored = false, bool need_next = false) const {
        str = path;
        if (!anchored) {
            str.erase(0, 2);
        }
        if (need_next && !root) {
            str.append("::");
        }
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

   //! Sets a key value in the namespace's key-value store
    /** @param key the key to store
        @param value the value to store

        @return any value previously stored in that key; must be dereferenced by the caller

        @since %Qore 1.0.13
    */
    DLLLOCAL QoreValue setKeyValue(const std::string& key, QoreValue val);

    //! Sets a key value in the namespace's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param value the value to store; must be already referenced for storage

        @return returns \a value if another value already exists for that key, otherwise returns no value

        @note if \a value is returned, the caller must dereference it

        @since %Qore 1.0.13
    */
    DLLLOCAL QoreValue setKeyValueIfNotSet(const std::string& key, QoreValue val);

    //! Sets a key value in the namespace's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param value the string to store; will be converted to a QoreStringNode if stored

        @note All namespace key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLLOCAL bool setKeyValueIfNotSet(const std::string& key, const char* str);

    //! Returns a referenced key value from the namespace's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @since %Qore 1.0.13
    */
    DLLLOCAL QoreValue getReferencedKeyValue(const std::string& key) const;

    //! Returns a referenced key value from the namespace's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @since %Qore 1.0.13
    */
    DLLLOCAL QoreValue getReferencedKeyValue(const char* key) const;

    DLLLOCAL void getNsList(nslist_t& nsl) const {
        //printd(5, "qore_ns_private::getNsList() this: %p '%s::' root: %d\n", this, name.c_str(), root);
        if (root)
            return;

        const qore_ns_private* w = this;
        while (w && w->parent) {
            nsl.push_front(w);
            w = w->parent;
        }
    }

    // destroys the object and frees all associated memory
    DLLLOCAL void purge() {
        constant.reset();

        classList.reset();

        hashDeclList.reset();

        nsl.reset();
    }

    DLLLOCAL qore_root_ns_private* getRoot() {
        qore_ns_private* w = this;
        while (w->parent) {
            w = (qore_ns_private*)w->parent;
        }

        return w->root ? reinterpret_cast<qore_root_ns_private*>(w) : nullptr;
    }

    DLLLOCAL const qore_root_ns_private* getRoot() const {
        const qore_ns_private* w = this;
        while (w->parent) {
            w = (qore_ns_private*)w->parent;
        }

        return w->root ? reinterpret_cast<const qore_root_ns_private*>(w) : nullptr;
    }

    DLLLOCAL QoreProgram* getProgram() const;

    DLLLOCAL void setClassHandler(q_ns_class_handler_t n_class_handler);

    // finds a local class in the committed class list, if not found executes the class handler
    DLLLOCAL QoreClass* findLoadClass(const char* cname) {
        //printd(5, "qore_ns_private::findLoadClass('%s') this: %p ('%s') class_handler: %p found: %d\n", cname, this,
        //  name.c_str(), class_handler, classList.find(cname));
        QoreClass* qc = classList.find(cname);
        if (!qc && class_handler)
            qc = class_handler(ns, cname);
        return qc;
    }

    DLLLOCAL void getGlobalVars(QoreHashNode& h) const {
        std::string path;
        getPath(path);
        var_list.getGlobalVars(path, h);
        nsl.getGlobalVars(h);
    }

    DLLLOCAL void clearConstants(QoreListNode& l);
    DLLLOCAL void clearData(ExceptionSink* xsink);
    DLLLOCAL void deleteData(bool deref_vars, ExceptionSink* xsink);
    //DLLLOCAL void deleteClearData(ExceptionSink* xsink);

    DLLLOCAL void parseAssimilate(QoreNamespace* ns);
    DLLLOCAL void runtimeAssimilate(QoreNamespace* ns);

    DLLLOCAL void updateDepthRecursive(unsigned ndepth);

    DLLLOCAL int parseAddPendingClass(const QoreProgramLocation* loc, const NamedScope& n, QoreClass* oc);
    DLLLOCAL int parseAddPendingClass(const QoreProgramLocation* loc, QoreClass* oc);

    DLLLOCAL int parseAddPendingHashDecl(const QoreProgramLocation* loc, const NamedScope& n,
            TypedHashDecl* hashdecl);
    DLLLOCAL int parseAddPendingHashDecl(const QoreProgramLocation* loc, TypedHashDecl* hashdecl);

    DLLLOCAL bool addGlobalVars(qore_root_ns_private& rns);

    DLLLOCAL cnemap_t::iterator parseAddConstant(const QoreProgramLocation* loc, const char* name, QoreValue value,
            bool pub);

    DLLLOCAL void parseAddConstant(const QoreProgramLocation* loc, const NamedScope& name, QoreValue value,
            bool pub);

    DLLLOCAL int parseAddMethodToClass(const QoreProgramLocation* loc, const NamedScope& name,
            MethodVariantBase* qcmethod, bool static_flag);

    DLLLOCAL int checkImportFunction(const char* name, ExceptionSink* xsink) {
        //printd(5, "qore_ns_private::checkImportFunction(%s) this: %p\n", name, this);

        if (func_list.findNode(name)) {
            xsink->raiseException("FUNCTION-IMPORT-ERROR", "function '%s' already exists in this namespace", name);
            return -1;
        }

        return 0;
    }

    DLLLOCAL FunctionEntry* runtimeImportFunction(ExceptionSink* xsink, QoreFunction* u,
            const char* new_name = nullptr, bool inject = false) {
        const char* fn = new_name ? new_name : u->getName();
        if (checkImportFunction(fn, xsink))
            return 0;

        return func_list.import(fn, u, this, inject);
    }

    DLLLOCAL int checkImportClass(const char* cname, ExceptionSink* xsink) {
        //printd(5, "qore_ns_private::checkImportClass(%s) this: %p\n", name, this);

        if (classList.find(cname)) {
            xsink->raiseException("CLASS-IMPORT-ERROR", "class '%s' already exists in namespace '%s'", cname,
                name.c_str());
            return -1;
        }
        if (hashDeclList.find(cname)) {
            xsink->raiseException("CLASS-IMPORT-ERROR", "hashdecl '%s' already exists in namespace '%s'", cname,
                name.c_str());
            return -1;
        }
        if (nsl.find(cname)) {
            xsink->raiseException("CLASS-IMPORT-ERROR", "a subnamespace named '%s' already exists in namespace '%s'",
                cname, name.c_str());
            return -1;
        }

        return 0;
    }

    DLLLOCAL int checkImportHashDecl(const char* hdname, ExceptionSink* xsink) {
        //printd(5, "qore_ns_private::checkImportHashDecl(%s) this: %p\n", hdname, this);

        if (hashDeclList.find(hdname)) {
            xsink->raiseException("HASHDECL-IMPORT-ERROR", "hashdecl '%s' already exists in namespace '%s'", hdname,
                name.c_str());
            return -1;
        }
        if (classList.find(hdname)) {
            xsink->raiseException("HASHDECL-IMPORT-ERROR", "class '%s' already exists in namespace '%s'", hdname,
                name.c_str());
            return -1;
        }

        return 0;
    }

    DLLLOCAL QoreClass* runtimeImportClass(ExceptionSink* xsink, const QoreClass* c, QoreProgram* spgm,
            q_setpub_t set_pub, const char* new_name = nullptr, bool inject = false,
            const qore_class_private* injectedClass = nullptr);

    DLLLOCAL TypedHashDecl* runtimeImportHashDecl(ExceptionSink* xsink, const TypedHashDecl* hd, QoreProgram* spgm,
            q_setpub_t set_pub, const char* new_name = nullptr);

    DLLLOCAL const FunctionEntry* runtimeFindFunctionEntry(const char* name) {
        return func_list.findNode(name, true);
    }

    DLLLOCAL const QoreFunction* runtimeFindFunction(const char* name) {
        return func_list.find(name, true);
    }

    DLLLOCAL const QoreFunction* findAnyFunction(const char* name) {
        return func_list.find(name, false);
    }

    DLLLOCAL QoreNamespace* findCreateNamespace(const char* nme, bool user, bool& is_new, qore_root_ns_private* rns);
    DLLLOCAL QoreNamespace* findCreateNamespacePath(const nslist_t& nsl, bool user, bool& is_new);
    DLLLOCAL QoreNamespace* findCreateNamespacePath(const NamedScope& nspath, bool pub, bool user, bool& is_new,
            int ignore_end = 1);

    DLLLOCAL TypedHashDecl* parseFindLocalHashDecl(const char* name) {
        return hashDeclList.find(name);
    }

    DLLLOCAL QoreValue getConstantValue(const char* name, const QoreTypeInfo*& typeInfo, bool& found);
    DLLLOCAL QoreClass* parseFindLocalClass(const char* name);
    DLLLOCAL qore_ns_private* parseAddNamespace(QoreNamespace* nns);

    DLLLOCAL void addModuleNamespace(qore_ns_private* nns, QoreModuleContext& qmc);
    DLLLOCAL void addCommitNamespaceIntern(qore_ns_private* nns);
    DLLLOCAL void addNamespace(qore_ns_private* nns);

    DLLLOCAL int parseInit();
    DLLLOCAL void parseResolveHierarchy();
    DLLLOCAL void parseResolveClassMembers();
    DLLLOCAL void parseResolveAbstract();
    DLLLOCAL int parseInitConstants();
    DLLLOCAL void parseRollback(ExceptionSink* xsink);
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* xsink);

    DLLLOCAL Var* runtimeMatchGlobalVar(const NamedScope& nscope, const qore_ns_private*& rns) const;
    DLLLOCAL const ConstantEntry* runtimeMatchNamespaceConstant(const NamedScope& nscope,
            const qore_ns_private*& rns) const;
    DLLLOCAL const QoreClass* runtimeMatchScopedClassWithMethod(const NamedScope& nscope) const;
    DLLLOCAL const QoreClass* runtimeMatchClass(const NamedScope& nscope, const qore_ns_private*& rns) const;
    DLLLOCAL const qore_ns_private* runtimeMatchNamespace(const NamedScope& nscope, int offset = 0) const;
    DLLLOCAL const qore_ns_private* runtimeMatchAddClass(const NamedScope& nscope, bool& fnd) const;

    DLLLOCAL const TypedHashDecl* runtimeMatchHashDecl(const NamedScope& nscope, const qore_ns_private*& rns) const;

    DLLLOCAL const FunctionEntry* runtimeMatchFunctionEntry(const NamedScope& nscope) const;
    DLLLOCAL const qore_ns_private* runtimeMatchAddFunction(const NamedScope& nscope, bool& fnd) const;

    //DLLLOCAL const QoreFunction* parseMatchFunction(const NamedScope& nscope, unsigned& match) const;

    DLLLOCAL const FunctionEntry* parseMatchFunctionEntry(const NamedScope& nscope, unsigned& match) const;

    DLLLOCAL QoreNamespace* resolveNameScope(const QoreProgramLocation* loc, const NamedScope& name) const;
    DLLLOCAL QoreNamespace* parseMatchNamespace(const NamedScope& nscope, unsigned& matched) const;

    DLLLOCAL TypedHashDecl* parseMatchScopedHashDecl(const NamedScope& name, unsigned& matched);

    DLLLOCAL QoreClass* parseMatchScopedClass(const NamedScope& name, unsigned& matched);
    DLLLOCAL QoreClass* parseMatchScopedClassWithMethod(const NamedScope& nscope, unsigned& matched);

    DLLLOCAL QoreValue parseCheckScopedReference(const QoreProgramLocation* loc, const NamedScope& ns, unsigned& m,
            const QoreTypeInfo*& typeInfo, bool& found, bool abr) const;

    DLLLOCAL QoreValue parseFindLocalConstantValue(const QoreProgramLocation* loc, const NamedScope& ns, unsigned& m,
            const QoreTypeInfo*& typeInfo, bool& found, bool abr) const;

    DLLLOCAL QoreValue parseFindLocalConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found);
    DLLLOCAL QoreNamespace* parseFindLocalNamespace(const char* nname);

    DLLLOCAL QoreValue parseMatchScopedConstantValue(const NamedScope& name, unsigned& matched,
            const QoreTypeInfo*& typeInfo, bool& found);

    DLLLOCAL FunctionEntry* addPendingVariantIntern(const char* fname, AbstractQoreFunctionVariant* v, bool& new_func);

    DLLLOCAL void addBuiltinVariant(const char* name, AbstractQoreFunctionVariant* v);
    DLLLOCAL void addBuiltinModuleVariant(const char* name, AbstractQoreFunctionVariant* v, QoreModuleContext& qmc);
    DLLLOCAL void addBuiltinVariantIntern(const char* name, AbstractQoreFunctionVariant* v);

    template <typename T, class B>
    DLLLOCAL void addBuiltinVariant(const char* name, T f, int64 flags, int64 functional_domain,
            const QoreTypeInfo* returnTypeInfo, unsigned num_params, va_list args) {
        //printd(5, "qore_ns_private::addBuiltinVariant('%s', %p, flags=%lld) BEFORE\n", name, f, flags);
        type_vec_t typeList;
        arg_vec_t defaultArgList;
        name_vec_t nameList;
        if (num_params)
            qore_process_params(num_params, typeList, defaultArgList, nameList, args);

        //printd(5, "qore_ns_private::addBuiltinVariant('%s', %p, flags=%lld, domain=%lld, ret=%s, num_params=%d, "
        //    "...)\n", name, f, flags, functional_domain, QoreTypeInfo::getName(returnTypeInfo), num_params);
        addBuiltinVariant(name, new B(f, flags, functional_domain, returnTypeInfo, typeList, defaultArgList,
            nameList));
    }

    template <typename T, class B>
    DLLLOCAL void addBuiltinVariant(void* ptr, const char* name, T f, int64 flags, int64 functional_domain,
            const QoreTypeInfo* returnTypeInfo, unsigned num_params, va_list args) {
        //printd(5, "qore_ns_private::addBuiltinVariant('%s', %p, flags=%lld) BEFORE\n", name, f, flags);
        type_vec_t typeList;
        arg_vec_t defaultArgList;
        name_vec_t nameList;
        if (num_params)
            qore_process_params(num_params, typeList, defaultArgList, nameList, args);

        //printd(5, "qore_ns_private::addBuiltinVariant('%s', %p, flags=%lld, domain=%lld, ret=%s, num_params=%d, "
        //    "...)\n", name, f, flags, functional_domain, QoreTypeInfo::getName(returnTypeInfo), num_params);
        addBuiltinVariant(name, new B(ptr, f, flags, functional_domain, returnTypeInfo, typeList, defaultArgList,
            nameList));
    }

    DLLLOCAL void scanMergeCommittedNamespace(const qore_ns_private& mns, QoreModuleContext& qmc) const;
    DLLLOCAL void copyMergeCommittedNamespace(const qore_ns_private& mns);

    DLLLOCAL int parseInitGlobalVars();

    DLLLOCAL int checkGlobalVarDecl(Var* v, const NamedScope& vname);
    DLLLOCAL void parseAddGlobalVarDecl(const QoreProgramLocation* loc, char* name, const QoreTypeInfo* typeInfo,
            QoreParseTypeInfo* parseTypeInfo, bool pub, qore_var_t type);

    DLLLOCAL void setPublic();

    DLLLOCAL void runtimeImportSystemClasses(const qore_ns_private& source, qore_root_ns_private& rns,
            ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemHashDecls(const qore_ns_private& source, qore_root_ns_private& rns,
            ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemConstants(const qore_ns_private& source, qore_root_ns_private& rns,
            ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemFunctions(const qore_ns_private& source, qore_root_ns_private& rns,
            ExceptionSink* xsink);

    DLLLOCAL static void addNamespace(QoreNamespace& ns, QoreNamespace* nns) {
        ns.priv->addNamespace(nns->priv);
    }

    DLLLOCAL static QoreValue parseResolveReferencedClassConstant(const QoreProgramLocation* loc, QoreClass* qc,
            const char* name, const QoreTypeInfo*& typeInfo, bool& found);

    DLLLOCAL static ConstantList& getConstantList(const QoreNamespace* ns) {
        return ns->priv->constant;
    }

    DLLLOCAL static const QoreFunction* runtimeFindFunction(QoreNamespace& ns, const char* name) {
        return ns.priv->runtimeFindFunction(name);
    }

    DLLLOCAL static const FunctionEntry* runtimeFindFunctionEntry(QoreNamespace& ns, const char* name) {
        return ns.priv->runtimeFindFunctionEntry(name);
    }

    DLLLOCAL static QoreListNode* getUserFunctionList(QoreNamespace& ns) {
        return ns.priv->func_list.getList();
    }

    DLLLOCAL static void parseAddPendingClass(QoreNamespace& ns, const QoreProgramLocation* loc, const NamedScope& n,
            QoreClass* oc) {
        ns.priv->parseAddPendingClass(loc, n, oc);
    }

    DLLLOCAL static void parseAddNamespace(QoreNamespace& ns, QoreNamespace* nns) {
        ns.priv->parseAddNamespace(nns);
    }

    DLLLOCAL static void parseAddConstant(QoreNamespace& ns, const QoreProgramLocation* loc, const NamedScope& name,
            QoreValue value, bool pub) {
        ns.priv->parseAddConstant(loc, name, value, pub);
    }

    DLLLOCAL static void parseCommit(QoreNamespace& ns) {
        ns.priv->parseCommit();
    }

    DLLLOCAL static void purge(QoreNamespace& ns) {
        ns.priv->purge();
    }

    DLLLOCAL static qore_ns_private* get(QoreNamespace& ns) {
        return ns.priv;
    }

    DLLLOCAL static const qore_ns_private* get(const QoreNamespace& ns) {
        return ns.priv;
    }

    DLLLOCAL static bool isPublic(const QoreNamespace& ns) {
        return ns.priv->pub;
    }

    DLLLOCAL static bool isUserPublic(const QoreNamespace& ns) {
        return ns.priv->pub && !ns.priv->builtin;
    }

private:
    // not implemented
    DLLLOCAL qore_ns_private(const qore_ns_private&) = delete;
    // not implemented
    DLLLOCAL qore_ns_private& operator=(const qore_ns_private&) = delete;

protected:
    // the module that defined this class, if any
    std::string from_module;

    // called from the root namespace constructor only
    DLLLOCAL qore_ns_private(QoreNamespace* n_ns) : ns(n_ns), constant(this), root(true), pub(true), builtin(true) {
    }

    DLLLOCAL void setModuleName() {
        assert(from_module.empty());
        const char* mod_name = get_module_context_name();
        if (mod_name) {
            from_module = mod_name;
        }
        //printd(5, "qore_ns_private::setModuleName() this: %p mod: %s\n", this, mod_name ? mod_name : "n/a");
    }
};

struct namespace_iterator_element {
    qore_ns_private* ns;
    nsmap_t::iterator i;

    DLLLOCAL namespace_iterator_element(qore_ns_private* ns) : ns(ns) {
        assert(ns);

        i = ns->nsl.nsmap.begin();
    }

    DLLLOCAL bool atEnd() const {
        return i == ns->nsl.nsmap.end();
    }

    DLLLOCAL QoreNamespace* next() {
        ++i;
        if (atEnd()) {
            return nullptr;
        }
        return i->second;
    }
};

class QorePrivateNamespaceIterator {
protected:
    typedef std::vector<namespace_iterator_element> nsv_t;
    nsv_t nsv;             // stack of namespaces
    qore_ns_private* root; // for starting over when done

    DLLLOCAL void set(qore_ns_private* rns) {
        nsv.push_back(namespace_iterator_element(rns));

        //printd(5, "QorePrivateNamespaceIterator::set() %p:%s committed: %d\n", rns, rns->name.c_str(), committed);
        while (!(rns->nsl.empty())) {
            rns = qore_ns_private::get(*((rns->nsl.nsmap.begin()->second)));
            //printd(5, "QorePrivateNamespaceIterator::set() -> %p:%s committed: %d\n", rns, rns->name.c_str(),
            //  committed);
            nsv.push_back(namespace_iterator_element(rns));
        }
    }

public:
    DLLLOCAL QorePrivateNamespaceIterator(qore_ns_private* rns) : root(rns) {
        assert(rns);
    }

    DLLLOCAL bool next() {
        // reset when starting over
        if (nsv.empty()) {
            set(root);
            return true;
        }

        namespace_iterator_element* nie = &(nsv.back());

        // if the last element of the current namespace list has been iterated, take it off the stack
        if (nie->atEnd()) {
            nsv.pop_back();
            if (nsv.empty())
                return false;

            nie = &(nsv.back());
        }

        QoreNamespace* next = nie->next();
        if (next)
            set(qore_ns_private::get(*next));

        return true;
    }

    DLLLOCAL qore_ns_private* operator->() {
        return nsv.back().ns;
    }

    DLLLOCAL qore_ns_private* operator*() {
        return nsv.back().ns;
    }

    DLLLOCAL qore_ns_private* get() {
        return nsv.back().ns;
    }
};

struct NSOInfoBase {
    qore_ns_private* ns = nullptr;

    DLLLOCAL NSOInfoBase() {
    }

    DLLLOCAL NSOInfoBase(qore_ns_private* n_ns) : ns(n_ns) {
    }

    DLLLOCAL unsigned depth() const {
        return ns->depth;
    }
};

template <typename T>
struct NSOInfo : public NSOInfoBase {
    // object
    T* obj = nullptr;

    DLLLOCAL NSOInfo() {
    }

    DLLLOCAL NSOInfo(qore_ns_private* n_ns, T* n_obj) : NSOInfoBase(n_ns), obj(n_obj) {
    }

    DLLLOCAL void assign(qore_ns_private* n_ns, T* n_obj) {
        ns = n_ns;
        obj = n_obj;
    }
};

// cannot use vector_map here for performance reasons
template <typename T>
class RootMap : public std::map<const std::string, NSOInfo<T>> {
private:
    // not implemented
    DLLLOCAL RootMap(const RootMap& old);
    // not implemented
    DLLLOCAL RootMap& operator=(const RootMap& m);

public:
    typedef NSOInfo<T> info_t;
    typedef std::map<const std::string, NSOInfo<T>> map_t;

    DLLLOCAL RootMap() {
    }

    DLLLOCAL void update(const std::string& name, qore_ns_private* ns, T* obj) {
        // get current lookup map entry for this object
        typename map_t::iterator i = this->lower_bound(name);
        if (i == this->end() || i->first != name) {
            this->insert(i, typename map_t::value_type(name, info_t(ns, obj)));
        } else { // if the old depth is > the new depth, then replace
            if (i->second.depth() > ns->depth) {
                i->second.assign(ns, obj);
            }
        }
    }

    DLLLOCAL void update(typename map_t::const_iterator ni) {
        // get current lookup map entry for this object
        typename map_t::iterator i = this->lower_bound(ni->first);
        if (i == this->end() || i->first != ni->first) {
            //printd(5, "RootMap::update(iterator) inserting '%s' new depth: %d\n", ni->first, ni->second.depth());
            this->insert(i, typename map_t::value_type(ni->first, ni->second));
        } else {
            // if the old depth is > the new depth, then replace
            if (i->second.depth() > ni->second.depth()) {
                //printd(5, "RootMap::update(iterator) replacing '%s' current depth: %d new depth: %d\n", ni->first,
                //  i->second.depth(), ni->second.depth());
                i->second = ni->second;
            }
            //else
            //printd(5, "RootMap::update(iterator) ignoring '%s' current depth: %d new depth: %d\n", ni->first,
            //  i->second.depth(), ni->second.depth());
        }
    }

    T* findObj(const std::string& name) {
        typename map_t::iterator i = this->find(name);
        return i == this->end() ? nullptr : i->second.obj;
    }
};

struct FunctionEntryInfo {
    FunctionEntry* obj = nullptr;

    DLLLOCAL FunctionEntryInfo() {
    }

    DLLLOCAL FunctionEntryInfo(FunctionEntry* o) : obj(o) {
    }

    DLLLOCAL unsigned depth() const {
        return getNamespace()->depth;
    }

    DLLLOCAL qore_ns_private* getNamespace() const {
        return obj->getNamespace();
    }

    DLLLOCAL void assign(FunctionEntry* n_obj) {
        obj = n_obj;
    }
};

// cannot use vector_map here for performance reasons
typedef std::map<const char*, FunctionEntryInfo, ltstr> femap_t;
class FunctionEntryRootMap : public femap_t {
private:
    // not implemented
    DLLLOCAL FunctionEntryRootMap(const FunctionEntryRootMap& old);
    // not implemented
    DLLLOCAL FunctionEntryRootMap& operator=(const FunctionEntryRootMap& m);

public:
    DLLLOCAL FunctionEntryRootMap() {
    }

    DLLLOCAL void update(const char* name, FunctionEntry* obj) {
        // get current lookup map entry for this object
        femap_t::iterator i = find(name);
        if (i == end())
            insert(femap_t::value_type(name, FunctionEntryInfo(obj)));
        else // if the old depth is > the new depth, then replace
            if (i->second.depth() > obj->getNamespace()->depth)
                i->second.assign(obj);
    }

    DLLLOCAL void update(femap_t::const_iterator ni) {
        // get current lookup map entry for this object
        femap_t::iterator i = find(ni->first);
        if (i == end()) {
            //printd(5, "FunctionEntryRootMap::update(iterator) inserting '%s' new depth: %d\n", ni->first,
            //  ni->second.depth());
            insert(femap_t::value_type(ni->first, ni->second));
        } else {
            // if the old depth is > the new depth, then replace
            if (i->second.depth() > ni->second.depth()) {
                //printd(5, "FunctionEntryRootMap::update(iterator) replacing '%s' current depth: %d new depth: %d\n",
                //  ni->first, i->second.depth(), ni->second.depth());
                i->second = ni->second;
            }
            //else
            //printd(5, "FunctionEntryRootMap::update(iterator) ignoring '%s' current depth: %d new depth: %d\n",
            //  ni->first, i->second.depth(), ni->second.depth());
        }
    }

    FunctionEntry* findObj(const char* name) {
        femap_t::iterator i = find(name);
        return i == end() ? 0 : i->second.obj;
    }
};

class NamespaceDepthList {
    friend class NamespaceDepthListIterator;
protected:
    // map from depth to namespace
    typedef std::multimap<unsigned, qore_ns_private*> nsdmap_t;
    nsdmap_t nsdmap;

public:
    DLLLOCAL NamespaceDepthList() {
    }

    DLLLOCAL void add(qore_ns_private* ns) {
        nsdmap.insert(nsdmap_t::value_type(ns->depth, ns));
    }

    DLLLOCAL void clear() {
        nsdmap.clear();
    }
};

class NamespaceDepthListIterator {
    NamespaceDepthList::nsdmap_t::iterator i, e;
public:
    DLLLOCAL NamespaceDepthListIterator(NamespaceDepthList& m) : i(m.nsdmap.begin()), e(m.nsdmap.end()) {
    }

    DLLLOCAL bool next() {
        if (i == e)
            return false;
        ++i;
        return i != e;
    }

    DLLLOCAL qore_ns_private* get() const {
        assert(i->second);
        return i->second;
    }
};

class NamespaceMap {
    friend class NamespaceMapIterator;
    friend class ConstNamespaceMapIterator;
    friend class ConstAllNamespacesIterator;

protected:
    // map from depth to namespace
    typedef std::multimap<unsigned, qore_ns_private*> nsdmap_t;
    // map from name to depth map
    //typedef std::map<const char*, nsdmap_t, ltstr> nsmap_t;
    typedef vector_map_t<const char*, nsdmap_t> nsmap_t;
    // map from namespace to depth for reindexing
    //typedef std::map<qore_ns_private*, unsigned> nsrmap_t;
    typedef vector_map_t<qore_ns_private*, unsigned> nsrmap_t;

    nsmap_t nsmap;   // name to depth to namespace map
    nsrmap_t nsrmap; // namespace to depth map (for fast reindexing)

    // not implemented
    DLLLOCAL NamespaceMap(const NamespaceMap& old);
    // not implemented
    DLLLOCAL NamespaceMap& operator=(const NamespaceMap& m);

public:
    DLLLOCAL NamespaceMap() {
    }

    DLLLOCAL void update(qore_ns_private* ns) {
        // if this namespace is already indexed, then reindex
        nsrmap_t::iterator ri = nsrmap.find(ns);
        if (ri != nsrmap.end()) {
            // if the depth is the same, then do nothing
            if (ns->depth == ri->second)
                return;

            // otherwise get the depth -> namespace map under this name
            nsmap_t::iterator i = nsmap.find(ns->name.c_str());
            assert(i != nsmap.end());

            // now get the namespace entry
            nsdmap_t::iterator di = i->second.find(ri->second);
            assert(di != i->second.end());

            // remove from depth -> namespace map
            i->second.erase(di);

            // remove from reverse map
            nsrmap.erase(ri);

            // add new entry to depth -> namespace map
            i->second.insert(nsdmap_t::value_type(ns->depth, ns));

            return;
        }
        else {
            // insert depth -> ns map entry
            nsmap_t::iterator i = nsmap.find(ns->name.c_str());
            if (i == nsmap.end())
                i = nsmap.insert(nsmap_t::value_type(ns->name.c_str(), nsdmap_t())).first;

            i->second.insert(nsdmap_t::value_type(ns->depth, ns));
        }

        // add new entry to reverse map
        nsrmap.insert(nsrmap_t::value_type(ns, ns->depth));
    }

    DLLLOCAL void commit(NamespaceMap& pend) {
        // commit entries
        for (nsrmap_t::iterator i = pend.nsrmap.begin(), e = pend.nsrmap.end(); i != e; ++i)
            update(i->first);
        pend.clear();
    }

    DLLLOCAL void clear() {
        nsmap.clear();
        nsrmap.clear();
    }

    // find the first namespace with the given name
    DLLLOCAL QoreNamespace* findFirst(const char* name) {
        nsmap_t::iterator mi = nsmap.find(name);
        if (mi != nsmap.end()) {
            nsdmap_t::iterator i = mi->second.begin();
            if (i != mi->second.end()) {
                return i->second->ns;
            }
        }
        return nullptr;
    }
};

class NamespaceMapIterator {
protected:
    NamespaceMap::nsmap_t::iterator mi;
    NamespaceMap::nsdmap_t::iterator i;
    bool valid;

public:
    DLLLOCAL NamespaceMapIterator(NamespaceMap& nsm, const char* name) : mi(nsm.nsmap.find(name)),
            valid(mi != nsm.nsmap.end()) {
        if (valid)
            i = mi->second.end();
    }

    DLLLOCAL bool next() {
        if (!valid)
            return false;

        if (i == mi->second.end())
            i = mi->second.begin();
        else
            ++i;

        return i != mi->second.end();
    }

    DLLLOCAL qore_ns_private* get() {
        return i->second;
    }
};

class ConstNamespaceMapIterator {
protected:
    NamespaceMap::nsmap_t::const_iterator mi;
    NamespaceMap::nsdmap_t::const_iterator i;
    bool valid;

public:
    DLLLOCAL ConstNamespaceMapIterator(const NamespaceMap& nsm, const char* name) : mi(nsm.nsmap.find(name)),
            valid(mi != nsm.nsmap.end()) {
        if (valid)
            i = mi->second.end();
    }

    DLLLOCAL bool next() {
        if (!valid)
            return false;

        if (i == mi->second.end())
            i = mi->second.begin();
        else
            ++i;

        return i != mi->second.end();
    }

    DLLLOCAL const qore_ns_private* get() {
        return i->second;
    }
};

class ConstAllNamespacesIterator {
public:
    DLLLOCAL ConstAllNamespacesIterator(const NamespaceMap& nsmap) : nsrmap(nsmap.nsrmap), i(nsmap.nsrmap.end()) {
    }

    DLLLOCAL bool next() {
        if (i == nsrmap.end()) {
            i = nsrmap.begin();
        }
        else {
            ++i;
        }

        return i != nsrmap.end();
    }

    DLLLOCAL const QoreNamespace* get() const {
        assert(i != nsrmap.end());
        return i->first->ns;
    }

private:
    const NamespaceMap::nsrmap_t& nsrmap;
    NamespaceMap::nsrmap_t::const_iterator i;
};

typedef FunctionEntryRootMap fmap_t;

typedef RootMap<ConstantEntry> cnmap_t;

typedef RootMap<QoreClass> clmap_t;

typedef RootMap<TypedHashDecl> thdmap_t;

typedef RootMap<Var> varmap_t;

struct deferred_new_check_t {
    const qore_class_private* qc;
    const QoreProgramLocation* loc;

    DLLLOCAL deferred_new_check_t(const qore_class_private* qc, const QoreProgramLocation* loc) : qc(qc), loc(loc) {
    }
};

class qore_root_ns_private : public qore_ns_private {
    friend class qore_ns_private;
    friend class QoreNamespace;

protected:
    typedef std::vector<deferred_new_check_t> deferred_new_check_vec_t;
    deferred_new_check_vec_t deferred_new_check_vec;

    DLLLOCAL int addPendingVariantIntern(qore_ns_private& ns, const char* name, AbstractQoreFunctionVariant* v) {
        // try to add function variant to given namespace
        bool new_func = false;
        FunctionEntry* fe = ns.addPendingVariantIntern(name, v, new_func);
        if (!fe)
            return -1;

        assert(fe->getNamespace() == &ns);

        if (new_func) {
            fmap_t::iterator i = fmap.find(fe->getName());
            // only add to pending map if either not in the committed map or the depth is higher in the committed map
            if (i == fmap.end() || i->second.depth() > ns.depth)
                pend_fmap.update(fe->getName(), fe);
        }

        return 0;
    }

    DLLLOCAL int addPendingVariantIntern(qore_ns_private& ns, const NamedScope& nscope,
            AbstractQoreFunctionVariant* v) {
        assert(nscope.size() > 1);
        SimpleRefHolder<AbstractQoreFunctionVariant> vh(v);

        QoreNamespace* fns = ns.ns;
        for (unsigned i = 0; i < nscope.size() - 1; ++i) {
            fns = fns->priv->parseFindLocalNamespace(nscope[i]);
            if (!fns) {
                parse_error(*v->getUserVariantBase()->getUserSignature()->getParseLocation(),
                    "cannot find namespace '%s::' in '%s()' as a child of namespace '%s::'", nscope[i], nscope.ostr,
                    ns.name.c_str());
                return -1;
            }
        }

        return addPendingVariantIntern(*fns->priv, nscope.getIdentifier(), vh.release());
    }

    // performed at runtime
    DLLLOCAL int runtimeImportClass(ExceptionSink* xsink, qore_ns_private& ns, const QoreClass* c, QoreProgram* spgm,
            q_setpub_t set_pub, const char* new_name = nullptr, bool inject = false,
            const qore_class_private* injectedClass = nullptr) {
        QoreClass* nc = ns.runtimeImportClass(xsink, c, spgm, set_pub, new_name, inject, injectedClass);
        if (!nc)
            return -1;

        //printd(5, "qore_root_ns_private::runtimeImportClass() this: %p ns: %p '%s' (depth %d) class: %p %s\n", this,
        //  &ns, ns.name.c_str(), ns.depth, nc, nc->getName());

        clmap.update(nc->getName(), &ns, nc);
        return 0;
    }

    // performed at runtime
    DLLLOCAL int runtimeImportHashDecl(ExceptionSink* xsink, qore_ns_private& ns, const TypedHashDecl* hd,
            QoreProgram* spgm, q_setpub_t set_pub, const char* new_name = nullptr) {
        TypedHashDecl* nhd = ns.runtimeImportHashDecl(xsink, hd, spgm, set_pub, new_name);
        if (!nhd)
            return -1;

        //printd(5, "qore_root_ns_private::thdmap() this: %p ns: %p '%s' (depth %d) hashdecl: %p %s\n", this, &ns,
        //  ns.name.c_str(), ns.depth, nc, nc->getName());

        thdmap.update(nhd->getName(), &ns, nhd);
        return 0;
    }

    // performed at runtime
    DLLLOCAL int runtimeImportFunction(ExceptionSink* xsink, qore_ns_private& ns, QoreFunction* u,
            const char* new_name = nullptr, bool inject = false) {
        FunctionEntry* fe = ns.runtimeImportFunction(xsink, u, new_name, inject);
        if (!fe)
            return -1;

        assert(fe->getNamespace() == &ns);

        //printd(5, "qore_root_ns_private::runtimeImportFunction() this: %p ns: %p '%s' (depth %d) func: %p %s\n",
        //  this, &ns, ns.name.c_str(), ns.depth, u, fe->getName());

        fmap.update(fe->getName(), fe);
        return 0;
    }

    DLLLOCAL bool runtimeExistsFunctionIntern(const char* name) {
        return fmap.find(name) != fmap.end();
    }

    DLLLOCAL const QoreClass* runtimeFindClassIntern(const char* name, const qore_ns_private*& ns) const {
        clmap_t::const_iterator i = clmap.find(name);

        if (i != clmap.end()) {
            ns = i->second.ns;
            //printd(5, "qore_root_ns_private::runtimeFindClassIntern() this: %p %s found in ns: '%s' depth: %d\n",
            //  this, name, ns->name.c_str(), ns->depth);
            return i->second.obj;
        }

        return nullptr;
    }

    DLLLOCAL const QoreClass* runtimeFindClassIntern(const NamedScope& name, const qore_ns_private*& ns) const;

    DLLLOCAL const TypedHashDecl* runtimeFindHashDeclIntern(const char* name, const qore_ns_private*& ns) {
        thdmap_t::iterator i = thdmap.find(name);

        if (i != thdmap.end()) {
            ns = i->second.ns;
            //printd(5, "qore_root_ns_private::runtimeFindHashDeclIntern() this: %p %s found in ns: '%s' depth: %d\n",
            //  this, name, ns->name.c_str(), ns->depth);
            return i->second.obj;
        }

        return nullptr;
    }

    DLLLOCAL const FunctionEntry* runtimeFindFunctionEntryIntern(const char* name) {
        fmap_t::const_iterator i = fmap.find(name);
        return i != fmap.end() ? i->second.obj : nullptr;
    }

    DLLLOCAL const FunctionEntry* runtimeFindFunctionEntryIntern(const NamedScope& name);

    DLLLOCAL FunctionEntry* parseFindFunctionEntryIntern(const char* name) {
        {
            // try to check in current namespace first
            qore_ns_private* nscx = parse_get_ns();
            if (nscx) {
                FunctionEntry* fe = nscx->func_list.findNode(name);
                if (fe)
                return fe;
            }
        }

        fmap_t::iterator i = fmap.find(name);
        fmap_t::iterator ip = pend_fmap.find(name);

        if (i != fmap.end()) {
            if (ip != pend_fmap.end()) {
                if (i->second.depth() < ip->second.depth())
                return i->second.obj;

                return ip->second.obj;
            }

            return i->second.obj;
        }

        if (ip != pend_fmap.end())
            return ip->second.obj;

        return 0;
    }

    DLLLOCAL QoreFunction* parseFindFunctionIntern(const char* name) {
        FunctionEntry* fe = parseFindFunctionEntryIntern(name);
        return !fe ? 0 : fe->getFunction();
    }

    DLLLOCAL const FunctionEntry* parseResolveFunctionEntryIntern(const QoreProgramLocation* loc, const char* fname) {
        QORE_TRACE("qore_root_ns_private::parseResolveFunctionEntryIntern()");

        const FunctionEntry* f = parseFindFunctionEntryIntern(fname);
        if (!f) {
            // cannot find function, throw exception
            parse_error(*loc, "function '%s()' cannot be found", fname);
        }

        return f;
    }

    // called during parsing (plock already grabbed)
    DLLLOCAL AbstractCallReferenceNode* parseResolveCallReferenceIntern(UnresolvedProgramCallReferenceNode* fr);

    DLLLOCAL void parseCommit() {
        // commit pending function lookup entries
        for (fmap_t::iterator i = pend_fmap.begin(), e = pend_fmap.end(); i != e; ++i) {
            fmap.update(i);
        }
        pend_fmap.clear();

        qore_ns_private::parseCommit();
        // exceptions can be thrown when performing runtime initialization
        qore_ns_private::parseCommitRuntimeInit(getProgram()->getParseExceptionSink());
    }

    DLLLOCAL ConstantEntry* parseFindOnlyConstantEntryIntern(const char* cname, qore_ns_private*& ns) {
        {
            // first try to look in current namespace context
            qore_ns_private* nscx = parse_get_ns();
            if (nscx) {
                ConstantEntry* ce = nscx->constant.findEntry(cname);
                if (ce) {
                    ns = nscx;
                    return ce;
                }
            }
        }

        // look up in global constant map
        cnmap_t::iterator i = cnmap.find(cname);

        if (i != cnmap.end()) {
            ns = i->second.ns;
            return i->second.obj;;
        }

        return 0;
    }

    DLLLOCAL QoreValue parseFindOnlyConstantValueIntern(const QoreProgramLocation* loc, const char* cname,
            const QoreTypeInfo*& typeInfo, bool& found) {
        assert(!found);
        qore_ns_private* ns;
        ConstantEntry* ce = parseFindOnlyConstantEntryIntern(cname, ns);
        if (!ce)
            return QoreValue();

        //printd(5, "qore_root_ns_private::parseFindOnlyConstantValueIntern() const: %s ns: %p %s\n", cname, ns,
        //  ns->name.c_str());

        found = true;
        NamespaceParseContextHelper nspch(ns);
        return ce->get(loc, typeInfo, ns);
    }

    DLLLOCAL QoreValue parseFindConstantValueIntern(const QoreProgramLocation* loc, const char* cname,
            const QoreTypeInfo*& typeInfo, bool& found, bool error) {
        assert(!found);
        // look up class constants first
        QoreClass* pc = parse_get_class();
        if (pc) {
            QoreValue rv = qore_class_private::parseFindConstantValue(pc, cname, typeInfo, found,
                pc ? qore_class_private::get(*pc) : nullptr);
            if (found) {
                return rv;
            }
        }

        QoreValue rv = parseFindOnlyConstantValueIntern(loc, cname, typeInfo, found);
        if (found) {
            return rv;
        }

        if (error) {
            parse_error(*loc, "constant '%s' cannot be resolved in any namespace", cname);
        }

        return QoreValue();
    }

    DLLLOCAL ResolvedCallReferenceNode* runtimeGetCallReference(const char* fname, ExceptionSink* xsink) {
        fmap_t::iterator i = fmap.find(fname);
        if (i == fmap.end()) {
            xsink->raiseException("NO-SUCH-FUNCTION", "callback function '%s()' does not exist", fname);
            return 0;
        }

        return i->second.obj->makeCallReference(get_runtime_location());
    }

    DLLLOCAL TypedHashDecl* parseFindScopedHashDeclIntern(const NamedScope& nscope, unsigned& matched);

    DLLLOCAL TypedHashDecl* parseFindHashDeclIntern(const char* hdname) {
        {
            // try to check in current namespace first
            qore_ns_private* nscx = parse_get_ns();
            if (nscx) {
                TypedHashDecl* hd = nscx->parseFindLocalHashDecl(hdname);
                if (hd)
                return hd;
            }
        }

        thdmap_t::iterator i = thdmap.find(hdname);

        if (i != thdmap.end()) {
            return i->second.obj;
        }

        //printd(5, "qore_root_ns_private::parseFindHashDeclIntern() this: %p '%s' not found\n", this, cname);
        return nullptr;
    }

    DLLLOCAL QoreClass* parseFindScopedClassIntern(const QoreProgramLocation* loc, const NamedScope& name,
            bool raise_error);
    DLLLOCAL QoreClass* parseFindScopedClassIntern(const NamedScope& name, unsigned& matched);
    DLLLOCAL QoreClass* parseFindScopedClassWithMethodInternError(const QoreProgramLocation* loc,
            const NamedScope& name, bool error);
    DLLLOCAL QoreClass* parseFindScopedClassWithMethodIntern(const NamedScope& name, unsigned& matched);

    DLLLOCAL QoreClass* parseFindClassIntern(const char* cname) {
        assert(cname);
        {
            // try to check in current namespace first
            qore_ns_private* nscx = parse_get_ns();
            if (nscx) {
                QoreClass* qc = nscx->parseFindLocalClass(cname);
                if (qc)
                    return qc;
            }
        }

        clmap_t::iterator i = clmap.find(cname);

        if (i != clmap.end()) {
            return i->second.obj;
        }

        // now check all namespaces with class handlers
        NamespaceDepthListIterator nhi(nshlist);
        while (nhi.next()) {
            QoreClass* qc = nhi.get()->findLoadClass(cname);
            if (qc)
                return qc;
        }

        //printd(5, "qore_root_ns_private::parseFindClassIntern() this: %p '%s' not found\n", this, cname);
        return nullptr;
    }

    DLLLOCAL const QoreClass* runtimeFindClass(const char* name) const {
        clmap_t::const_iterator i = clmap.find(name);
        return i != clmap.end() ? i->second.obj : nullptr;
    }

    DLLLOCAL QoreNamespace* runtimeFindNamespaceForAddFunction(const NamedScope& name, ExceptionSink* xsink) {
        //printd(5, "QoreNamespaceIntern::runtimeFindNamespaceForAddFunction() this: %p name: %s (%s)\n", this,
        //  name.ostr, name[0]);
        bool fnd = false;

        // iterate all namespaces with the initial name and look for the match
        NamespaceMapIterator nmi(nsmap, name[0]);
        while (nmi.next()) {
            const qore_ns_private* rv = nmi.get()->runtimeMatchAddFunction(name, fnd);
            //printd(5, "QoreNamespaceIntern::runtimeFindNamespaceForAddFunction() this: %p name: %s ns: %p '%s' "
            //  "rv: %p fnd: %d\n", this, name.ostr, nmi.get(), nmi.get()->name.c_str(), rv, fnd);
            if (rv)
                return const_cast<QoreNamespace*>(rv->ns);
        }

        if (fnd) {
            xsink->raiseException("FUNCTION-IMPORT-ERROR", "target function '%s' already exists in the given "
                "namespace", name.ostr);
        } else {
            xsink->raiseException("FUNCTION-IMPORT-ERROR", "target namespace in '%s' does not exist", name.ostr);
        }
        return nullptr;
    }

    DLLLOCAL QoreNamespace* runtimeFindNamespaceForAddClass(const NamedScope& name, ExceptionSink* xsink) {
        bool fnd = false;

        // iterate all namespaces with the initial name and look for the match
        NamespaceMapIterator nmi(nsmap, name.get(0));
        while (nmi.next()) {
            const qore_ns_private* rv = nmi.get()->runtimeMatchAddClass(name, fnd);
            if (rv)
                return const_cast<QoreNamespace*>(rv->ns);
        }

        if (fnd) {
            xsink->raiseException("CLASS-IMPORT-ERROR", "target class '%s' already exists in the given namespace",
                name.ostr);
        } else {
            xsink->raiseException("CLASS-IMPORT-ERROR", "target namespace in '%s' does not exist", name.ostr);
        }
        return 0;
    }

    DLLLOCAL void addConstant(qore_ns_private& ns, const char* cname, QoreValue value, const QoreTypeInfo* typeInfo);

    DLLLOCAL QoreValue parseFindReferencedConstantValueIntern(const QoreProgramLocation* loc, const NamedScope& name,
            const QoreTypeInfo*& typeInfo, bool& found, bool error);

    DLLLOCAL QoreValue parseResolveBarewordIntern(const QoreProgramLocation* loc, const char* bword,
            const QoreTypeInfo*& typeInfo, bool& found);

    DLLLOCAL QoreValue parseResolveReferencedScopedReferenceIntern(const QoreProgramLocation* loc,
            const NamedScope& name, const QoreTypeInfo*& typeInfo, bool& found);

    DLLLOCAL void parseAddConstantIntern(const QoreProgramLocation* loc, QoreNamespace& ns, const NamedScope& name,
            QoreValue value, bool pub);

    DLLLOCAL void parseAddClassIntern(const QoreProgramLocation* loc, const NamedScope& name, QoreClass* oc);

    DLLLOCAL void parseAddHashDeclIntern(const QoreProgramLocation* loc, const NamedScope& name, TypedHashDecl* hd);

    DLLLOCAL qore_ns_private* parseResolveNamespaceIntern(const QoreProgramLocation* loc, const NamedScope& nscope,
            qore_ns_private* sns);
    DLLLOCAL qore_ns_private* parseResolveNamespace(const QoreProgramLocation* loc, const NamedScope& nscope,
            qore_ns_private* sns);
    DLLLOCAL qore_ns_private* parseResolveNamespace(const QoreProgramLocation* loc, const NamedScope& nscope);

    DLLLOCAL const FunctionEntry* parseResolveFunctionEntryIntern(const NamedScope& nscope);

    DLLLOCAL Var* parseAddResolvedGlobalVarDefIntern(const QoreProgramLocation* loc, const NamedScope& name,
        const QoreTypeInfo* typeInfo, qore_var_t type = VT_GLOBAL);
    DLLLOCAL Var* parseAddGlobalVarDefIntern(const QoreProgramLocation* loc, const NamedScope& name,
        QoreParseTypeInfo* typeInfo, qore_var_t type = VT_GLOBAL);

    DLLLOCAL Var* parseCheckImplicitGlobalVarIntern(const QoreProgramLocation* loc, const NamedScope& name,
        const QoreTypeInfo* typeInfo);

    DLLLOCAL Var* parseFindGlobalVarIntern(const NamedScope& vname) {
        assert(vname.size() > 1);

        Var* rv = 0;
        unsigned match = 0;

        {
            // try to check in current namespace first
            qore_ns_private* nscx = parse_get_ns();
            if (nscx && nscx->name == vname[0]) {
                QoreNamespace* vns = nscx->parseMatchNamespace(vname, match);
                if (vns && (rv = vns->priv->var_list.parseFindVar(vname.getIdentifier())))
                return rv;
            }
        }

        // iterate all namespaces with the initial name and look for the match
        {
            NamespaceMapIterator nmi(nsmap, vname[0]);
            while (nmi.next()) {
                QoreNamespace* vns = nmi.get()->parseMatchNamespace(vname, match);
                if (vns && (rv = vns->priv->var_list.parseFindVar(vname.getIdentifier())))
                return rv;
            }
        }

        return rv;
    }

    DLLLOCAL Var* parseFindGlobalVarIntern(const char* vname) {
        {
            // try to check in current namespace first
            qore_ns_private* nscx = parse_get_ns();
            if (nscx) {
                Var* v = nscx->var_list.parseFindVar(vname);
                if (v)
                return v;
            }

            //printd(5, "qore_root_ns_private::parseFindGlobalVarIntern() this: %p '%s' nscx: %p ('%s') varmap: %d "
            //  "pend_varmap: %d\n", this, vname, nscx, nscx ? nscx->name.c_str() : "n/a",
            //  varmap.find(vname) != varmap.end(), pend_varmap.find(vname) != pend_varmap.end());
        }

        varmap_t::iterator i = varmap.find(vname);

        if (i != varmap.end()) {
            return i->second.obj;
        }

        return nullptr;
    }

    DLLLOCAL Var* runtimeFindGlobalVar(const NamedScope& nscope, const qore_ns_private*& vns) const;

    DLLLOCAL Var* runtimeFindGlobalVar(const char* vname, const qore_ns_private*& vns) const {
        if (strstr(vname, "::")) {
            NamedScope nscope(vname);
            return runtimeFindGlobalVar(nscope, vns);
        }

        varmap_t::const_iterator i = varmap.find(vname);
        if (i != varmap.end()) {
            assert(i->second.ns);
            vns = i->second.ns;
            return i->second.obj;
        }
        return nullptr;
    }

    DLLLOCAL const ConstantEntry* runtimeFindNamespaceConstant(const NamedScope& nscope,
            const qore_ns_private*& cns) const;

    DLLLOCAL const ConstantEntry* runtimeFindNamespaceConstant(const char* cname, const qore_ns_private*& cns) const {
        if (strstr(cname, "::")) {
            NamedScope nscope(cname);
            return runtimeFindNamespaceConstant(nscope, cns);
        }

        cnmap_t::const_iterator i = cnmap.find(cname);
        if (i != cnmap.end()) {
            assert(i->second.ns);
            cns = i->second.ns;
            return i->second.obj;
        }
        return nullptr;
    }

    DLLLOCAL Var* runtimeCreateVar(qore_ns_private& vns, const char* vname, const QoreTypeInfo* typeInfo,
            bool builtin) {
        Var* v = vns.var_list.runtimeCreateVar(vname, typeInfo, builtin);
        if (v) {
            varmap.update(v->getName(), &vns, v);
        }
        return v;
    }

    DLLLOCAL bool parseResolveGlobalVarsAndClassHierarchiesIntern();

    // returns 0 for success, non-zero for error
    DLLLOCAL int parseAddMethodToClassIntern(const QoreProgramLocation* loc, const NamedScope& name,
            MethodVariantBase* qcmethod, bool static_flag);

    DLLLOCAL static void rebuildConstantIndexes(cnmap_t& cnmap, ConstantList& cl, qore_ns_private* ns) {
        ConstantListIterator cli(cl);
        while (cli.next())
            cnmap.update(cli.getName().c_str(), ns, cli.getEntry());
    }

    DLLLOCAL static void rebuildClassIndexes(clmap_t& clmap, QoreClassList& cl, qore_ns_private* ns) {
        ClassListIterator cli(cl);
        while (cli.next())
            clmap.update(cli.getName(), ns, cli.get());
    }

    DLLLOCAL static void rebuildHashDeclIndexes(thdmap_t& thdmap, HashDeclList& hdl, qore_ns_private* ns) {
        HashDeclListIterator hdli(hdl);
        while (hdli.next())
            thdmap.update(hdli.getName(), ns, hdli.get());
    }

    DLLLOCAL static void rebuildFunctionIndexes(fmap_t& fmap, fl_map_t& flmap, qore_ns_private* ns) {
        for (fl_map_t::iterator i = flmap.begin(), e = flmap.end(); i != e; ++i) {
            assert(i->second->getNamespace() == ns);
            fmap.update(i->first, i->second);
            //printd(5, "qore_root_ns_private::rebuildFunctionIndexes() this: %p ns: %p func %s\n", this, ns,
            //  i->first);
        }
    }

    DLLLOCAL void rebuildIndexes(qore_ns_private* ns) {
        // process function indexes
        rebuildFunctionIndexes(fmap, ns->func_list, ns);

        // process variable indexes
        for (map_var_t::iterator i = ns->var_list.vmap.begin(), e = ns->var_list.vmap.end(); i != e; ++i)
            varmap.update(i->first, ns, i->second);

        // process constant indexes
        rebuildConstantIndexes(cnmap, ns->constant, ns);

        // process class indexes
        rebuildClassIndexes(clmap, ns->classList, ns);

        // process hashdecl indexes
        rebuildHashDeclIndexes(thdmap, ns->hashDeclList, ns);

        // reindex namespace
        nsmap.update(ns);

        // inserts into depth list
        nshlist.add(ns);
    }

    DLLLOCAL void parseRebuildIndexes(qore_ns_private* ns) {
        //printd(5, "qore_root_ns_private::parseRebuildIndexes() this: %p ns: %p (%s) depth %d\n", this, ns,
        //  ns->name.c_str(), ns->depth);

        // process function indexes
        for (fl_map_t::iterator i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
            assert(i->second->getNamespace() == ns);
            pend_fmap.update(i->first, i->second);
        }

        // process variable indexes
        for (map_var_t::iterator i = ns->var_list.vmap.begin(), e = ns->var_list.vmap.end(); i != e; ++i)
            varmap.update(i->first, ns, i->second);

        // process constant indexes
        rebuildConstantIndexes(cnmap, ns->constant, ns);

        // process class indexes
        rebuildClassIndexes(clmap, ns->classList, ns);

        // process hashdecl indexes
        rebuildHashDeclIndexes(thdmap, ns->hashDeclList, ns);

        // reindex namespace
        nsmap.update(ns);
    }

    DLLLOCAL void parseAddNamespaceIntern(QoreNamespace* nns);

public:
    RootQoreNamespace* rns;
    QoreNamespace* qoreNS;
    //! owning program object
    /** this is nullptr for the static system namespace; only set for root namespaces in Program objects
    */
    QoreProgram* pgm = nullptr;

    fmap_t fmap,         // root function map
        pend_fmap;       // root pending function map (only used during parsing)

    cnmap_t cnmap;       // root constant map

    clmap_t clmap;       // root class map

    thdmap_t thdmap;     // root hashdecl map

    varmap_t varmap;     // root variable map

    NamespaceMap nsmap;  // root namespace map

    NamespaceDepthList nshlist; // root namespace with handler map

    // unresolved pending global variable list - only used in the 1st stage of parsing (data read in to tree)
    gvlist_t pend_gvlist;

    DLLLOCAL qore_root_ns_private(RootQoreNamespace* n_rns) : qore_ns_private(n_rns), rns(n_rns), qoreNS(nullptr) {
        assert(root);
        assert(pub);
        // add initial namespace to committed map
        nsmap.update(this);
    }

    DLLLOCAL qore_root_ns_private(const qore_root_ns_private& old, int64 po, QoreProgram* pgm, RootQoreNamespace* ns)
            : qore_ns_private(old, po, ns), pgm(pgm) {
        assert(pgm);
        if ((po & PO_NO_API) == PO_NO_API) {
            // create empty Qore namespace
            qoreNS = new QoreNamespace("Qore");
            nsl.nsmap.insert(nsmap_t::value_type("Qore", qoreNS));
            qoreNS->priv->nsl.nsmap.insert(nsmap_t::value_type("Option", new QoreNamespace("Option")));
        } else
            qoreNS = nsl.find("Qore");
        assert(qoreNS);

        // always set the module public flag to true in the root namespace
        pub = true;

        // rebuild root indexes - only for committed objects
        rebuildAllIndexes();
    }

    DLLLOCAL ~qore_root_ns_private() {
    }

    DLLLOCAL RootQoreNamespace* copy(int64 po, QoreProgram* pgm) {
        RootQoreNamespace* rv = new RootQoreNamespace(nullptr);
        qore_root_ns_private* rpriv = new qore_root_ns_private(*this, po, pgm, rv);
        rv->priv = rv->rpriv = rpriv;
        rpriv->rns = rv;
        return rv;
    }

    DLLLOCAL void rebuildAllIndexes() {
        // clear depth list
        nshlist.clear();

        // rebuild root indexes
        QorePrivateNamespaceIterator qpni(this);
        while (qpni.next())
            rebuildIndexes(qpni.get());
    }

    DLLLOCAL void deferParseCheckAbstractNew(const qore_class_private* qc, const QoreProgramLocation* loc) {
        deferred_new_check_vec.push_back(deferred_new_check_t(qc, loc));
    }

    DLLLOCAL void runtimeImportGlobalVariable(ExceptionSink* xsink, qore_ns_private& tns, Var* v, bool readonly,
            const char* import_as) {
        Var* var = tns.var_list.import(v, xsink, readonly, import_as);
        if (!var) {
            return;
        }
        varmap.update(var->getName(), &tns, var);
    }

    DLLLOCAL QoreNamespace* runtimeFindNamespace(const NamedScope& name) {
        // iterate all namespaces with the initial name and look for the match
        NamespaceMapIterator nmi(nsmap, name[0]);
        while (nmi.next()) {
            const qore_ns_private* rv = nmi.get()->runtimeMatchNamespace(name);
            if (rv) {
                return const_cast<QoreNamespace*>(rv->ns);
            }
        }

        return nullptr;
    }

    DLLLOCAL QoreNamespace* runtimeFindNamespace(const QoreString& name) {
        if (name.bindex("::", 0) != -1) {
            NamedScope scope(name.c_str());
            return runtimeFindNamespace(scope);
        }

        return nsmap.findFirst(name.c_str());
    }

    DLLLOCAL const QoreClass* runtimeFindScopedClassWithMethod(const NamedScope& name) const;
    DLLLOCAL const QoreClass* runtimeFindScopedClassWithMethodIntern(const NamedScope& name) const;

    /*
    DLLLOCAL void deleteClearData(ExceptionSink* xsink) {
    }
    */

    DLLLOCAL qore_ns_private* getQore() {
        return qoreNS->priv;
    }

    DLLLOCAL const qore_ns_private* getQore() const {
        return qoreNS->priv;
    }

    DLLLOCAL QoreHashNode* getGlobalVars() const {
        QoreHashNode* rv = new QoreHashNode(autoTypeInfo);
        qore_ns_private::getGlobalVars(*rv);
        return rv;
    }

    DLLLOCAL void commitModule(QoreModuleContext& qmc) {
        for (unsigned j = 0; j < qmc.mcnl.size(); ++j) {
            ModuleContextNamespaceCommit& mc = qmc.mcnl[j];
            mc.parent->addCommitNamespaceIntern(mc.nns);
        }

        for (unsigned j = 0; j < qmc.mcfl.size(); ++j) {
            ModuleContextFunctionCommit& mc = qmc.mcfl[j];
            mc.parent->addBuiltinVariantIntern(mc.name, mc.v);
        }

        // issue #3461: must rebuild all indexes here or symbols will appear missing
        if (qmc.mcnl.size() || qmc.mcfl.size()) {
            rebuildAllIndexes();
        }
    }

    DLLLOCAL void parseRollback(ExceptionSink* xsink) {
        // roll back pending lookup entries
        pend_fmap.clear();
        cnmap.clear();

        varmap.clear();
        nsmap.clear();

        // roll back pending global variables
        pend_gvlist.clear();

        clmap.clear();
        thdmap.clear();

        // delete any deferred new object checks for classes with abstract members
        deferred_new_check_vec.clear();

        qore_ns_private::parseRollback(xsink);
    }

    DLLLOCAL TypedHashDecl* parseFindHashDecl(const QoreProgramLocation* loc, const NamedScope& name);

    DLLLOCAL const TypedHashDecl* runtimeFindHashDeclIntern(const NamedScope& name, const qore_ns_private*& ns);

    DLLLOCAL QoreNamespace* runtimeFindCreateNamespacePath(const NamedScope& nspath, bool pub, bool user) {
        assert(nspath.size());
        bool is_new = false;
        QoreNamespace* nns = findCreateNamespacePath(nspath, pub, user, is_new);
        if (is_new) // add namespace index
            nsmap.update(nns->priv);
        return nns;
    }

    DLLLOCAL QoreNamespace* runtimeFindCreateNamespacePath(const qore_ns_private& ns, bool user) {
        // get a list of namespaces from after the root (not including the root) to the current
        nslist_t nsl;
        ns.getNsList(nsl);

        printd(5, "qore_root_ns_private::runtimeFindCreateNamespacePath() this: %p ns: '%s'\n", this,
            ns.name.c_str());

        bool is_new = false;
        QoreNamespace* nns = findCreateNamespacePath(nsl, user, is_new);
        assert(ns.name == nns->getName());
        if (is_new) // add namespace index
            nsmap.update(nns->priv);
        return nns;
    }

    DLLLOCAL void runtimeRebuildConstantIndexes(qore_ns_private* ns) {
        rebuildConstantIndexes(cnmap, ns->constant, ns);
    }

    DLLLOCAL void runtimeRebuildClassIndexes(qore_ns_private* ns) {
        rebuildClassIndexes(clmap, ns->classList, ns);
    }

    DLLLOCAL void runtimeRebuildHashDeclIndexes(qore_ns_private* ns) {
        rebuildHashDeclIndexes(thdmap, ns->hashDeclList, ns);
    }

    DLLLOCAL void runtimeRebuildFunctionIndexes(qore_ns_private* ns) {
        rebuildFunctionIndexes(fmap, ns->func_list, ns);
    }

    DLLLOCAL const AbstractQoreFunctionVariant* runtimeFindCall(const char* name, const QoreListNode* params,
            ExceptionSink* xsink);

    DLLLOCAL QoreListNode* runtimeFindCallVariants(const char* name, ExceptionSink* xsink);

    DLLLOCAL int parseInit();

    DLLLOCAL class_vec_t runtimeFindAllClassesRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL hashdecl_vec_t runtimeFindAllHashDeclsRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL func_vec_t runtimeFindAllFunctionsRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL ns_vec_t runtimeFindAllNamespacesRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL gvar_vec_t runtimeFindAllGlobalVarsRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL const_vec_t runtimeFindAllNamespaceConstantsRegex(const QoreString& pattern, int re_opts,
            ExceptionSink* xsink) const;

    DLLLOCAL static QoreHashNode* getGlobalVars(RootQoreNamespace& rns) {
        return rns.rpriv->getGlobalVars();
    }

    DLLLOCAL static void runtimeImportSystemClasses(RootQoreNamespace& rns, const RootQoreNamespace& source,
            ExceptionSink* xsink) {
        rns.priv->runtimeImportSystemClasses(*source.priv, *rns.rpriv, xsink);
    }

    DLLLOCAL static void runtimeImportSystemHashDecls(RootQoreNamespace& rns, const RootQoreNamespace& source,
            ExceptionSink* xsink) {
        rns.priv->runtimeImportSystemHashDecls(*source.priv, *rns.rpriv, xsink);
    }

    DLLLOCAL static void runtimeImportSystemConstants(RootQoreNamespace& rns, const RootQoreNamespace& source,
            ExceptionSink* xsink) {
        rns.priv->runtimeImportSystemConstants(*source.priv, *rns.rpriv, xsink);
    }

    DLLLOCAL static void runtimeImportSystemFunctions(RootQoreNamespace& rns, const RootQoreNamespace& source,
            ExceptionSink* xsink) {
        rns.priv->runtimeImportSystemFunctions(*source.priv, *rns.rpriv, xsink);
    }

    DLLLOCAL static QoreNamespace* runtimeFindCreateNamespacePath(const RootQoreNamespace& rns,
            const qore_ns_private& ns, bool user) {
        return rns.rpriv->runtimeFindCreateNamespacePath(ns, user);
    }

    DLLLOCAL static QoreNamespace* runtimeFindCreateNamespacePath(const RootQoreNamespace& rns,
            const NamedScope& nspath, bool pub, bool user) {
        return rns.rpriv->runtimeFindCreateNamespacePath(nspath, pub, user);
    }

    DLLLOCAL static RootQoreNamespace* copy(const RootQoreNamespace& rns, int64 po, QoreProgram* pgm) {
        return rns.rpriv->copy(po, pgm);
    }

    DLLLOCAL static int addPendingVariant(qore_ns_private& nsp, const char* name, AbstractQoreFunctionVariant* v) {
        return getRootNS()->rpriv->addPendingVariantIntern(nsp, name, v);
    }

    DLLLOCAL static int addPendingVariant(qore_ns_private& nsp, const NamedScope& name,
            AbstractQoreFunctionVariant* v) {
        return getRootNS()->rpriv->addPendingVariantIntern(nsp, name, v);
    }

    DLLLOCAL static int runtimeImportFunction(RootQoreNamespace& rns, ExceptionSink* xsink, QoreNamespace& ns,
            QoreFunction* u, const char* new_name = nullptr, bool inject = false) {
        return rns.rpriv->runtimeImportFunction(xsink, *ns.priv, u, new_name, inject);
    }

    DLLLOCAL static int runtimeImportClass(RootQoreNamespace& rns, ExceptionSink* xsink, QoreNamespace& ns,
            const QoreClass* c, QoreProgram* spgm, q_setpub_t set_pub, const char* new_name = 0, bool inject = false,
            const qore_class_private* injectedClass = nullptr) {
        return rns.rpriv->runtimeImportClass(xsink, *ns.priv, c, spgm, set_pub, new_name, inject, injectedClass);
    }

    DLLLOCAL static int runtimeImportHashDecl(RootQoreNamespace& rns, ExceptionSink* xsink, QoreNamespace& ns,
            const TypedHashDecl* c, QoreProgram* spgm, q_setpub_t set_pub, const char* new_name = nullptr) {
        return rns.rpriv->runtimeImportHashDecl(xsink, *ns.priv, c, spgm, set_pub, new_name);
    }

    DLLLOCAL static const QoreClass* runtimeFindClass(RootQoreNamespace& rns, const char* name,
            const qore_ns_private*& ns) {
        if (strstr(name, "::")) {
            NamedScope nscope(name);
            return rns.rpriv->runtimeFindClassIntern(nscope, ns);
        }
        return rns.rpriv->runtimeFindClassIntern(name, ns);
    }

    DLLLOCAL static const TypedHashDecl* runtimeFindHashDecl(RootQoreNamespace& rns, const char* name,
            const qore_ns_private*& ns) {
        if (strstr(name, "::")) {
            NamedScope nscope(name);
            return rns.rpriv->runtimeFindHashDeclIntern(nscope, ns);
        }
        return rns.rpriv->runtimeFindHashDeclIntern(name, ns);
    }

    DLLLOCAL static const QoreFunction* runtimeFindFunction(RootQoreNamespace& rns, const char* name,
            const qore_ns_private*& ns) {
        const FunctionEntry* fe = runtimeFindFunctionEntry(rns, name);
        if (fe) {
            ns = fe->getNamespace();
            return fe->getFunction();
        }
        return nullptr;
    }

    DLLLOCAL static const FunctionEntry* runtimeFindFunctionEntry(RootQoreNamespace& rns, const char* name) {
        if (strstr(name, "::")) {
            NamedScope nscope(name);
            return rns.rpriv->runtimeFindFunctionEntryIntern(nscope);
        }
        return rns.rpriv->runtimeFindFunctionEntryIntern(name);
    }

    DLLLOCAL static bool runtimeExistsFunction(RootQoreNamespace& rns, const char* name) {
        return rns.rpriv->runtimeExistsFunctionIntern(name);
    }

    DLLLOCAL static void addConstant(qore_root_ns_private& rns, qore_ns_private& ns, const char* cname,
            QoreValue value, const QoreTypeInfo* typeInfo) {
        rns.addConstant(ns, cname, value, typeInfo);
    }

    DLLLOCAL static const QoreFunction* parseResolveFunction(const QoreProgramLocation* loc, const char* fname) {
        const FunctionEntry* fe = getRootNS()->rpriv->parseResolveFunctionEntryIntern(loc, fname);
        return fe ? fe->getFunction() : nullptr;
    }

    DLLLOCAL static const FunctionEntry* parseResolveFunctionEntry(const QoreProgramLocation* loc, const char* fname) {
        return getRootNS()->rpriv->parseResolveFunctionEntryIntern(loc, fname);
    }

    // called during parsing (plock already grabbed)
    DLLLOCAL static AbstractCallReferenceNode* parseResolveCallReference(UnresolvedProgramCallReferenceNode* fr) {
        return getRootNS()->rpriv->parseResolveCallReferenceIntern(fr);
    }

    DLLLOCAL static bool parseResolveGlobalVarsAndClassHierarchies() {
        return getRootNS()->rpriv->parseResolveGlobalVarsAndClassHierarchiesIntern();
    }

    DLLLOCAL static void parseCommit(RootQoreNamespace& rns) {
        rns.rpriv->parseCommit();
    }

    DLLLOCAL static QoreValue parseFindConstantValue(const QoreProgramLocation* loc, const char* name,
            const QoreTypeInfo*& typeInfo, bool& found, bool error) {
        found = false;
        return getRootNS()->rpriv->parseFindConstantValueIntern(loc, name, typeInfo, found, error);
    }

    DLLLOCAL static QoreValue parseFindReferencedConstantValue(const QoreProgramLocation* loc, const NamedScope& name,
            const QoreTypeInfo*& typeInfo, bool& found, bool error) {
        found = false;
        return getRootNS()->rpriv->parseFindReferencedConstantValueIntern(loc, name, typeInfo, found, error);
    }

    DLLLOCAL static QoreValue parseResolveBareword(const QoreProgramLocation* loc, const char* bword,
            const QoreTypeInfo*& typeInfo, bool& found) {
        found = false;
        return getRootNS()->rpriv->parseResolveBarewordIntern(loc, bword, typeInfo, found);
    }

    DLLLOCAL static QoreValue parseResolveReferencedScopedReference(const QoreProgramLocation* loc,
            const NamedScope& name, const QoreTypeInfo*& typeInfo, bool& found) {
        found = false;
        return getRootNS()->rpriv->parseResolveReferencedScopedReferenceIntern(loc, name, typeInfo, found);
    }

    DLLLOCAL static QoreClass* parseFindClass(const QoreProgramLocation* loc, const char* name,
            bool raise_error = true) {
        QoreClass* qc = getRootNS()->rpriv->parseFindClassIntern(name);
        if (!qc && raise_error) {
            parse_error(*loc, "reference to undefined class '%s'", name);
        }
        return qc;
    }

    DLLLOCAL static QoreClass* parseFindScopedClass(const QoreProgramLocation* loc, const NamedScope& name,
            bool raise_error = true) {
        return getRootNS()->rpriv->parseFindScopedClassIntern(loc, name, raise_error);
    }

    DLLLOCAL static QoreClass* parseFindScopedClassWithMethod(const QoreProgramLocation* loc, const NamedScope& name,
            bool error) {
        return getRootNS()->rpriv->parseFindScopedClassWithMethodInternError(loc, name, error);
    }

    DLLLOCAL static void parseAddConstant(const QoreProgramLocation* loc, QoreNamespace& ns, const NamedScope& name,
            QoreValue value, bool pub) {
        getRootNS()->rpriv->parseAddConstantIntern(loc, ns, name, value, pub);
    }

    // returns 0 for success, non-zero for error
    DLLLOCAL static int parseAddMethodToClass(const QoreProgramLocation* loc, const NamedScope& name,
            MethodVariantBase* qcmethod, bool static_flag) {
        return getRootNS()->rpriv->parseAddMethodToClassIntern(loc, name, qcmethod, static_flag);
    }

    DLLLOCAL static void parseAddClass(const QoreProgramLocation* loc, const NamedScope& name, QoreClass* oc) {
        getRootNS()->rpriv->parseAddClassIntern(loc, name, oc);
    }

    DLLLOCAL static void parseAddHashDecl(const QoreProgramLocation* loc, const NamedScope& name, TypedHashDecl* hd) {
        getRootNS()->rpriv->parseAddHashDeclIntern(loc, name, hd);
    }

    DLLLOCAL static void parseAddNamespace(QoreNamespace* nns) {
        getRootNS()->rpriv->parseAddNamespaceIntern(nns);
    }

    DLLLOCAL static const QoreFunction* parseResolveFunction(const NamedScope& nscope) {
        const FunctionEntry* fe = getRootNS()->rpriv->parseResolveFunctionEntryIntern(nscope);
        return fe ? fe->getFunction() : nullptr;
    }

    DLLLOCAL static const FunctionEntry* parseResolveFunctionEntry(const NamedScope& nscope) {
        return getRootNS()->rpriv->parseResolveFunctionEntryIntern(nscope);
    }

    DLLLOCAL const QoreClass* runtimeFindScopedClass(const NamedScope& name) const;

    DLLLOCAL static ResolvedCallReferenceNode* runtimeGetCallReference(RootQoreNamespace& rns, const char* name,
            ExceptionSink* xsink) {
        return rns.rpriv->runtimeGetCallReference(name, xsink);
    }

    DLLLOCAL static Var* parseAddResolvedGlobalVarDef(const QoreProgramLocation* loc, const NamedScope& vname,
            const QoreTypeInfo* typeInfo, qore_var_t type = VT_GLOBAL) {
        return getRootNS()->rpriv->parseAddResolvedGlobalVarDefIntern(loc, vname, typeInfo, type);
    }

    DLLLOCAL static Var* parseAddGlobalVarDef(const QoreProgramLocation* loc, const NamedScope& vname,
            QoreParseTypeInfo* typeInfo, qore_var_t type = VT_GLOBAL) {
        return getRootNS()->rpriv->parseAddGlobalVarDefIntern(loc, vname, typeInfo, type);
    }

    DLLLOCAL static Var* parseCheckImplicitGlobalVar(const QoreProgramLocation* loc, const NamedScope& name,
            const QoreTypeInfo* typeInfo) {
        return getRootNS()->rpriv->parseCheckImplicitGlobalVarIntern(loc, name, typeInfo);
    }

    DLLLOCAL static Var* parseFindGlobalVar(const char* vname) {
        return getRootNS()->rpriv->parseFindGlobalVarIntern(vname);
    }

    DLLLOCAL static Var* parseFindGlobalVar(const NamedScope& nscope) {
        return getRootNS()->rpriv->parseFindGlobalVarIntern(nscope);
    }

    DLLLOCAL static void scanMergeCommittedNamespace(const RootQoreNamespace& ns, const RootQoreNamespace& mns, QoreModuleContext& qmc) {
        ns.priv->scanMergeCommittedNamespace(*(mns.priv), qmc);
    }

    DLLLOCAL static void copyMergeCommittedNamespace(RootQoreNamespace& ns, const RootQoreNamespace& mns) {
        ns.priv->copyMergeCommittedNamespace(*(mns.priv));

        // rebuild root indexes - only for committed objects
        ns.rpriv->rebuildAllIndexes();
    }

    DLLLOCAL static Var* runtimeFindGlobalVar(const RootQoreNamespace& rns, const char* vname, const qore_ns_private*& vns) {
        return rns.rpriv->runtimeFindGlobalVar(vname, vns);
    }

    DLLLOCAL static Var* runtimeCreateVar(RootQoreNamespace& rns, QoreNamespace& vns, const char* vname, const QoreTypeInfo* typeInfo, bool builtin = false) {
        return rns.rpriv->runtimeCreateVar(*vns.priv, vname, typeInfo, builtin);
    }

    /*
    DLLLOCAL static void runtimeModuleRebuildIndexes(RootQoreNamespace& rns) {
        // rebuild root indexes
        QorePrivateNamespaceIterator qpni(rns.priv, true);
        while (qpni.next())
            rns.rpriv->rebuildIndexes(qpni.get());
    }
    */

    DLLLOCAL static const QoreClass* runtimeFindClass(RootQoreNamespace& rns, const char* name) {
        return rns.rpriv->runtimeFindClass(name);
    }

    DLLLOCAL static const ConstantEntry* runtimeFindNamespaceConstant(const RootQoreNamespace& rns, const char* cname, const qore_ns_private*& cns) {
        return rns.rpriv->runtimeFindNamespaceConstant(cname, cns);
    }

    DLLLOCAL static QoreNamespace* runtimeFindNamespaceForAddFunction(RootQoreNamespace& rns, const NamedScope& name, ExceptionSink* xsink) {
        return rns.rpriv->runtimeFindNamespaceForAddFunction(name, xsink);
    }

    DLLLOCAL static QoreNamespace* runtimeFindNamespaceForAddClass(RootQoreNamespace& rns, const NamedScope& name, ExceptionSink* xsink) {
        return rns.rpriv->runtimeFindNamespaceForAddClass(name, xsink);
    }

    DLLLOCAL static qore_root_ns_private* get(RootQoreNamespace& rns) {
        return rns.rpriv;
    }

    DLLLOCAL static const qore_root_ns_private* get(const RootQoreNamespace& rns) {
        return rns.rpriv;
    }

    DLLLOCAL static qore_ns_private* getQore(RootQoreNamespace& rns) {
        return rns.rpriv->qoreNS->priv;
    }

    DLLLOCAL static const qore_ns_private* getQore(const RootQoreNamespace& rns) {
        return rns.rpriv->qoreNS->priv;
    }

    DLLLOCAL static void clearConstants(RootQoreNamespace& ns, QoreListNode& l) {
        ns.priv->clearConstants(l);
        ns.rpriv->cnmap.clear();
    }

    DLLLOCAL static void clearData(RootQoreNamespace& ns, ExceptionSink* xsink) {
        ns.priv->clearData(xsink);
    }
};

#endif
