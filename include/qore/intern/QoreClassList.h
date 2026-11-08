/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClassList.h

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

#ifndef _QORE_QORECLASSLIST_H

#define _QORE_QORECLASSLIST_H

#include <qore/common.h>

#include <qore/QoreClass.h>

#include <cstdlib>
#include <cstring>

struct cl_rec_t {
    QoreClass* cls = nullptr;
    bool priv = false;
    //! true if the entry holds a reference to the class's constants; see qore_class_private::const_refs
    bool consts = true;
    //! true if the entry holds a reference to the values of the class's static variables; see qore_class_private::var_refs
    bool vars = true;
    //! true if the entry holds a reference to the class's static variables; see qore_class_private::data_refs
    bool data = true;

    DLLLOCAL cl_rec_t() {
    }

    DLLLOCAL cl_rec_t(QoreClass* c, bool p, bool v = true) : cls(c), priv(p), vars(v) {
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
    //! set when the constants of the classes have been released; a class added afterwards releases its reference
    bool ns_const : 1;
    //! set when the Program owning the list releases the values of the static variables of its classes
    /** Set when the Program's data is being cleared (see setVarsReleased()), and by clear() and deleteClassData().
        A copy of a class from a list with this flag set does not share the values (see qore_class_private::var_refs),
        nor does a class added to it.
    */
    bool ns_vars : 1;

    DLLLOCAL void deleteAll();

    //! releases the references of a class added to the list after its data was released
    /** A class can be added while the Program's data is released or afterwards (by a runtime module load in a
        destructor, for example): its references to the constants and to the values of the static variables are then
        released here, as the list no longer releases them; its reference to the static variables (data_refs) is
        released with the rest of the list's
    */
    DLLLOCAL void releaseAdded(cl_rec_t& rec);

    //! adds a copy of a class; \a vars is true if the copy may share the values of its static variables
    /** The entry takes over the copy's reference to the values, if it has one (see
        qore_class_private::takeWrapperVarRef()), and releases it at once if \a vars is false
    */
    DLLLOCAL void addInternal(QoreClass* ot, bool priv, bool vars);

    //! returns true if a copy of the class in the given entry of this list may share the values of its static variables
    /** see qore_class_private::var_refs
    */
    DLLLOCAL bool sharesVars(const cl_rec_t& rec) const;

public:
    DLLLOCAL QoreClassList() : ns_const(false), ns_vars(false) {}
    DLLLOCAL ~QoreClassList();
    DLLLOCAL QoreClassList(const QoreClassList& old, const QoreParseOptions& po, qore_ns_private* ns);

    DLLLOCAL void mergeUserPublic(const QoreClassList& old, qore_ns_private* ns);

    DLLLOCAL size_t size() const {
        return hm.size();
    }

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
    //! Removes a specific class by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name, ExceptionSink* xsink);
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* xsink);
    DLLLOCAL void reset();

    //! Invalidates the parent-namespace back-pointer of every class declared by the given namespace
    /** Called from qore_ns_private::purge() before the list drops its class references, so that a
        class which outlives the namespace does not keep a dangling pointer to it.
    */
    DLLLOCAL void namespaceDeleted(const qore_ns_private* ns);
    DLLLOCAL void assimilate(QoreClassList& n, qore_ns_private& ns,
        std::vector<std::string>* pending_names = nullptr);
    DLLLOCAL QoreHashNode* getInfo();

    //DLLLOCAL QoreValue findConstant(const char* cname, const QoreTypeInfo*& typeInfo, bool& found);

    //DLLLOCAL AbstractQoreNode* parseResolveBareword(const QoreProgramLocation* loc, const char* name, const QoreTypeInfo*& typeInfo);

    DLLLOCAL bool empty() const {
        return hm.empty();
    }

    // clears static class vars
    //! releases the static variable values of the classes; \a pgm is the Program owning the list
    DLLLOCAL void clear(ExceptionSink* xsink, const QoreProgram* pgm);
    DLLLOCAL void clearConstants(QoreListNode& l);
    DLLLOCAL void clearConstants(ExceptionSink* xsink);
    //! releases the class data held by the Program whose namespace is deleted; see qore_class_private::deleteClassData()
    DLLLOCAL void deleteClassData(const QoreProgram* pgm, ExceptionSink* xsink);

    //! marks the list as owned by a Program that is releasing the values of the static variables of its classes
    /** Called for all of the Program's class lists before any value is released, so that a Program created from it
        while the values are released (by a destructor) does not share them; see qore_class_private::var_refs
    */
    DLLLOCAL void setVarsReleased() {
        ns_vars = true;
    }
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
