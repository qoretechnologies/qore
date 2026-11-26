/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FunctionList.h

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

#ifndef _QORE_FUNCTIONLIST_H

#define _QORE_FUNCTIONLIST_H

#include <qore/common.h>

#include "qore/intern/Function.h"
#include "qore/intern/DeclSeq.h"

#include <map>
#include <string>
#include <atomic>

class qore_ns_private;

//  function calls are handled with FunctionCallNode
class FunctionEntry : public QoreReferenceCounter {
   friend class FunctionList;

private:
    // not implemented
    FunctionEntry(const FunctionEntry& old) = delete;

protected:
    //! the function; atomic, as a Program's entry can be given a copy of a shared function at run time while other
    //! threads call through it (replaceFunction(), qore#7588)
    std::atomic<QoreFunction*> func;
    std::string name;
    qore_ns_private* ns;
    //! true if the function is shared with a namespace that no Program owns (the static system namespace or the
    //! namespace of a module), directly or through the namespace that it was copied from (qore#7540)
    bool shared_unowned = false;
    //! true if the function is shared with another entry: this entry is a copy of it, or it was copied by reference
    //! (qore#7588); atomic, as the entry of a Program is copied for its child Programs under its read lock
    std::atomic<bool> shares_function{false};
    //! the order in which the function was declared; a copy has the number of the entry that it is a copy of (issue #7171)
    uint64_t decl_seq = qore_next_decl_seq();

    DLLLOCAL ~FunctionEntry() {
        fn()->deref();
    }

    //! returns the function
    DLLLOCAL QoreFunction* fn() const {
        return func.load(std::memory_order_acquire);
    }

public:
    DLLLOCAL FunctionEntry(QoreFunction* u, qore_ns_private* ns) : func(u), ns(ns) {
    }

    DLLLOCAL FunctionEntry(const char* new_name, QoreFunction* u, qore_ns_private* ns)
        : func(u), name(new_name), ns(ns) {
    }

    DLLLOCAL void ref() {
        ROreference();
    }

    DLLLOCAL bool deref() {
        if (ROdereference()) {
            delete this;
            return true;
        }
        return false;
    }

    //! returns the number that gives the order in which the function was declared
    DLLLOCAL uint64_t getDeclSeq() const {
        return decl_seq;
    }

    //! sets the number of the entry that this entry is a copy of
    DLLLOCAL void setDeclSeq(uint64_t seq) {
        decl_seq = seq;
    }

    DLLLOCAL qore_ns_private* getNamespace() const {
        return ns;
    }

    //! Returns true if the function is shared with a namespace that no Program owns (qore#7540)
    DLLLOCAL bool isSharedUnowned() const {
        return shared_unowned;
    }

    //! Records whether the function is shared with a namespace that no Program owns
    DLLLOCAL void setSharedUnowned(bool shared) {
        shared_unowned = shared;
    }

    //! Returns true if the function is shared with another entry (qore#7588)
    DLLLOCAL bool sharesFunction() const {
        return shared_unowned || shares_function.load(std::memory_order_acquire);
    }

    //! Records that the function is shared with another entry
    DLLLOCAL void setSharesFunction() {
        shares_function.store(true, std::memory_order_release);
    }

    DLLLOCAL QoreFunction* getFunction() const {
        return fn();
    }

    //! Gives the entry its own copy of its function, which it shares with another entry (sharesFunction()), so that a
    //! builtin variant added at run time to the function of a Program's namespace is the Program's only (qore#7588)
    /** Called under the runtime namespace write lock; a thread calling through the entry meanwhile uses the function
        that it read, which the entry that it is shared with keeps.  Native code compiled before (JIT, AOT) keeps
        calling the function that it was compiled with, which does not have the variant; calls resolved through the
        entry when they run see it
    */
    DLLLOCAL void replaceFunction(QoreFunction* nf) {
        assert(sharesFunction());
        QoreFunction* old = func.exchange(nf, std::memory_order_acq_rel);
        shared_unowned = false;
        shares_function.store(false, std::memory_order_release);
        old->deref();
    }

    DLLLOCAL QoreFunction* getFunction(bool runtime) const {
        QoreFunction* f = fn();
        if (runtime && f->committedEmpty())
            return nullptr;
        return f;
    }

    DLLLOCAL const char* getName() const {
        return name.empty() ? fn()->getName() : name.c_str();
    }

