/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_TreeMap.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologoes, s.r.o.

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

#ifndef _QORE_QC_TREEMAP_H
#define _QORE_QC_TREEMAP_H

#include <qore/Qore.h>
#include <map>
#include <string>

// forward declarations for DGC support
class RObject;
class RSetHelper;

DLLEXPORT extern qore_classid_t CID_TREEMAP;
DLLLOCAL extern QoreClass* QC_TREEMAP;

DLLLOCAL QoreClass* initTreeMapClass(QoreNamespace& ns);

static inline bool isPathEnd(char c) {
    return c == '/' || c == '?';
}

static inline size_t getFirstPathSegmentLength(const std::string& path) {
    size_t prefixLen = path.find_first_of("/?");
    return prefixLen == std::string::npos ? path.length() : prefixLen;
}

static inline bool isPrefix(const std::string& prefix, const std::string& str) {
    return str.length() >= prefix.length() && !str.compare(0, prefix.length(), prefix);
}

static inline bool isPathPrefix(const std::string& prefix, const std::string& path) {
    return isPrefix(prefix, path) && (path.length() == prefix.length() || isPathEnd(path[prefix.length()]));
}

class TreeMapData : public AbstractPrivateData {
public:
    DLLLOCAL TreeMapData() {
    }

    // called from destructor - does not call decScanPrivateData (object being destroyed)
    DLLLOCAL void destructor(ExceptionSink* xsink);

    // override deref to ensure destructor is called when reference count drops to 0
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            destructor(xsink);
            delete this;
        }
    }

    DLLLOCAL void put(QoreObject* self, const QoreStringNode* key, const QoreValue value, ExceptionSink* xsink);

    // DGC support - scans stored values for circular references
    DLLLOCAL bool scanMembers(RObject& obj, RSetHelper& rsh);

    // return value must be dereferenced by the caller
    DLLLOCAL QoreValue get(const QoreStringNode* key, const ReferenceNode* unmatched, ExceptionSink* xsink) const {
        TempEncodingHelper keyStr(key, QCS_DEFAULT, xsink);
        if (!keyStr) {
            return QoreValue();
        }

        QoreAutoRWReadLocker al(rwl);
        if (!data.empty()) {
            std::string path(keyStr->c_str());

            Map::const_iterator b = data.begin();
            Map::const_iterator it = data.upper_bound(path);

            size_t prefixLen = getFirstPathSegmentLength(path);
            while (it != b && !(--it)->first.compare(0, prefixLen, path, 0, prefixLen)) {
                if (isPathPrefix(it->first, path)) {
                    if (unmatched) {
                        size_t l = it->first.length();
                        if (isPathEnd(path[l])) {
                            l++;
                        }
                        QoreTypeSafeReferenceHelper ref(unmatched, xsink);
                        if (!ref)
                            return QoreValue();
                        SimpleRefHolder<QoreStringNode> path(key->substr(l, xsink));
                        if (*xsink || ref.assign(path.release()))
                            return QoreValue();
                    }
                    return it->second.refSelf();
                }
            }
        }
        if (unmatched) {
            QoreTypeSafeReferenceHelper ref(unmatched, xsink);
            if (!ref)
                return QoreValue();
            if (ref.assign(&Nothing))
                return QoreValue();
        }
        return QoreValue();
    }

    DLLLOCAL QoreHashNode* getAll() const {
        QoreAutoRWReadLocker al(rwl);

        if (data.empty())
            return nullptr;

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);
        for (Map::const_iterator i = data.begin(), e = data.end(); i != e; ++i)
            h->setKeyValue(i->first.c_str(), i->second.refSelf(), nullptr);
        return h.release();
    }

    // return value must be dereferenced
    DLLLOCAL QoreValue take(QoreObject* self, const QoreStringNode* key, ExceptionSink* xsink);

    // static accessor for QoreObject.cpp DGC support
    DLLLOCAL static TreeMapData* get(QoreObject& obj, ExceptionSink* xsink) {
        return static_cast<TreeMapData*>(obj.getReferencedPrivateData(CID_TREEMAP, xsink));
    }

private:
    typedef std::map<std::string, QoreValue> Map;
    Map data;
    mutable QoreRWLock rwl;

    // issue #5028: maintain a count of all scanable objects in the treemap for DGC
    int scan_count = 0;
};

#endif
