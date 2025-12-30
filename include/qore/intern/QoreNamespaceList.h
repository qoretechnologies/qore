/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreNamespaceList.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

    namespaces are children of a program object.  there is a parse
    lock per program object to ensure that objects are added (or backed out)
    atomically per program object.  All the objects referenced here should
    be safe to read & copied at all times.  They will only be deleted when the
    program object is deleted (except the pending structures, which will be
    deleted any time there is a parse error, together with all other
    pending structures)

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

#ifndef _QORE_NAMESPACELIST_H

#define _QORE_NAMESPACELIST_H

// issue #3461: cannot use a vector map because we erase in the map while iterating

#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<std::string, QoreNamespace*> nsmap_t;
#else
#include <map>
typedef std::map<std::string, QoreNamespace*> nsmap_t;
#endif

class qore_ns_private;
class qore_root_ns_private;

struct GVEntryBase {
    NamedScope* name;
    Var* var;

    DLLLOCAL GVEntryBase(const NamedScope& n, Var* v) : name(new NamedScope(n)), var(v) {
    }

    DLLLOCAL GVEntryBase(const QoreProgramLocation* loc, char* n, const QoreTypeInfo* typeInfo,
        QoreParseTypeInfo* parseTypeInfo, qore_var_t type);

    DLLLOCAL GVEntryBase(const GVEntryBase& old) : name(old.name), var(old.var) {
    }

    DLLLOCAL void clear();

    DLLLOCAL Var* takeVar() {
        Var* rv = var;
        var = nullptr;
        return rv;
    }
};

struct GVEntry : public GVEntryBase {
    qore_ns_private* ns;

    DLLLOCAL GVEntry(qore_ns_private* n_ns, const NamedScope& n, Var* v) : GVEntryBase(n, v), ns(n_ns) {
    }

    DLLLOCAL GVEntry(const GVEntry& old) : GVEntryBase(old), ns(old.ns) {
    }

    DLLLOCAL GVEntry(const GVEntryBase& old, qore_ns_private* n_ns) : GVEntryBase(old), ns(n_ns) {
    }
};

template <class T>
struct GVList : public std::vector<T> {
    DLLLOCAL ~GVList() {
        clear();
    }

    DLLLOCAL void clear() {
        for (typename GVList<T>::iterator i = std::vector<T>::begin(), e = std::vector<T>::end(); i != e; ++i)
            (*i).clear();
        std::vector<T>::clear();
    }

    DLLLOCAL void zero() {
        std::vector<T>::clear();
    }
};

typedef GVList<GVEntryBase> gvblist_t;
typedef GVList<GVEntry> gvlist_t;

class QoreNamespaceList {
private:
    DLLLOCAL void deleteAll();

    // not implemented
    DLLLOCAL QoreNamespaceList(const QoreNamespaceList& old);
    // not implemented
    DLLLOCAL QoreNamespaceList& operator=(const QoreNamespaceList& nsl);

public:
    nsmap_t nsmap;

    DLLLOCAL QoreNamespaceList() {
    }

    DLLLOCAL QoreNamespaceList(const QoreNamespaceList& old, int64 po, const qore_ns_private& parent);

    DLLLOCAL ~QoreNamespaceList() {
        deleteAll();
    }

    DLLLOCAL QoreNamespace *find(const char* name) {
        nsmap_t::iterator i = nsmap.find(name);
        return i == nsmap.end() ? 0 : i->second;
    }

    DLLLOCAL QoreNamespace *find(const std::string &name) {
        nsmap_t::iterator i = nsmap.find(name);
        return i == nsmap.end() ? 0 : i->second;
    }
    DLLLOCAL const QoreNamespace* find(const std::string& name) const {
        nsmap_t::const_iterator i = nsmap.find(name);
        return i == nsmap.end() ? 0 : i->second;
    }

    // do not delete the pointer returned from this function
    DLLLOCAL qore_ns_private* parseAdd(QoreNamespace* ot, qore_ns_private* parent);

    DLLLOCAL qore_ns_private* runtimeAdd(QoreNamespace* ot, qore_ns_private* parent);

    DLLLOCAL void resolveCopy();
    DLLLOCAL int parseInitConstants();

    DLLLOCAL int parseInitGlobalVars();
    DLLLOCAL void clearConstants(QoreListNode& l);
    DLLLOCAL void clearData(ExceptionSink* sink);
    DLLLOCAL void deleteGlobalVars(ExceptionSink* sink);

    DLLLOCAL void parseResolveHierarchy();
    DLLLOCAL void parseResolveClassMembers();
    DLLLOCAL int parseInit();
    DLLLOCAL void parseResolveAbstract();
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* sink);
    DLLLOCAL void parseRollback(ExceptionSink* sink);
    //! Removes a specific namespace by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name, ExceptionSink* xsink);
    DLLLOCAL void deleteAllConstants(ExceptionSink *xsink);
    DLLLOCAL void reset();

    DLLLOCAL void parseAssimilate(QoreNamespaceList& n, qore_ns_private* parent,
        std::vector<std::string>* pending_names = nullptr);
    DLLLOCAL void runtimeAssimilate(QoreNamespaceList& n, qore_ns_private* parent);

    DLLLOCAL bool addGlobalVars(qore_root_ns_private& rns);

    DLLLOCAL void deleteData(bool deref_vars, ExceptionSink *xsink);

    DLLLOCAL bool empty() const {
        return nsmap.empty();
    }

    DLLLOCAL size_t size() const {
        return nsmap.size();
    }

    DLLLOCAL void getGlobalVars(QoreHashNode& h) const;
};

#endif
