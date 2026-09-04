/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ModuleInfo.h

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

#ifndef _QORE_MODULEINFO_H

#define _QORE_MODULEINFO_H

#ifdef NEED_DLFCN_WRAPPER
extern "C" {
#endif
#include <dlfcn.h>
#ifdef NEED_DLFCN_WRAPPER
}
#endif

#include <string>
#include <map>
#include <deque>
#include <memory>
#include <vector>

// parse options set while parsing the module's header (init & del)
#define MOD_HEADER_PO (PO_LOCKDOWN & ~(PO_NO_MODULES | PO_NO_REFLECTION))

// initial user module parse options
#define USER_MOD_PO (PO_NO_TOP_LEVEL_STATEMENTS | PO_REQUIRE_PROTOTYPES | PO_REQUIRE_OUR | PO_IN_MODULE)

// module load options
#define QMLO_NONE            0
#define QMLO_INJECT          (1 << 0)
#define QMLO_REINJECT        (1 << 1)
#define QMLO_PRIVATE         (1 << 2)
#define QMLO_RELOAD          (1 << 3)
#define QMLO_FROM_PARSE      (1 << 4)

//! list of version numbers in order of importance (i.e. 1.2.3 = 1, 2, 3)
struct version_list_t : public std::vector<int> {
protected:
    QoreString ver;

public:
    DLLLOCAL version_list_t() {
    }

    DLLLOCAL version_list_t(const char* v) {
        set(v);
    }

    DLLLOCAL char set(const char* v);

    DLLLOCAL version_list_t& operator=(const char* v) {
        set(v);
        return *this;
    }

    DLLLOCAL const char* operator*() const {
        return ver.c_str();
    }
};

//! attach status of a child module declared with the %try-child-module parse directive
/** @see design/qore-module-structure.md "Child Modules"
*/
enum ChildModuleStatus : unsigned char {
    CMS_PENDING = 0,    //!< the attach has not been attempted yet
    CMS_ATTACHED,       //!< the child was loaded, or its load was already in progress in this thread
    CMS_ABSENT,         //!< no module with this name could be found in the module path
    CMS_SKIPPED,        //!< not attempted; module loading is not allowed in the parent module's Program
    CMS_FAILED,         //!< the child is present but could not be loaded; the parent is loaded without it
};

//! a child module declared with the %try-child-module parse directive
struct ChildModuleInfo {
    //! the declaration as given; a feature name with an optional version constraint
    std::string spec;
    //! the child's feature name
    std::string name;
    //! the current attach status
    ChildModuleStatus status = CMS_PENDING;
    //! the error code of the child's own load exception; set only with CMS_FAILED
    /** the failure is reported when it happens and is not raised to the parent's caller, so this and \a desc
        are how a broken child is diagnosed afterwards through the module hash
    */
    std::string err;
    //! the description of the child's own load exception with CMS_FAILED, the reason with CMS_SKIPPED
    std::string desc;

    DLLLOCAL ChildModuleInfo(const char* spec, const char* name) : spec(spec), name(name) {
    }
};

typedef std::vector<ChildModuleInfo> child_mod_vec_t;

//! returns the string corresponding to the given child module status
DLLLOCAL const char* qore_child_module_status_string(ChildModuleStatus status);

//! records a child module declaration made with the %try-child-module parse directive
/** called from the scanner; raises a parse exception if the directive is used outside a user module, if the
    module declares itself as a child, or if the same child is declared twice

    @param loc the location of the directive
    @param spec the module specification; a feature name with an optional version constraint
*/
DLLLOCAL void qore_declare_child_module(const QoreProgramLocation* loc, const char* spec);

//! returns the feature name in the given module specification (i.e. without any version constraint)
DLLLOCAL void qore_get_module_spec_name(const char* spec, QoreString& name);

class QoreAbstractModule {
    friend class QoreModuleManager;
    friend class ChildAttachHelper;

public:
    version_list_t version_list;
    // list of dependent modules to reexport
    name_vec_t rmod;

    // for binary modules
    DLLLOCAL QoreAbstractModule(const char* cwd, const char* fn, const char* n, const char* d,
            const char* v, const char* a, const char* u, const QoreString& l, unsigned load_opt) :
            filename(fn), name(n), desc(d), author(a), url(u), license(l), path(fn) ,
            priv(load_opt & QMLO_PRIVATE), injected(load_opt & QMLO_INJECT), reinjected(load_opt & QMLO_REINJECT),
            init_failed(false), version_list(v) {
        q_normalize_path(filename, cwd);
    }

