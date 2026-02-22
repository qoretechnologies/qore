/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    EnumList.h

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

#ifndef _QORE_INTERN_ENUMLIST_H

#define _QORE_INTERN_ENUMLIST_H

#include <qore/safe_dslist>
#include <qore/hash_map_include.h>
#include <qore/QoreEnumDecl.h>

#include <string>

// forward declarations
class qore_ns_private;

typedef HASH_MAP<const char*, QoreEnumDecl*, qore_hash_str, eqstr> hm_qen_t;

class EnumListIterator;
class ConstEnumListIterator;

class EnumList {
    friend class EnumListIterator;
    friend class ConstEnumListIterator;

private:
    hm_qen_t hm;        // hash_map for name lookups

    DLLLOCAL void deleteAll();

    DLLLOCAL void remove(hm_qen_t::iterator i);

    DLLLOCAL void addInternal(QoreEnumDecl* ed);

public:
    DLLLOCAL EnumList() {}
    DLLLOCAL ~EnumList();
    DLLLOCAL EnumList(const EnumList& old, const QoreParseOptions& po, qore_ns_private* ns);

    DLLLOCAL void mergeUserPublic(const EnumList& old, qore_ns_private* ns);

    // returns the number of enums imported
    DLLLOCAL int importSystemEnums(const EnumList& source, qore_ns_private* ns, ExceptionSink* xsink);

    DLLLOCAL int add(QoreEnumDecl* ed);

    DLLLOCAL QoreEnumDecl* find(const char* name);
    DLLLOCAL const QoreEnumDecl* find(const char* name) const;

    DLLLOCAL int parseInit();

    DLLLOCAL void reset();
    //! Removes a specific enum by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name);

    DLLLOCAL void assimilate(EnumList& n, qore_ns_private& ns,
        std::vector<std::string>* pending_names = nullptr);
    DLLLOCAL QoreHashNode* getInfo();

    DLLLOCAL bool empty() const {
        return hm.empty();
    }
};

class EnumListIterator {
protected:
    hm_qen_t& hd;
    hm_qen_t::iterator i;

public:
    DLLLOCAL EnumListIterator(EnumList& n_hd) : hd(n_hd.hm), i(hd.end()) {
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

    DLLLOCAL QoreEnumDecl* get() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

class ConstEnumListIterator {
protected:
    const hm_qen_t& hd;
    hm_qen_t::const_iterator i;

public:
    DLLLOCAL ConstEnumListIterator(const EnumList& n_hd) : hd(n_hd.hm), i(hd.end()) {
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

    DLLLOCAL const QoreEnumDecl* get() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const;

    DLLLOCAL bool isUserPublic() const;
};

#endif
