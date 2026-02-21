/*
    FunctionList.cpp

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
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreNamespaceIntern.h"

#include <cstring>

ResolvedCallReferenceNode* FunctionEntry::makeCallReference(const QoreProgramLocation* loc) const {
   return new LocalFunctionCallReferenceNode(loc, func);
}

ModuleImportedFunctionEntry::ModuleImportedFunctionEntry(const FunctionEntry& old, qore_ns_private* ns, bool builtin)
        : FunctionEntry(old.getName(), new QoreFunction(*(old.getFunction()),
            builtin ? PO_NO_INHERIT_USER_FUNC_VARIANTS : PO_NO_SYSTEM_FUNC_VARIANTS), ns) {
}

FunctionList::FunctionList(const FunctionList& old, qore_ns_private* ns, const QoreParseOptions& po) {
    bool no_user = static_cast<bool>(po & PO_NO_INHERIT_USER_FUNC_VARIANTS);
    bool no_builtin = static_cast<bool>(po & PO_NO_SYSTEM_FUNC_VARIANTS);
    for (fl_map_t::const_iterator i = old.begin(), e = old.end(); i != e; ++i) {
        QoreFunction* f = i->second->getFunction();
        if (f->allPrivate()) {
            continue;
        }
        if (!f->hasBuiltin()) {
            if (no_user || !f->hasUserPublic())
                continue;
        } else if (no_builtin && !f->hasUserPublic())
            continue;

        // copy by reference if possible
        FunctionEntry* fe;
        if (!f->hasPrivate() && (!no_user || !f->hasUser()) && (!no_builtin || !f->hasBuiltin())
            && (f->getModuleName() || !get_module_context_name())) {
            QoreFunction* func = i->second->getFunction();
            func->ref();
            fe = new FunctionEntry(i->first, func, ns);;
        } else {
            // otherwise we have to make a new function object with only the desired visible variants
            // pass po.getLo() until QoreFunction is migrated to QoreParseOptions
            fe = new FunctionEntry(i->first, new QoreFunction(*f, po.getLo()), ns);
        }
        insert(std::make_pair(fe->getName(), fe));
    }
}

void FunctionList::del() {
    for (auto& i : *this) {
        i.second->deref();
    }
    /*
    for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
        delete i->second;
    */
    clear();
    assert(empty());
}

FunctionEntry* FunctionList::add(QoreFunction* func, qore_ns_private* ns) {
    QORE_TRACE("FunctionList::add()");
    assert(!findNode(func->getName()));

    FunctionEntry* n = new FunctionEntry(func, ns);
    insert(std::make_pair(func->getName(), n));
    return n;
}

FunctionEntry* FunctionList::import(QoreFunction* func, qore_ns_private* ns) {
    QORE_TRACE("FunctionList::import()");
    assert(!findNode(func->getName()));

    // copy function entry for import and insert into map
    FunctionEntry* fe = new FunctionEntry(new QoreFunction(*func, 0), ns);
    insert(fl_map_t::value_type(fe->getName(), fe));
    return fe;
}

FunctionEntry* FunctionList::import(const char* new_name, QoreFunction* func, qore_ns_private* ns, bool inject) {
    QORE_TRACE("FunctionList::import()");

    assert(!findNode(new_name));

    // copy function entry for import and insert into map
    FunctionEntry* fe = new FunctionEntry(new_name, new QoreFunction(*func, 0, true, inject), ns);
    insert(std::make_pair(fe->getName(), fe));
    return fe;
}

FunctionEntry* FunctionList::findNode(const char* name, bool runtime) const {
    printd(5, "FunctionList::findNode(%s)\n", name);

    fl_map_t::const_iterator i = fl_map_t::find(name);
    if (i != end()) {
        if (runtime && i->second->getFunction()->committedEmpty()) {
            return nullptr;
        }

        return i->second;
    }
    return nullptr;
}

QoreFunction* FunctionList::find(const char* name, bool runtime) const {
    printd(5, "FunctionList::findFunction(%s) (QoreFunction)\n", name);

    fl_map_t::const_iterator i = fl_map_t::find(name);
    if (i != end())
        return i->second->getFunction(runtime);

    return nullptr;
}

QoreListNode* FunctionList::getList() {
    QoreListNode* l = new QoreListNode(stringTypeInfo);

    for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
        l->push(new QoreStringNode(i->first), nullptr);

    return l;
}

int FunctionList::parseInit() {
    int err = 0;
    for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i) {
        if (i->second->parseInit() && !err) {
            err = -1;
        }
    }
    return err;
}

void FunctionList::parseCommit() {
    // commit pending variants in all functions
    for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
        i->second->parseCommit();
}

void FunctionList::parseRollback() {
    for (fl_map_t::iterator i = begin(), e = end(); i != e;) {
        if (i->second->parseRollback()) {
            i->second->deref();
            erase(i++);
            continue;
        }

        ++i;
    }
}

void FunctionList::parseRemove(const char* name) {
    fl_map_t::iterator i = fl_map_t::find(name);
    if (i != end()) {
        i->second->deref();
        erase(i);
    }
}

void FunctionList::assimilate(FunctionList& fl, qore_ns_private* ns,
        std::vector<std::string>* pending_names) {
    for (fl_map_t::iterator i = fl.begin(), e = fl.end(); i != e;) {
        fl_map_t::const_iterator li = fl_map_t::find(i->first);
        if (li == end()) {
            insert(fl_map_t::value_type(i->first, i->second));
            i->second->updateNs(ns);
            // track the new function name for rollback support
            if (pending_names) {
                pending_names->push_back(i->first);
            }
        } else {
            li->second->getFunction()->parseAssimilate(*(i->second->getFunction()));
            i->second->deref();
        }

        fl.erase(i++);
    }
}

int FunctionList::importSystemFunctions(const FunctionList& src, qore_ns_private* ns, ExceptionSink* xsink) {
    int cnt = 0;
    for (fl_map_t::const_iterator i = src.begin(), e = src.end(); i != e; ++i) {
        if (i->second->hasBuiltin()) {
            fl_map_t::const_iterator ci = fl_map_t::find(i->second->getName());
            if (ci != fl_map_t::end() && !ci->second->getFunction()->injected()) {
                xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system function %s::%s() due to an " \
                    "existing function without the injection flag set", ns->name.c_str(), ci->second->getName());
                break;
            }

            import(i->second->getFunction(), ns);
            ++cnt;
        }
    }
    return cnt;
}