    // for user modules
    DLLLOCAL QoreAbstractModule(const char* cwd, const char* fn, const char* n, unsigned load_opt,
            const char* path = nullptr) :
            filename(fn), name(n), path(path ? path : fn), priv(load_opt & QMLO_PRIVATE),
            injected(load_opt & QMLO_INJECT),
            reinjected(load_opt & QMLO_REINJECT), init_failed(false) {
        q_normalize_path(filename, cwd);
    }

    DLLLOCAL virtual ~QoreAbstractModule();

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const char* getFileName() const {
        return filename.c_str();
    }

    DLLLOCAL const QoreString& getFileNameStr() const {
        return filename;
    }

    DLLLOCAL const char* getDesc() const {
        return desc.c_str();
    }

    DLLLOCAL const char* getVersion() const {
        return* version_list;
    }

    DLLLOCAL const char* getURL() const {
        return url.c_str();
    }

    DLLLOCAL const char* getOrigName() const {
        return orig_name.empty() ? nullptr : orig_name.c_str();
    }

    DLLLOCAL void resetName() {
        assert(!orig_name.empty());
        name = orig_name;
        orig_name.clear();
    }

    DLLLOCAL bool isInjected() const {
        return injected;
    }

    DLLLOCAL bool isReInjected() const {
        return reinjected;
    }

    DLLLOCAL bool isInitFailed() const {
        return init_failed;
    }

    DLLLOCAL void setInitFailed() {
        init_failed = true;
    }

    DLLLOCAL void addModuleReExport(const char* m) {
        rmod.push_back(m);
    }

    //! sets the child modules declared by this module with the %try-child-module parse directive
    /** @param specs the child module specifications in declaration order

        duplicate declarations are ignored; the module manager mutex must be held when calling this method
    */
    DLLLOCAL void setChildModules(const std::vector<std::string>& specs);

    //! returns true if this module declares child modules with the %try-child-module parse directive
    /** the module manager mutex must be held when calling this method
    */
    DLLLOCAL bool hasChildModules() const {
        return !children.empty();
    }

    DLLLOCAL void reexport(ExceptionSink& xsink, QoreProgram* pgm) const;

    DLLLOCAL void addToProgram(QoreProgram* pgm, ExceptionSink& xsink) const {
        addToProgramImpl(pgm, xsink);
        if (!xsink)
            reexport(xsink, pgm);
    }

    DLLLOCAL bool equalTo(const QoreAbstractModule* m) const {
        assert(name == m->name);
        return filename == m->filename;
    }

    DLLLOCAL bool isPath(const char* p) const {
        return filename == p;
    }

    DLLLOCAL void rename(const QoreString& n) {
        assert(orig_name.empty());
        name = n;
    }

    DLLLOCAL void setOrigName(const char* n) {
        assert(orig_name.empty());
        orig_name = n;
    }

    DLLLOCAL bool isPrivate() const {
        return priv;
    }

    DLLLOCAL void setPrivate(bool p = true) {
        assert(priv != p);
        priv = p;
    }

    DLLLOCAL void setLink(QoreAbstractModule* n) {
        //printd(5, "AbstractQoreModule::setLink() n: %p '%s'\n", n, n->getName());
        assert(!next);
        assert(!n->prev);
        next = n;
        n->prev = this;
    }

    DLLLOCAL QoreAbstractModule* getNext() const {
        return next;
    }

    DLLLOCAL virtual bool isBuiltin() const = 0;
    DLLLOCAL virtual bool isUser() const = 0;
    DLLLOCAL virtual QoreHashNode* getHash(bool with_filename = true) const = 0;
    DLLLOCAL virtual void issueModuleCmd(const QoreProgramLocation* loc, const QoreString& cmd, ExceptionSink* xsink)
        = 0;

protected:
    QoreString filename,
        name,
        desc,
        author,
        url,
        license,
        path,
        orig_name;

    // link to associated modules (originals with reinjection, etc)
    QoreAbstractModule* prev = nullptr,
        * next = nullptr;

    // child modules declared with %try-child-module in declaration order; all access to this vector and to
    // children_done is serialized by the module manager mutex
    child_mod_vec_t children;

    // true when every child declaration has reached a terminal status (attached, absent, or failed); false
    // while any child is still pending or was skipped, in which case the attach is retried on the next load
    bool children_done = false;

    // true while a thread is attaching this module's children; other threads wait for it to complete so
    // that a module load only returns once its children have been registered
    bool children_attaching = false;

    bool priv : 1,
        injected : 1,
        reinjected : 1,
        init_failed : 1;

    DLLLOCAL QoreHashNode* getHashIntern(bool with_filename = true) const;

    DLLLOCAL virtual void addToProgramImpl(QoreProgram* tpgm, ExceptionSink& xsink) const = 0;

