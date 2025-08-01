/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ModuleInfo.h

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

class QoreAbstractModule {
public:
    version_list_t version_list;
    // list of dependent modules to reexport
    name_vec_t rmod;

    // for binary modules
    DLLLOCAL QoreAbstractModule(const char* cwd, const char* fn, const char* n, const char* d,
            const char* v, const char* a, const char* u, const QoreString& l, unsigned load_opt) :
            filename(fn), name(n), desc(d), author(a), url(u), license(l),
            priv(load_opt & QMLO_PRIVATE), injected(load_opt & QMLO_INJECT), reinjected(load_opt & QMLO_REINJECT),
            version_list(v) {
        q_normalize_path(filename, cwd);
    }

    // for user modules
    DLLLOCAL QoreAbstractModule(const char* cwd, const char* fn, const char* n, unsigned load_opt) :
            filename(fn), name(n), priv(load_opt & QMLO_PRIVATE), injected(load_opt & QMLO_INJECT),
            reinjected(load_opt & QMLO_REINJECT) {
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

    DLLLOCAL void addModuleReExport(const char* m) {
        rmod.push_back(m);
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
        orig_name;

    // link to associated modules (originals with reinjection, etc)
    QoreAbstractModule* prev = nullptr,
        * next = nullptr;

    bool priv : 1,
        injected : 1,
        reinjected : 1;

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

class QoreModuleManager {
    friend class QoreAbstractModule;
    friend class ModuleLoadMapHelper;

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

    DLLLOCAL int parseLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name, QoreProgram* pgm,
            bool reexport = false);
    DLLLOCAL int runTimeLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name, QoreProgram* pgm,
            QoreProgram* mpgm = nullptr, unsigned load_opt = QMLO_NONE, int warning_mask = QP_WARN_MODULES,
            bool reexport = false, qore_binary_module_desc_t mod_desc_func = nullptr);

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
        if (old_name)
            setUserModuleDependency(mi->getName(), old_name);
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

private:
    // not implemented
    DLLLOCAL QoreModuleManager(const QoreModuleManager&) = delete;
    // not implemented
    DLLLOCAL QoreModuleManager& operator=(const QoreModuleManager&) = delete;

    typedef std::map<std::string, int> module_load_map_t;
    QoreCondition module_load_cond;
    // map feature names to TIDs when module initialization is in progress
    module_load_map_t module_load_map;
    // number of threads waiting on module_load_set
    int module_load_waiting = 0;

protected:
    // mutex for atomicity
    QoreThreadLock mutex;

    // user module dependency map: module -> dependents
    ModMap md_map;
    // user module dependent map: dependent -> module
    ModMap rmd_map;

    // module blacklist
    typedef std::map<const char*, const char*, ltstr> bl_map_t;
    bl_map_t mod_blacklist;

    // module hash
    typedef std::map<const char*, QoreAbstractModule*, ltstr> module_map_t;
    module_map_t map;

    // set of user modules with no dependencies
    strset_t umset;

    // list of module directories
    UniqueDirectoryList moduleDirList;

    DLLLOCAL QoreAbstractModule* findModuleUnlocked(const char* name) {
        module_map_t::iterator i = map.find(name);
        return i == map.end() ? 0 : i->second;
    }

    DLLLOCAL QoreAbstractModule* loadModuleIntern(const char* name, QoreProgram* pgm, ExceptionSink& xsink) {
        AutoLocker sl(mutex); // make sure checking and loading are atomic

        return loadModuleIntern(xsink, xsink, name, pgm);
    }

    DLLLOCAL QoreAbstractModule* loadModuleIntern(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
            QoreProgram* pgm, bool reexport = false, mod_op_e op = MOD_OP_NONE, version_list_t* version = nullptr,
            const char* src = nullptr, QoreProgram* mpgm = nullptr, unsigned load_opt = QMLO_NONE,
            int warning_mask = QP_WARN_MODULES, qore_binary_module_desc_t mod_desc_func = nullptr);

    DLLLOCAL QoreAbstractModule* loadBinaryModuleFromPath(ExceptionSink& xsink, const char* path,
            const char* feature = nullptr, bool reexport = false, QoreProgram* mpgm = nullptr,
            QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE,
            qore_binary_module_desc_t mod_desc = nullptr);

    DLLLOCAL QoreAbstractModule* loadBinaryModuleFromDesc(ExceptionSink& xsink, DLHelper* dlh,
            QoreModuleInfo& mod_info, const char* path, const char* feature = nullptr, bool reexport = false,
            QoreProgram* mpgm = nullptr, QoreProgram* path_pgm = nullptr, unsigned load_opt = QMLO_NONE);

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
    DLLLOCAL QoreBuiltinModule(const char* cwd, const char* fn, const char* n, const char* d,
            const char* v, const char* a, const char* u, const QoreString& l, unsigned major, unsigned minor,
            qore_module_init_t init, qore_module_ns_init_t ns_init, qore_module_delete_t del,
            qore_module_parse_cmd_t pcmd, const void* p, QoreHashNode* info = nullptr, unsigned load_opt = QMLO_NONE)
            : QoreAbstractModule(cwd, fn, n, d, v, a, u, l, load_opt), api_major(major), api_minor(minor),
                module_init(init), module_ns_init(ns_init), module_delete(del), module_parse_cmd(pcmd), info(info),
                dlptr(p) {
    }

    DLLLOCAL virtual ~QoreBuiltinModule() {
        printd(5, "QoreBuiltinModule::~QoreBuiltinModule() '%s': %s calling module_delete: %p\n", name.c_str(),
            filename.c_str(), module_delete);
        module_delete();
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
            int warning_mask = QP_WARN_MODULES) : QoreAbstractModule(cwd, fn, n, load_opt), pgm(p) {
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

protected:
    //! Module logic / namespace container
    QoreProgram* pgm;

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
    int64 po;

    ExceptionSink& xsink;
    bool dup;
};

class ModuleLoadMapHelper {
public:
    DLLLOCAL ModuleLoadMapHelper(const char* feature);
    DLLLOCAL ~ModuleLoadMapHelper();

private:
    QoreModuleManager::module_load_map_t::iterator i;
};

#endif
