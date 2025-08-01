/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClassList.h

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

#ifndef _QORE_QORECLASSLIST_H

#define _QORE_QORECLASSLIST_H

#include <qore/common.h>

#include <qore/QoreClass.h>

#include <cstdlib>
#include <cstring>

struct cl_rec_t {
    QoreClass* cls = nullptr;
    bool priv = false;

    DLLLOCAL cl_rec_t() {
    }

    DLLLOCAL cl_rec_t(QoreClass* c, bool p) : cls(c), priv(p) {
    }
};

// we use a vector map as the number of classes is generally relatively small
//#include <qore/vector_map>
//typedef vector_map_t<const char*, cl_rec_t> hm_qc_t;

///*
#ifdef HAVE_QORE_HASH_MAPX
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, cl_rec_t, qore_hash_str, eqstr> hm_qc_t;
#else
#include <map>
typedef std::map<const char*, cl_rec_t, ltstr> hm_qc_t;
#endif
//*/

class QoreNamespaceList;

class ClassListIterator;
class ConstClassListIterator;

class QoreClassList {
    friend class ClassListIterator;
    friend class ConstClassListIterator;

private:
    hm_qc_t hm;        // hash_map for name lookups
    bool ns_const : 1;
    bool ns_vars : 1;

    DLLLOCAL void deleteAll();

    DLLLOCAL void addInternal(QoreClass* ot, bool priv);

public:
    DLLLOCAL QoreClassList() : ns_const(false), ns_vars(false) {}
    DLLLOCAL ~QoreClassList();
    DLLLOCAL QoreClassList(const QoreClassList& old, int64 po, qore_ns_private* ns);

    DLLLOCAL void mergeUserPublic(const QoreClassList& old, qore_ns_private* ns);

    // returns the number of classes imported
    DLLLOCAL int importSystemClasses(const QoreClassList& source, qore_ns_private* ns, ExceptionSink* xsink);

    DLLLOCAL int add(QoreClass* ot);
    DLLLOCAL QoreClass* find(const char* name);
    DLLLOCAL const QoreClass* find(const char* name) const;
    DLLLOCAL void resolveCopy();
    DLLLOCAL void parseResolveHierarchy();
    DLLLOCAL void parseResolveClassMembers();
    DLLLOCAL int parseInit();
    DLLLOCAL void parseResolveAbstract();
    DLLLOCAL void parseRollback();
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* xsink);
    DLLLOCAL void reset();
    DLLLOCAL void assimilate(QoreClassList& n, qore_ns_private& ns);
    DLLLOCAL QoreHashNode* getInfo();

    //DLLLOCAL QoreValue findConstant(const char* cname, const QoreTypeInfo*& typeInfo, bool& found);

    //DLLLOCAL AbstractQoreNode* parseResolveBareword(const QoreProgramLocation* loc, const char* name, const QoreTypeInfo*& typeInfo);

    DLLLOCAL bool empty() const {
        return hm.empty();
    }

    // clears static class vars
    DLLLOCAL void clear(ExceptionSink* xsink);
    DLLLOCAL void clearConstants(QoreListNode& l);
    DLLLOCAL void clearConstants(ExceptionSink* xsink);
    DLLLOCAL void deleteClassData(bool deref_vars, ExceptionSink* xsink);
};

class ClassListIterator {
protected:
    hm_qc_t& cl;
    hm_qc_t::iterator i;

public:
    DLLLOCAL ClassListIterator(QoreClassList& n_cl) : cl(n_cl.hm), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end())
            i = cl.begin();
        else
            ++i;
        return i != cl.end();
    }

    DLLLOCAL const char* getName() const {
        return i->first;
    }

    DLLLOCAL QoreClass* get() const {
        return i->second.cls;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

class ConstClassListIterator {
protected:
    const hm_qc_t& cl;
    hm_qc_t::const_iterator i;

public:
    DLLLOCAL ConstClassListIterator(const QoreClassList& n_cl) : cl(n_cl.hm), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end())
            i = cl.begin();
        else
            ++i;
        return i != cl.end();
    }

    DLLLOCAL const char* getName() const {
        return i->first;
    }

    DLLLOCAL const QoreClass* get() const {
        return i->second.cls;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

#endif // _QORE_QORECLASSLIST_H