    DLLLOCAL int parseInit() {
        return fn()->parseInit(ns);
    }

    DLLLOCAL void parseCommit() {
        fn()->parseCommit();
    }

    // returns -1 if the entry can be deleted
    DLLLOCAL int parseRollback() {
        // if there are no committed variants, then return -1 to erase the function entry entirely
        if (fn()->committedEmpty())
            return -1;

        // otherwise just roll back the pending variants
        fn()->parseRollback();
        return 0;
    }

    DLLLOCAL ResolvedCallReferenceNode* makeCallReference(const QoreProgramLocation* loc) const;

    DLLLOCAL bool isPublic() const {
        return fn()->hasPublic();
    }

    DLLLOCAL bool isUserPublic() const {
        return fn()->hasUserPublic();
    }

    DLLLOCAL bool hasBuiltin() const {
        return fn()->hasBuiltin();
    }

    DLLLOCAL void updateNs(qore_ns_private* ns) {
        this->ns = ns;
    }
};

class ModuleImportedFunctionEntry : public FunctionEntry {
public:
    DLLLOCAL ModuleImportedFunctionEntry(const FunctionEntry& old, qore_ns_private* ns, bool builtin = false);
};

#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, FunctionEntry*, qore_hash_str, eqstr> fl_map_t;
#else
typedef std::map<const char*, FunctionEntry*> fl_map_t;
#endif

class FunctionList : public fl_map_t {
public:
    DLLLOCAL FunctionList() {
    }

    DLLLOCAL FunctionList(const FunctionList& old, qore_ns_private* ns, const QoreParseOptions& po);

    //! returns the entries in the order in which their functions were declared (issue #7171)
    DLLLOCAL std::vector<FunctionEntry*> getEntriesInDeclarationOrder() const;

    DLLLOCAL ~FunctionList() {
        del();
    }

    DLLLOCAL FunctionEntry* add(QoreFunction* func, qore_ns_private* ns);
    DLLLOCAL FunctionEntry* import(QoreFunction* func, qore_ns_private* ns);
    DLLLOCAL FunctionEntry* import(const char* new_name, QoreFunction* func, qore_ns_private* ns, bool inject);
    DLLLOCAL QoreFunction* find(const char* name, bool runtime) const;
    DLLLOCAL FunctionEntry* findNode(const char* name, bool runtime = false) const;

    DLLLOCAL void mergeUserPublic(const FunctionList& src, qore_ns_private* ns) {
        for (fl_map_t::const_iterator i = src.begin(), e = src.end(); i != e; ++i) {
            if (!i->second->isUserPublic()) {
                //printd(5, "FunctionList::mergePublic() this: %p SKIPPING %s!\n", this, i->first);
                continue;
            }

            // If an entry with this name already exists in the target, it's a benign
            // same-origin duplicate — scanMergeCommittedNamespace already accepted it
            // (same QoreFunction pointer or same from_module). Skip the copy to avoid
            // re-inserting the entry and to preserve the original's ownership/refcount.
            if (findNode(i->first)) {
                continue;
            }
            FunctionEntry* fe = new ModuleImportedFunctionEntry(*i->second, ns);
            //printd(5, "FunctionList::mergePublic() this: %p merging in %s (%p)\n", this, i->first, fe);
            assert(!fe->isUserPublic());
            insert(fl_map_t::value_type(fe->getName(), fe));
        }
    }

    // returns the number of functions imported
    DLLLOCAL int importSystemFunctions(const FunctionList& src, qore_ns_private* ns, ExceptionSink* xsink);

    DLLLOCAL void del();
    DLLLOCAL int parseInit();
    DLLLOCAL void parseRollback();
    //! Removes a specific function by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name);
    DLLLOCAL void parseCommit();
    DLLLOCAL QoreListNode* getList();
    DLLLOCAL void assimilate(FunctionList& fl, qore_ns_private* ns,
        std::vector<std::string>* pending_names = nullptr);
    //! Merges the functions of a namespace that is added at run time (qore_ns_private::runtimeAssimilate())
    /** No parse is running: a new function is added, and a function that the namespace has is kept as it is, the new
        one being left in \a fl, which is kept with the namespace (qore_ns_private::retained_ns); no parse error is
        raised (qore#7530)
    */
    DLLLOCAL void runtimeAssimilate(FunctionList& fl, qore_ns_private* ns);
};

#endif
