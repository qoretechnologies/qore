/*
    GlobalVariableList.cpp

    Program QoreObject Definition

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

#include <qore/Qore.h>
#include "qore/intern/GlobalVariableList.h"

#include <cassert>

GlobalVariableList::GlobalVariableList(const GlobalVariableList& old, int64 po) {
    // don't inherit any global vars if the appropriate flag is not already set
    if ((po & PO_NO_INHERIT_GLOBAL_VARS))
        return;

    map_var_t::iterator last = vmap.begin();
    for (map_var_t::const_iterator i = old.vmap.begin(), e = old.vmap.end(); i != e; ++i) {
        //printd(5, "GlobalVariableList::GlobalVariableList() this: %p v: %p '%s' pub: %d\n", this, i->second, i->second->getName(), i->second->isPublic());
        if (!i->second->isPublic())
            continue;
        Var* v = new Var(const_cast<Var*>(i->second));
        last = vmap.insert(last, map_var_t::value_type(v->getName(), v));
    }
}

void GlobalVariableList::mergePublic(const GlobalVariableList& old) {
   map_var_t::iterator last = vmap.begin();
   for (map_var_t::const_iterator i = old.vmap.begin(), e = old.vmap.end(); i != e; ++i) {
      if (!i->second->isPublic())
         continue;
      Var* v = new Var(const_cast<Var*>(i->second));
      last = vmap.insert(last, map_var_t::value_type(v->getName(), v));
   }
}

// adds directly to committed list
Var* GlobalVariableList::import(Var* v, ExceptionSink* xsink, bool readonly, const char* import_as) {
    const char* name = import_as ? import_as : v->getName();
    map_var_t::iterator i = vmap.find(name);
    if (i != vmap.end()) {
        xsink->raiseException("PROGRAM-IMPORTGLOBALVARIABLE-EXCEPTION", "'%s' already exists in the target namespace",
            name);
        return nullptr;
    }

    Var* var = new Var(v, readonly, false, import_as);
    vmap[name] = var;

    printd(5, "GlobalVariableList::import(): reference to %s (%p) added\n", name, var);
    return var;
}

// sets all non-imported variables to NULL (dereferences contents if any)
void GlobalVariableList::clearAll(ExceptionSink* xsink) {
   //printd(5, "GlobalVariableList::clear_all() this: %p (size: %d)\n", this, vmap.size());
   for (map_var_t::iterator i = vmap.begin(), e = vmap.end(); i != e; ++i) {
      i->second->clearLocal(xsink);
   }
}

void GlobalVariableList::deleteAll(ExceptionSink* xsink) {
    for (auto& i : vmap) {
        i.second->deref(xsink);
    }
    vmap.clear();
}

Var* GlobalVariableList::runtimeCreateVar(const char* name, const QoreTypeInfo* typeInfo, bool builtin) {
    if (parseFindVar(name))
        return nullptr;

    Var* var = new Var(get_runtime_location(), name, typeInfo, builtin);
    vmap[var->getName()] = var;

    printd(5, "GlobalVariableList::runtimeCreateVar(): %s (%p) added (resolved type %s)\n", name, var, QoreTypeInfo::getName(typeInfo));
    return var;
}

Var* GlobalVariableList::parseCreatePendingVar(const QoreProgramLocation* loc, const char* name, const QoreTypeInfo* typeInfo) {
    assert(!parseFindVar(name));

    Var* var = new Var(loc, name, typeInfo);
    vmap[var->getName()] = var;

    //printd(5, "GlobalVariableList::parseCreatePendingVar(): %s (%p) added (resolved type %s)\n", name, var, QoreTypeInfo::getName(typeInfo));
    return var;
}

Var* GlobalVariableList::parseFindVar(const char* name) {
    map_var_t::iterator i = vmap.find(name);
    if (i != vmap.end())
        return i->second;
    return nullptr;
}

const Var* GlobalVariableList::parseFindVar(const char* name) const {
    map_var_t::const_iterator i = vmap.find(name);
    if (i != vmap.end())
        return i->second;
    return nullptr;
}

int GlobalVariableList::parseInit() {
    int err = 0;
    for (auto& i : vmap) {
        if (i.second->parseInit() && !err) {
            err = -1;
        }
    }
    return err;
}

void GlobalVariableList::reset() {
    //printd(5, "GlobalVariableList::reset() this: %p\n", this);
    for (auto& i : vmap) {
        i.second->deref(nullptr);
    }
    vmap.clear();
}

QoreListNode* GlobalVariableList::getVarList() const {
    QoreListNode* l = new QoreListNode(stringTypeInfo);

    for (map_var_t::const_iterator i = vmap.begin(); i != vmap.end(); i++)
        l->push(new QoreStringNode(i->first), nullptr);

    return l;
}

void GlobalVariableList::parseAdd(Var* v) {
    assert(!parseFindVar(v->getName()));
    vmap[v->getName()] = v;
}

void GlobalVariableList::getGlobalVars(const std::string& path, QoreHashNode& h) const {
    for (map_var_t::const_iterator i = vmap.begin(); i != vmap.end(); i++) {
        std::string n = path;
        if (!n.empty())
            n.append("::");
        n.append(i->first);
        h.setKeyValue(n.c_str(), i->second->eval(), nullptr);
    }
}