    DLLLOCAL void set(const char* d, const char* v, const char* a, const char* u, const QoreString& l) {
        desc = d;
        author = a;
        url = u;
        license = l;
        version_list = v;
    }

private:
    // not implemented
    QoreAbstractModule(const QoreAbstractModule&) = delete;
    QoreAbstractModule& operator=(const QoreAbstractModule&) = delete;
};

// list/dequeue of strings
typedef std::deque<std::string> strdeque_t;

//! non-thread-safe unique list of strings of directory names
/** a deque should require fewer memory allocations compared to a linked list.
    the set is used for uniqueness
 */
class UniqueDirectoryList {
protected:
    typedef std::set<std::string> strset_t;

    strdeque_t dlist;
    strset_t dset;

public:
    DLLLOCAL void addDirList(const char* str);

    DLLLOCAL bool push_back(const std::string &str) {
        if (dset.find(str) != dset.end()) {
            return false;
        }
        dlist.push_back(str);
        dset.insert(str);
        return true;
    }

    DLLLOCAL bool empty() const {
        return dlist.empty();
    }

    DLLLOCAL strdeque_t::const_iterator begin() const {
        return dlist.begin();
    }

    DLLLOCAL strdeque_t::const_iterator end() const {
        return dlist.end();
    }

    DLLLOCAL void appendPath(QoreString& str) const {
        if (dlist.empty()) {
            str.concat("<empty>");
            return;
        }
        for (strdeque_t::const_iterator i = dlist.begin(), e = dlist.end(); i != e; ++i) {
            str.concat((*i).c_str());
            str.concat(':');
        }
        str.terminate(str.size() - 1);
    }
};

class QoreModuleContextHelper : public QoreModuleContext {
public:
    DLLLOCAL QoreModuleContextHelper(const char* name, QoreProgram* pgm, ExceptionSink& xsink);
    DLLLOCAL ~QoreModuleContextHelper();
};

class QoreModuleDefContextHelper : public QoreModuleDefContext {
protected:
    QoreModuleDefContext* old;

public:
    DLLLOCAL QoreModuleDefContextHelper() : old(set_module_def_context(this)) {
    }

    DLLLOCAL ~QoreModuleDefContextHelper() {
        set_module_def_context(old);
    }
};

class QoreUserModuleDefContextHelper;
class QoreUserModule;

typedef std::set<std::string> strset_t;
typedef std::map<std::string, strset_t> md_map_t;

class ModMap {
private:
    DLLLOCAL ModMap(const ModMap &);
    DLLLOCAL ModMap& operator=(const ModMap&);

protected:
    md_map_t map;

public:
    DLLLOCAL ModMap() {
    }

    DLLLOCAL ~ModMap() {
    }

    DLLLOCAL bool addDep(const char* l, const char* r) {
        md_map_t::iterator i = map.lower_bound(l);
        if (i == map.end() || i->first != l) {
            i = map.insert(i, md_map_t::value_type(l, strset_t()));
        } else if (i->second.find(r) != i->second.end()) {
            return true;
        }
        i->second.insert(r);
        return false;
    }

    DLLLOCAL md_map_t::iterator begin() {
        return map.begin();
    }

    DLLLOCAL md_map_t::iterator end() {
        return map.end();
    }

    DLLLOCAL md_map_t::iterator find(const char* n) {
        return map.find(n);
    }

    //! returns true if \a r is a direct dependent of \a l
    DLLLOCAL bool hasDep(const std::string& l, const std::string& r) const {
        md_map_t::const_iterator i = map.find(l);
        return i != map.end() && i->second.find(r) != i->second.end();
    }

    //! returns the dependents of \a l, or nullptr if there are none
    DLLLOCAL const strset_t* getDeps(const std::string& l) const {
        md_map_t::const_iterator i = map.find(l);
        return i == map.end() ? nullptr : &i->second;
    }

    DLLLOCAL md_map_t::iterator find(const std::string& n) {
        return map.find(n);
    }

    DLLLOCAL void erase(md_map_t::iterator i) {
        map.erase(i);
    }

    DLLLOCAL void clear() {
        map.clear();
    }

    DLLLOCAL bool empty() const {
        return map.empty();
    }

#ifdef DEBUG
    DLLLOCAL void show(const char* name) {
        printf("ModMap '%s':\n", name);
        for (md_map_t::iterator i = map.begin(), e = map.end(); i != e; ++i) {
            QoreString str("[");
            for (strset_t::iterator si = i->second.begin(), se = i->second.end(); si != se; ++si)
                str.sprintf("'%s',", (*si).c_str());
            str.concat("]");

            printd(0, " + %s '%s' -> %s\n", name, i->first.c_str(), str.c_str());
        }
    }
#endif
};

struct DLHelper {
    void* ptr;

