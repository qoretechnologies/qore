/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HashDeclList.h

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

#ifndef _QORE_INTERN_HASHDECLLIST_H

#define _QORE_INTERN_HASHDECLLIST_H

#include <cstdlib>
#include <cstring>

// we use a vector map as the number of hashdecls is generally relatively small
#include <qore/vector_map>
typedef vector_map_t<const char*, TypedHashDecl*> hm_qth_t;
/*
#ifdef HAVE_QORE_HASH_MAP
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, TypedHashDecl*, qore_hash_str, eqstr> hm_qth_t;
#else
#include <map>
typedef std::map<const char*, TypedHashDecl*, ltstr> hm_qth_t;
#endif
*/

class QoreNamespaceList;

class HashDeclListIterator;
class ConstHashDeclListIterator;

class HashDeclList {
    friend class HashDeclListIterator;
    friend class ConstHashDeclListIterator;

private:
    hm_qth_t hm;        // hash_map for name lookups

    DLLLOCAL void deleteAll();

    DLLLOCAL void remove(hm_qth_t::iterator i);

    DLLLOCAL void addInternal(TypedHashDecl* ot);

public:
    DLLLOCAL HashDeclList() {}
    DLLLOCAL ~HashDeclList();
    DLLLOCAL HashDeclList(const HashDeclList& old, int64 po, qore_ns_private* ns);

    DLLLOCAL void mergeUserPublic(const HashDeclList& old, qore_ns_private* ns);

    // returns the number of hashdecls imported
    DLLLOCAL int importSystemHashDecls(const HashDeclList& source, qore_ns_private* ns, ExceptionSink* xsink);

    DLLLOCAL int add(TypedHashDecl* ot);

    DLLLOCAL TypedHashDecl* find(const char* name);
    DLLLOCAL const TypedHashDecl* find(const char* name) const;

    DLLLOCAL int parseInit();

    DLLLOCAL void reset();
    //! Removes a specific hashdecl by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name);

    DLLLOCAL void assimilate(HashDeclList& n, qore_ns_private& ns,
        std::vector<std::string>* pending_names = nullptr);
    DLLLOCAL QoreHashNode* getInfo();

    DLLLOCAL bool empty() const {
        return hm.empty();
    }

    //! Updates parentHashDecl pointers after copying to point to hashdecls in this list
    DLLLOCAL void updateParentPointers();
};

class HashDeclListIterator {
protected:
    hm_qth_t& hd;
    hm_qth_t::iterator i;

public:
    DLLLOCAL HashDeclListIterator(HashDeclList& n_hd) : hd(n_hd.hm), i(hd.end()) {
    }

    DLLLOCAL bool next() {
        if (i == hd.end()) {
            i = hd.begin();
        } else {
            ++i;
        }
        return i != hd.end();
    }

    DLLLOCAL const char* getName() const {
        return i->first;
    }

    DLLLOCAL TypedHashDecl* get() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

class ConstHashDeclListIterator {
protected:
    const hm_qth_t& hd;
    hm_qth_t::const_iterator i;

public:
    DLLLOCAL ConstHashDeclListIterator(const HashDeclList& n_hd) : hd(n_hd.hm), i(hd.end()) {
    }

    DLLLOCAL bool next() {
        if (i == hd.end()) {
            i = hd.begin();
        } else {
            ++i;
        }
        return i != hd.end();
    }

    DLLLOCAL const char* getName() const {
        return i->first;
    }

    DLLLOCAL const TypedHashDecl* get() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

#endif
