/*
    QoreClassList.cpp

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
#include "qore/intern/QoreClassList.h"
#include "qore/intern/HashDeclList.h"
#include "qore/intern/QoreNamespaceList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreNamespaceIntern.h"

#include <cassert>

QoreClassList::QoreClassList(const QoreClassList& old, const QoreParseOptions& po, qore_ns_private* ns) : ns_const(false), ns_vars(false) {
    for (hm_qc_t::const_iterator i = old.hm.begin(), e = old.hm.end(); i != e; ++i) {
        if (!i->second.cls->isSystem()) {
            //printd(5, "QoreClassList::QoreClassList() this: %p c: %p '%s' po & PO_NO_INHERIT_USER_CLASSES: %s
            //    pub: %s\n", this, i->second.cls, i->second.cls->getName(), po & PO_NO_INHERIT_USER_CLASSES ? "true": "false",
            //    qore_class_private::isPublic(*i->second.cls) ? "true": "false");
            if (po & PO_NO_INHERIT_USER_CLASSES || !qore_class_private::isPublic(*i->second.cls)) {
                continue;
            }
        } else {
            if (po & PO_NO_INHERIT_SYSTEM_CLASSES) {
                continue;
            }
        }
        QoreClass* qc = i->second.cls->copy();
        // we cannot update the namespace in the class because classes are immutable; the class always points to the
        // original namespace
        addInternal(qc, true);
    }
}

QoreClassList::~QoreClassList() {
    deleteAll();
}

void QoreClassList::deleteAll() {
    for (auto& i : hm) {
        //printd(5, "QoreClassList::deleteAll() this: %p cls: '%s' %p priv: %p ns_const: %d ns_vars: %d\n", this,
        //  i.second.cls->getName(), i.second.cls, qore_class_private::get(*i.second.cls), ns_const, ns_vars);
        qore_class_private::get(*i.second.cls)->deref(!ns_const, !ns_vars);
    }
    hm.clear();
    ns_const = false;
    ns_vars = false;
}

void QoreClassList::addInternal(QoreClass* oc, bool priv) {
    printd(5, "QCL::addInternal() this: %p '%s' (%p)\n", this, oc->getName(), oc);

    assert(!find(oc->getName()));
    hm[oc->getName()] = cl_rec_t(oc, priv);
}

int QoreClassList::add(QoreClass* oc) {
    printd(5, "QCL::add() this: %p '%s' (%p)\n", this, oc->getName(), oc);

    if (find(oc->getName())) {
        return 1;
    }

    hm[oc->getName()] = cl_rec_t(oc, false);
    return 0;
}

QoreClass* QoreClassList::find(const char* name) {
    hm_qc_t::iterator i = hm.find(name);
    return i != hm.end() ? i->second.cls : nullptr;
}

const QoreClass* QoreClassList::find(const char* name) const {
    hm_qc_t::const_iterator i = hm.find(name);
    return i != hm.end() ? i->second.cls : nullptr;
}

void QoreClassList::mergeUserPublic(const QoreClassList& old, qore_ns_private* ns) {
    for (hm_qc_t::const_iterator i = old.hm.begin(), e = old.hm.end(); i != e; ++i) {
        if (i->second.priv || !qore_class_private::isUserPublic(*i->second.cls)) {
            continue;
        }

        QoreClass* qc = find(i->first);
        if (qc) {
            // the class must be injected, already imported (same pointer), or a copy of the same
            // original class (same classID)
            assert(qore_class_private::injected(*qc)
                || qore_class_private::get(*qc) == qore_class_private::get(*i->second.cls)
                || qore_class_private::get(*qc)->classID == qore_class_private::get(*i->second.cls)->classID);
            continue;
        }

        qc = i->second.cls->copy();
        // we cannot update the namespace in the class because classes are immutable; the class always points to the
        // original namespace
        addInternal(qc, true);
    }
}

int QoreClassList::importSystemClasses(const QoreClassList& source, qore_ns_private* ns, ExceptionSink* xsink) {
    int cnt = 0;
    for (hm_qc_t::const_iterator i = source.hm.begin(), e = source.hm.end(); i != e; ++i) {
        if (i->second.cls->isSystem()) {
            hm_qc_t::const_iterator ci = hm.find(i->second.cls->getName());
            if (ci != hm.end()) {
                if (!qore_class_private::injected(*ci->second.cls)) {
                    xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system class %s::%s due to the " \
                        "presence of an existing class without the injection flag set", ns->name.c_str(),
                        ci->second.cls->getName());
                    break;
                }
                continue;
            }
            //printd(5, "QoreClassList::importSystemClasses() this: %p importing %p %s::'%s'\n", this, i->second,
            //  ns->name.c_str(), i->second->getName());
            QoreClass* qc = i->second.cls->copy();
            // we cannot update the namespace in the class because classes are immutable; the class always points to the
            // original namespace
            addInternal(qc, true);
            ++cnt;
        }
    }
    return cnt;
}

void QoreClassList::resolveCopy() {
    for (auto& i : hm) {
        qore_class_private::resolveCopy(*(i.second.cls));
    }
}

int QoreClassList::parseInit() {
    int err = 0;
    for (auto& i : hm) {
        printd(5, "QoreClassList::parseInit() this: %p initializing %p '%s' (%s)\n", this, i.second.cls, i.first,
            i.second.cls->getName());

        if (qore_class_private::get(*(i.second.cls))->parseInit() && !err) {
            err = -1;
        }
    }
    return err;
}

void QoreClassList::parseResolveHierarchy() {
    for (auto& i : hm) {
        qore_class_private::get(*(i.second.cls))->parseResolveHierarchy();
    }
}

void QoreClassList::parseResolveClassMembers() {
    for (auto& i : hm) {
        qore_class_private::get(*(i.second.cls))->initializeMembers();
    }
}

void QoreClassList::parseResolveAbstract() {
    for (auto& i : hm) {
        qore_class_private::get(*(i.second.cls))->parseResolveAbstract();
    }
}

void QoreClassList::parseRollback() {
    for (auto& i : hm) {
        qore_class_private::parseRollback(*(i.second.cls));
    }
}

void QoreClassList::parseRemove(const char* name, ExceptionSink* xsink) {
    hm_qc_t::iterator i = hm.find(name);
    if (i != hm.end()) {
        // Roll back any pending variants first
        qore_class_private::parseRollback(*(i->second.cls));
        // Then delete the class
        qore_class_private::get(*i->second.cls)->deref(!ns_const, !ns_vars);
        hm.erase(i);
    }
}

void QoreClassList::parseCommit() {
    for (auto& i : hm) {
        //printd(5, "QoreClassList::parseCommit() this: %p qc: %p '%s' pub: %d\n", this, i.second,
        //  i.second->getName(), qore_class_private::isPublic(*i.second));
        qore_class_private::parseCommit(*(i.second.cls));
    }
}

void QoreClassList::parseCommitRuntimeInit(ExceptionSink* xsink) {
    for (auto& i : hm) {
        qore_class_private::parseCommitRuntimeInit(*(i.second.cls), xsink);
    }
}

void QoreClassList::reset() {
    deleteAll();
}

void QoreClassList::assimilate(QoreClassList& n, qore_ns_private& ns,
        std::vector<std::string>* pending_names) {
    for (auto& i : n.hm) {
        if (ns.hashDeclList.find(i.first)) {
            parse_error(*qore_class_private::get(*i.second.cls)->loc, "hashdecl '%s' has already been defined in " \
                "namespace '%s'", i.first, ns.name.c_str());
            qore_class_private::get(*i.second.cls)->deref(!ns_const, !ns_vars);
        } else if (QoreClass* existing = ns.classList.find(i.first)) {
            if (!qore_class_private::injected(*existing)) {
                parse_error(*qore_class_private::get(*i.second.cls)->loc,
                    "class '%s' has already been defined in namespace '%s'", i.first, ns.name.c_str());
            }
            qore_class_private::get(*i.second.cls)->deref(!ns_const, !ns_vars);
        } else if (find(i.first)) {
            parse_error(*qore_class_private::get(*i.second.cls)->loc, "class '%s' is already pending in namespace " \
                "'%s'", i.first, ns.name.c_str());
            qore_class_private::get(*i.second.cls)->deref(!ns_const, !ns_vars);
        } else if (ns.nsl.find(i.first)) {
            parse_error(*qore_class_private::get(*i.second.cls)->loc, "cannot add class '%s' to existing " \
                "namespace '%s' because a subnamespace has already been defined with this name", i.first,
                ns.name.c_str());
            qore_class_private::get(*i.second.cls)->deref(!ns_const, !ns_vars);
        } else {
            printd(5, "QoreClassList::assimilate() this: %p adding: %p '%s::%s'\n", this, i.second,
                ns.name.c_str(), i.second.cls->getName());
            // "move" data to new list
            hm[i.first] = i.second;
            // move class to new namespace
            qore_class_private::get(*i.second.cls)->updateNamespace(&ns);
            // track the new class name for rollback support
            if (pending_names) {
                pending_names->push_back(i.first);
            }
        }
    }
    n.hm.clear();
}

QoreHashNode* QoreClassList::getInfo() {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    for (hm_qc_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
        h->setKeyValue(i->first, i->second.cls->getMethodList(), nullptr);
    return h;
}

void QoreClassList::clearConstants(QoreListNode& l) {
    assert(!ns_const);
    ns_const = true;
    for (auto& i : hm) {
        qore_class_private::get(*i.second.cls)->clearConstants(l);
    }
}

void QoreClassList::clearConstants(ExceptionSink* xsink) {
    if (!ns_const) {
        ns_const = true;
        for (auto& i : hm) {
            qore_class_private::get(*i.second.cls)->clearConstants(xsink);
        }
    }
}

// clears static class vars
void QoreClassList::clear(ExceptionSink* xsink) {
    assert(!ns_vars);
    ns_vars = true;
    for (auto& i : hm) {
        qore_class_private::get(*i.second.cls)->clear(xsink);
    }
}

void QoreClassList::deleteClassData(bool deref_vars, ExceptionSink* xsink) {
    if (deref_vars) {
        if (!ns_vars) {
            ns_vars = true;
        } else {
            deref_vars = false;
        }
    } else {
        assert(ns_vars);
    }
    for (auto& i : hm) {
        qore_class_private::get(*i.second.cls)->deleteClassData(deref_vars, xsink);
    }
}

bool ClassListIterator::isPublic() const {
    return !i->second.priv && qore_class_private::isPublic(*i->second.cls);
}

bool ConstClassListIterator::isPublic() const {
    return !i->second.priv && qore_class_private::isPublic(*i->second.cls);
}

bool ClassListIterator::isUserPublic() const {
    return !i->second.priv && qore_class_private::isUserPublic(*i->second.cls);
}

bool ConstClassListIterator::isUserPublic() const {
    return !i->second.priv && qore_class_private::isUserPublic(*i->second.cls);
}