    DLLLOCAL DLHelper(void* p) : ptr(p) {
    }

    DLLLOCAL ~DLHelper() {
        if (ptr)
            dlclose(ptr);
    }

    DLLLOCAL void* release() {
        void* rv = ptr;
        ptr = nullptr;
        return rv;
    }
};

class ModuleLoadMapHelper;

class QoreModuleManager {
    friend class QoreAbstractModule;
    friend class ModuleLoadMapHelper;
    friend class ChildAttachHelper;

public:
    DLLLOCAL QoreModuleManager() {
    }

    DLLLOCAL ~QoreModuleManager() {
    }

    DLLLOCAL void init(bool se);
    DLLLOCAL void delUser();
    DLLLOCAL void cleanup();

    DLLLOCAL void issueParseCmd(const QoreProgramLocation* loc, const char* mname, const QoreString& cmd);

    DLLLOCAL int issueRuntimeCmd(const char* mname, QoreProgram* pgm, const QoreString& cmd, ExceptionSink* xsink);

    DLLLOCAL void addModule(QoreAbstractModule* m) {
        assert(map.find(m->getName()) == map.end());
        map.insert(module_map_t::value_type(m->getName(), m));
        //printd(5, "QoreModuleManager::addModule() m: %p '%s'\n", m, m->getName());
    }

    DLLLOCAL QoreAbstractModule* findModule(const char* name) {
        AutoLocker al(mutex);
        return findModuleUnlocked(name);
    }

    //! Load a reexported module into a program using the current module-load locking context.
    DLLLOCAL void loadModuleForReexport(ExceptionSink& xsink, const char* name, QoreProgram* pgm);

    //! find a module by name without locking; the caller must hold the mutex
    DLLLOCAL QoreAbstractModule* findModuleUnlocked(const char* name) {
        module_map_t::iterator i = map.find(name);
        return i == map.end() ? nullptr : i->second;
    }

    //! Import a module's namespace into a program without acquiring the lock
    /** This is for use by AOT runtime code that is called from within a locked context
        (e.g., qore_aot_module_init called from loadBinaryModuleFromDesc).
        @param name the module name to import
        @param pgm the target QoreProgram
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int importModuleNSUnlocked(const char* name, QoreProgram* pgm, ExceptionSink& xsink);

    //! Import a module's namespace into a program (locked version)
    /** @param name the module feature name
        @param pgm the target QoreProgram
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int importModuleNS(const char* name, QoreProgram* pgm, ExceptionSink& xsink) {
        AutoLocker al(mutex);
        return importModuleNSUnlocked(name, pgm, xsink);
    }

    DLLLOCAL int parseLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name, QoreProgram* pgm,
            bool reexport = false);
    DLLLOCAL int runTimeLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name, QoreProgram* pgm,
            QoreProgram* mpgm = nullptr, unsigned load_opt = QMLO_NONE, int warning_mask = QP_WARN_MODULES,
            bool reexport = false, qore_binary_module_desc_t mod_desc_func = nullptr);

    //! Ensure a dependency provider is loaded globally without importing its namespace into a Program
    /** @param path_pgm supplies module search paths, parse options, and sandbox context for a cold provider load
        @return 0 on success, -1 on error
    */
    DLLLOCAL int loadProviderModule(ExceptionSink& xsink, const char* name, QoreProgram* path_pgm);

    //! Worker for ModuleManager::registerAOTStaticModule — no dlopen, skip filesystem search
    DLLLOCAL int registerAOTStaticModuleIntern(ExceptionSink& xsink, QoreProgram* tpgm,
            qore_binary_module_desc_t desc_fn, const char* path);

    DLLLOCAL QoreHashNode* getModuleHash();
    DLLLOCAL QoreListNode* getModuleList();

    DLLLOCAL void addModuleDir(const char* dir) {
        AutoLocker al(mutex);
        moduleDirList.push_back(dir);
    }

    DLLLOCAL void addModuleDirList(const char* strlist) {
        AutoLocker al(mutex);
        moduleDirList.addDirList(strlist);
    }

    DLLLOCAL void addStandardModulePaths();

    DLLLOCAL void registerUserModuleFromSource(const char* name, const char* src, QoreProgram* pgm,
            ExceptionSink& xsink);

    DLLLOCAL void trySetUserModuleDependency(const QoreAbstractModule* mi) {
        if (!mi->isUser())
            return;

        const char* old_name = get_module_context_name();
        if (old_name) {
            // Only track dependency if the dependent module is also a user module.
            // Binary modules (including AOT modules) are not cleaned up via delUser(),
            // so tracking their dependencies would cause assertion failures.
            QoreAbstractModule* dep_mi = findModuleUnlocked(old_name);
            if (dep_mi && dep_mi->isUser()) {
                setUserModuleDependency(mi->getName(), old_name);
            }
        }
        trySetUserModule(mi->getName());
    }

    DLLLOCAL void trySetUserModule(const char* name) {
        md_map_t::iterator i = md_map.find(name);
        if (i == md_map.end()) {
            umset.insert(name);
            //printd(5, "QoreModuleManager::trySetUserModule('%s') UMSET SET: rmd_map: empty\n", name);
        }
#ifdef DEBUG
        /*
        else {
            QoreString str("[");
            for (strset_t::iterator si = i->second.begin(), se = i->second.end(); si != se; ++si)
                str.sprintf("'%s',", (*si).c_str());
            str.concat("]");
            //printd(5, "QoreModuleManager::trySetUserModule('%s') UMSET NOT SET: md_map: %s\n", name, str.c_str());
        }
        */
#endif
    }

    DLLLOCAL void setUserModuleDependency(const char* name, const char* dep) {
        //printd(5, "QoreModuleManager::setUserModuleDependency('%s' -> '%s')\n", name, dep);
        if (md_map.addDep(name, dep))
            return;
        rmd_map.addDep(dep, name);

        strset_t::iterator ui = umset.find(name);
        if (ui != umset.end()) {
            umset.erase(ui);
            //printd(5, "QoreModuleManager::setUserModuleDependency('%s' -> '%s') REMOVED '%s' FROM UMMSET\n", name,
            //    dep, name);
        }
    }

    DLLLOCAL void removeUserModuleDependency(const char* name, const char* orig_name = 0) {
        //printd(5, "QoreModuleManager::removeUserModuleDependency() name: '%s' orig: '%s'\n", name,
        //    orig_name ? orig_name : "n/a");
        md_map_t::iterator i = rmd_map.find(name);
        if (i == rmd_map.end() && orig_name)
            i = rmd_map.find(orig_name);
        if (i != rmd_map.end()) {
            // remove dependents
            for (strset_t::iterator si = i->second.begin(), se = i->second.end(); si != se; ++si) {
                md_map_t::iterator di = md_map.find(*si);
                assert(di != md_map.end());

                strset_t::iterator dsi = di->second.find(i->first);
                assert(dsi != di->second.end());
                di->second.erase(dsi);
                if (di->second.empty()) {
                    //printd(5, "QoreModuleManager::removeUserModuleDependency('%s') '%s' now empty, ADDING TO "
                    //    "UMMSET: '%s'\n", name, i->first.c_str(), (*si).c_str());
                    //md_map.erase(di);
                    assert(umset.find(*si) == umset.end());
                    umset.insert(*si);
                }
            }
            // remove from dep map
            rmd_map.erase(i);
        }

        i = md_map.find(name);
        if (i != md_map.end())
            md_map.erase(i);
        if (orig_name) {
            i = md_map.find(orig_name);
            if (i != md_map.end())
                md_map.erase(i);
        }
    }

    DLLLOCAL int addModuleToBlacklist(const char* name, const char* msg);

    //! Returns true if the module dependency map already has a path from \a from to \a to
    /** A path from \a from to \a to means that \a to must be deleted before \a from; adding the reverse edge
        would make a cycle, which module teardown (delUser()) cannot resolve.  The mutex must be held when
        calling this method.
    */
    DLLLOCAL bool hasUserModuleDependencyPath(const std::string& from, const std::string& to);

    //! Attaches the child modules declared by \a mi with the %try-child-module parse directive
    /** Must be called with the mutex held and only after \a mi has been registered in the module map; the
        mutex is released and reacquired while each child is loaded.  See
        design/qore-module-structure.md "Child Modules" for the contract implemented here.

        A child declaration is optional, so no child status fails the parent; a child that is present but
        cannot be loaded is reported and skipped.

        @param mi the parent module
        @param xsink exception sink; used only for interruptions of the attach itself
        @param wsink warning sink for children that could not be attached
        @param warning_mask the warning mask in effect for the load

        @return 0 for OK (including for absent, skipped, and failed children), -1 if the attach was
        interrupted
    */
    DLLLOCAL int attachChildModules(QoreAbstractModule& mi, ExceptionSink& xsink, ExceptionSink& wsink,
            int warning_mask);

    //! Queues \a mi for child module attachment at the outermost module load boundary in this thread
    /** Must be called with the mutex held; if no module load encloses the current one, the queue is drained
        immediately.

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int queueChildModules(QoreAbstractModule& mi, ExceptionSink& xsink, ExceptionSink& wsink,
            int warning_mask);

private:
    // not implemented
    DLLLOCAL QoreModuleManager(const QoreModuleManager&) = delete;
    // not implemented
    DLLLOCAL QoreModuleManager& operator=(const QoreModuleManager&) = delete;

    // per-feature load/init state; absence from module_load_map encodes NOT_LOADED
    enum ModuleLoadState : unsigned char {
        MLS_INITIALIZING,   // owner_tid is running init; other threads wait or detect a cycle
        MLS_LOADED,         // init completed without error (terminal)
        MLS_FAILED          // init aborted with an exception (terminal); err/desc carry it
    };
    struct ModuleLoadEntry {
        int owner_tid;                          // the single writer running this feature's init
        ModuleLoadState state = MLS_INITIALIZING;
        std::string err;                        // valid iff state == MLS_FAILED
        std::string desc;                       // valid iff state == MLS_FAILED
        unsigned waiters = 0;                   // cross-thread waiters currently parked on this entry

        DLLLOCAL ModuleLoadEntry(int tid) : owner_tid(tid) {}
    };
    typedef std::map<std::string, ModuleLoadEntry> module_load_map_t;
    QoreCondition module_load_cond;
    // map feature names to per-feature load state when module initialization is in progress
    module_load_map_t module_load_map;
    // number of threads waiting on module_load_cond
    int module_load_waiting = 0;
    // wait-for graph for cross-thread module-load cycle detection: waiter TID -> owner TID it blocks on
    std::map<int, int> module_wait_for;

    // Registers a wait-for edge (caller_tid -> owner_tid) and walks the graph; if it leads back to the
    // caller, raises CIRCULAR-MODULE-DEPENDENCY and returns true (caller must NOT wait).  Otherwise
    // commits the edge and returns false.  Must be called with mutex held.
    DLLLOCAL bool checkModuleLoadCycle(const char* feature, int owner_tid, ExceptionSink& xsink);
    // Removes any wait-for edge owned by the given TID.  Must be called with mutex held.
    DLLLOCAL void clearModuleLoadWaitEdge(int tid);

protected:
    // mutex for atomicity
    QoreThreadLock mutex;

    // user module dependency map: module -> dependents
    ModMap md_map;
    // user module dependent map: dependent -> module
    ModMap rmd_map;

    // module blacklist — name and reason are stored as std::string so the map owns
    // its own copies; callers pass through ephemeral buffers (TempEncodingHelper
    // c_str()) that are freed once the helper destructs, so holding raw const char*
    // here would dangle once the add function returns.
    typedef std::map<std::string, std::string, std::less<>> bl_map_t;
    bl_map_t mod_blacklist;

    // module hash
    typedef std::map<const char*, QoreAbstractModule*, ltstr> module_map_t;
    module_map_t map;

    // set of user modules with no dependencies
    strset_t umset;

    // list of module directories
    UniqueDirectoryList moduleDirList;

    DLLLOCAL QoreAbstractModule* loadModuleIntern(const char* name, QoreProgram* pgm, ExceptionSink& xsink) {
        AutoLocker sl(mutex); // make sure checking and loading are atomic

        return loadModuleIntern(xsink, xsink, name, pgm);
    }

    //! Loads a module; the mutex must be held when calling this method
    /** @param path_pgm the Program to use for parse option and module search path inheritance when no target
        Program is given in \a pgm; used when attaching child modules, which are loaded without a target
        Program but must resolve against the parent module's search path
        @param not_found if not nullptr, set to true if the module could not be found in the module path; used
        to distinguish an absent child module from a broken one
    */
    DLLLOCAL QoreAbstractModule* loadModuleIntern(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
            QoreProgram* pgm, bool reexport = false, mod_op_e op = MOD_OP_NONE, version_list_t* version = nullptr,
            const char* src = nullptr, QoreProgram* mpgm = nullptr, unsigned load_opt = QMLO_NONE,
            int warning_mask = QP_WARN_MODULES, qore_binary_module_desc_t mod_desc_func = nullptr,
            QoreProgram* path_pgm = nullptr, bool* not_found = nullptr);

    DLLLOCAL QoreAbstractModule* loadBinaryModuleFromPath(ExceptionSink& xsink, const char* path,
            const char* feature = nullptr, bool reexport = false, QoreProgram* mpgm = nullptr,
            QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE,
            qore_binary_module_desc_t mod_desc = nullptr);

    DLLLOCAL QoreAbstractModule* loadBinaryModuleFromDesc(ExceptionSink& xsink, DLHelper* dlh,
            QoreModuleInfo& mod_info, const char* path, const char* feature = nullptr, bool reexport = false,
            QoreProgram* mpgm = nullptr, QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE,
            ModuleLoadMapHelper* load_guard = nullptr);

    //! Load AOT dependency providers while the parent module owns a load-map reservation.
    DLLLOCAL int loadAOTBinaryModuleDependencies(ExceptionSink& xsink,
            const std::vector<std::string>& dependencies, QoreProgram* path_pgm);

    DLLLOCAL QoreAbstractModule* loadUserModuleFromPath(ExceptionSink& xsink, ExceptionSink& wsink, const char* path,
            const char* feature = nullptr, QoreProgram* tpgm = nullptr, bool reexport = false,
            QoreProgram* mpgm = nullptr, QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE,
            int warning_mask = QP_WARN_MODULES);

    DLLLOCAL QoreAbstractModule* loadUserModuleFromSource(ExceptionSink& xsink, ExceptionSink& wsink,
            const char* path, const char* feature, QoreProgram* tpgm, const char* src, bool reexport,
            QoreProgram* mpgm = nullptr, int warning_mask = QP_WARN_MODULES);

    //! loads separated module. see #2966
    DLLLOCAL QoreAbstractModule* loadSeparatedModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* path,
            const char* feature, QoreProgram* tpgm, bool reexport = false, QoreProgram* mpgm = nullptr,
            QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE, int warning_mask = QP_WARN_MODULES);

    DLLLOCAL QoreAbstractModule* setupUserModule(ExceptionSink& xsink, std::unique_ptr<QoreUserModule>& mi,
            QoreUserModuleDefContextHelper& qmd, unsigned load_opt = QMLO_NONE, int warning_mask = QP_WARN_MODULES);

    DLLLOCAL void reinjectModule(QoreAbstractModule* mi);
    DLLLOCAL void delOrig(QoreAbstractModule* mi);
    DLLLOCAL void getUniqueName(QoreString& nname, const char* name, const char* prefix);

    DLLLOCAL int checkBlacklist(ExceptionSink& xsink, const char* name);
};

DLLLOCAL extern QoreModuleManager QMM;

class QoreBuiltinModule : public QoreAbstractModule {
public:
    //! Construct from QoreModuleInfo (API 2.0)
    DLLLOCAL QoreBuiltinModule(const char* cwd, const char* path, QoreModuleInfo& mod_info, const void* dlptr,
            QoreHashNode* info = nullptr, unsigned load_opt = QMLO_NONE)
            : QoreAbstractModule(cwd, path, mod_info.name.c_str(), mod_info.desc.c_str(),
                mod_info.version.c_str(), mod_info.author.c_str(), mod_info.url.c_str(),
                mod_info.license_str, load_opt),
              api_major(mod_info.api_major), api_minor(mod_info.api_minor),
              module_init(mod_info.init), module_ns_init(mod_info.ns_init),
              module_delete(mod_info.del), module_parse_cmd(mod_info.parse_cmd),
              info(info), dlptr(dlptr) {
    }

    DLLLOCAL virtual ~QoreBuiltinModule() {
        printd(5, "QoreBuiltinModule::~QoreBuiltinModule() '%s': %s calling module_delete: %p\n", name.c_str(),
            filename.c_str(), module_delete);
        // Set the module context name so module_delete() can identify which module is being unloaded
        const char* old_ctx_name = set_module_context_name(name.c_str());
        module_delete();
        set_module_context_name(old_ctx_name);
        // we do not close binary modules because we may have thread local data that needs to be
        // destroyed when exit() is called
    }

    DLLLOCAL unsigned getAPIMajor() const {
        return api_major;
    }

    DLLLOCAL unsigned getAPIMinor() const {
        return api_minor;
    }

    DLLLOCAL virtual bool isBuiltin() const override {
        return true;
    }

    DLLLOCAL virtual bool isUser() const override {
        return false;
    }

    DLLLOCAL QoreHashNode* getHash(bool with_filename = true) const override;

    DLLLOCAL const void* getPtr() const {
        return dlptr;
    }

    DLLLOCAL virtual void issueModuleCmd(const QoreProgramLocation* loc, const QoreString& cmd, ExceptionSink* xsink)
        override;

protected:
    unsigned api_major, api_minor;
    qore_module_init_t module_init;
    qore_module_ns_init_t module_ns_init;
    qore_module_delete_t module_delete;
    qore_module_parse_cmd_t module_parse_cmd;
    QoreHashNode* info;
    const void* dlptr;

    DLLLOCAL virtual void addToProgramImpl(QoreProgram* pgm, ExceptionSink& xsink) const override;
};

class QoreUserModule : public QoreAbstractModule {
public:
    DLLLOCAL QoreUserModule(QoreProgram* p, const char* cwd, const char* fn, const char* n, unsigned load_opt,
            int warning_mask = QP_WARN_MODULES, const char* path = nullptr)
            : QoreAbstractModule(cwd, fn, n, load_opt, path), pgm(p) {
        //printd(5, "QoreUserModule::QoreUserModule() this: %p name: %s\n", this, name.c_str());
    }

    DLLLOCAL virtual ~QoreUserModule();

    DLLLOCAL QoreProgram* getProgram() const {
        return pgm;
    }

    DLLLOCAL void set(const char* d, const char* v, const char* a, const char* u, const QoreString& l,
            AbstractQoreNode* dl) {
        QoreAbstractModule::set(d, v, a, u, l);
        del = dl;
    }

    DLLLOCAL void setInitClosure(QoreValue v) {
        init_c.discard(nullptr);
        init_c = v;
    }

    DLLLOCAL QoreValue refInitClosure() const {
        return init_c.refSelf();
    }

    DLLLOCAL virtual bool isBuiltin() const override {
        return false;
    }

    DLLLOCAL virtual bool isUser() const override {
        return true;
    }

    DLLLOCAL virtual QoreHashNode* getHash(bool with_filename = true) const override {
        return getHashIntern(with_filename);
    }

    DLLLOCAL virtual void issueModuleCmd(const QoreProgramLocation* loc, const QoreString& cmd,
            ExceptionSink* xsink) override {
        if (xsink) {
            xsink->raiseException(*loc, "PARSE-COMMAND-ERROR", "module '%s' loaded from '%s' is a user module; only "
                "builtin modules can support parse commands", name.c_str(), filename.c_str());
        }
    }

    //! Run the module deletion callback if it has not yet been called
    /** Safe to call multiple times; only runs the callback once.
        Called by QoreModuleManager::delUser() Phase 0 to ensure del callbacks
        execute while all module programs (and their TypeInfos) are still alive.
    */
    DLLLOCAL void runDelCallback(ExceptionSink& xsink);

protected:
    //! Module logic / namespace container
    QoreProgram* pgm;

    QoreValue init_c{}; // retained for AOT compilation of embedded local modules
    AbstractQoreNode* del = nullptr; // deletion closure / call reference

    DLLLOCAL virtual void addToProgramImpl(QoreProgram* pgm, ExceptionSink& xsink) const override;
};

class QoreModuleNameContextHelper {
public:
    DLLLOCAL QoreModuleNameContextHelper(const char* name) : old_name(set_module_context_name(name)) {
    }

    DLLLOCAL ~QoreModuleNameContextHelper() {
        set_module_context_name(old_name);
    }

protected:
    const char* old_name;
};

class QoreUserModuleDefContextHelper : public QoreModuleDefContextHelper {
public:
    DLLLOCAL QoreUserModuleDefContextHelper(const char* name, const char* path, QoreProgram* p, ExceptionSink& xs);

    DLLLOCAL ~QoreUserModuleDefContextHelper() {
        const char* name = set_module_context_name(old_name);
        set_module_context_path(old_path);

        if (xsink && !dup) {
            QMM.removeUserModuleDependency(name);
        }
    }

    DLLLOCAL void setDuplicate() {
        assert(!dup);
        dup = true;
    }

    DLLLOCAL void setNameInit(const char* name);

    DLLLOCAL void close();

protected:
    const char* old_name;
    const char* old_path;

    qore_program_private* pgm;
    QoreParseOptions po;

    ExceptionSink& xsink;
    bool dup;
};

//! marks a module as having its child modules attached in the current thread
/** the module manager mutex must be held for the lifetime of this object except while a child module is
    being loaded; on destruction any thread waiting for the attach to complete is woken
*/
class ChildAttachHelper {
public:
    DLLLOCAL ChildAttachHelper(QoreAbstractModule& mi, const std::string& name);
    DLLLOCAL ~ChildAttachHelper();

private:
    QoreAbstractModule& mi;
    const std::string name;

    // not implemented
    ChildAttachHelper(const ChildAttachHelper&) = delete;
    ChildAttachHelper& operator=(const ChildAttachHelper&) = delete;
};

class ModuleLoadMapHelper {
public:
    // reserves \a feature in module_load_map as MLS_INITIALIZING(this thread); on destruction the
    // reservation transitions to MLS_FAILED (capturing the pending exception) if \a xsink holds an
    // exception, otherwise MLS_LOADED.  Every failure path in the module loader sets \a xsink before
    // the reservation is released, so the terminal state is inferred rather than signalled explicitly.
    DLLLOCAL ModuleLoadMapHelper(const char* feature, ExceptionSink& xsink, bool unlock_now = true);
    DLLLOCAL ~ModuleLoadMapHelper();

    DLLLOCAL void unlock();
    DLLLOCAL void lock();

private:
    QoreModuleManager::module_load_map_t::iterator i;
    ExceptionSink& xsink;
    bool unlocked;
};

#endif
