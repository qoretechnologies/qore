/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    EnumList.cpp

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

#include <qore/Qore.h>
#include "qore/intern/EnumList.h"
#include "qore/intern/QoreClassList.h"
#include "qore/intern/QoreNamespaceList.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/QoreNamespaceIntern.h"

#include <cassert>

void EnumList::remove(hm_qen_t::iterator i) {
    QoreEnumDecl* ed = i->second;
    hm.erase(i);
    qore_enum_decl_private::get(*ed)->deref();
}

void EnumList::deleteAll() {
    for (hm_qen_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i) {
        qore_enum_decl_private::get(*i->second)->deref();
    }
    hm.clear();
}

EnumList::~EnumList() {
    deleteAll();
}

void EnumList::addInternal(QoreEnumDecl* ed) {
    assert(!find(ed->getName()));
    hm[ed->getName()] = ed;
}

int EnumList::add(QoreEnumDecl* ed) {
    // Ensure that enums always have a namespace set before adding to the namespace enum list
    assert(qore_enum_decl_private::get(*ed)->getNamespace());

    if (find(ed->getName())) {
        return -1;
    }

    hm[ed->getName()] = ed;
    return 0;
}

QoreEnumDecl* EnumList::find(const char* name) {
    hm_qen_t::iterator i = hm.find(name);
    return i != hm.end() ? i->second : nullptr;
}

const QoreEnumDecl* EnumList::find(const char* name) const {
    hm_qen_t::const_iterator i = hm.find(name);
    return i != hm.end() ? i->second : nullptr;
}

EnumList::EnumList(const EnumList& old, int64 po, qore_ns_private* ns) {
    for (auto& i : old.hm) {
        if (!i.second->isSystem()) {
            if ((po & PO_NO_ENUM) || !qore_enum_decl_private::get(*i.second)->isPublic()) {
                continue;
            }
        } else {
            if (po & PO_NO_ENUM) {
                continue;
            }
        }

        QoreEnumDecl* ed = new QoreEnumDecl(*i.second);
        qore_enum_decl_private::get(*ed)->setNamespace(ns);
        addInternal(ed);
    }
}

void EnumList::mergeUserPublic(const EnumList& old, qore_ns_private* ns) {
    for (auto& i : old.hm) {
        if (!qore_enum_decl_private::get(*i.second)->isUserPublic()) {
            continue;
        }

        if (find(i.first)) {
            continue;
        }
        QoreEnumDecl* ed = new QoreEnumDecl(*i.second);
        qore_enum_decl_private::get(*ed)->setNamespace(ns);
        addInternal(ed);
    }
}

int EnumList::importSystemEnums(const EnumList& source, qore_ns_private* ns, ExceptionSink* xsink) {
    int cnt = 0;
    for (auto& i : source.hm) {
        if (i.second->isSystem()) {
            hm_qen_t::const_iterator ci = hm.find(i.second->getName());
            if (ci != hm.end()) {
                xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system enum %s::%s due to the " \
                    "presence of an existing enum with the same name in the target namespace", ns->name.c_str(),
                    ci->second->getName());
                break;
            }
            QoreEnumDecl* ed = new QoreEnumDecl(*i.second);
            qore_enum_decl_private::get(*ed)->setNamespace(ns);
            addInternal(ed);
            ++cnt;
        }
    }
    return cnt;
}

int EnumList::parseInit() {
    int err = 0;
    for (auto& i : hm) {
        if (qore_enum_decl_private::get(*(i.second))->parseInit() && !err) {
            err = -1;
        }
    }
    return err;
}

void EnumList::reset() {
    deleteAll();
}

void EnumList::assimilate(EnumList& n, qore_ns_private& ns,
        std::vector<std::string>* pending_names) {
    hm_qen_t::iterator i = n.hm.begin();
    while (i != n.hm.end()) {
        if (ns.classList.find(i->first)) {
            parse_error(*qore_enum_decl_private::get(*i->second)->getParseLocation(), "class '%s' has already " \
                "been defined in namespace '%s'", i->first, ns.name.c_str());
            n.remove(i);
        } else if (ns.hashDeclList.find(i->first)) {
            parse_error(*qore_enum_decl_private::get(*i->second)->getParseLocation(), "hashdecl '%s' has already " \
                "been defined in namespace '%s'", i->first, ns.name.c_str());
            n.remove(i);
        } else if (ns.enumList.find(i->first)) {
            parse_error(*qore_enum_decl_private::get(*i->second)->getParseLocation(), "enum '%s' has already " \
                "been defined in namespace '%s'", i->first, ns.name.c_str());
            n.remove(i);
        } else if (find(i->first)) {
            parse_error(*qore_enum_decl_private::get(*i->second)->getParseLocation(), "enum '%s' is already " \
                "pending in namespace '%s'", i->first, ns.name.c_str());
            n.remove(i);
        } else {
            // "move" data to new list
            hm[i->first] = i->second;
            qore_enum_decl_private::get(*(i->second))->setNamespace(&ns);
            // track the new enum name for rollback support
            if (pending_names) {
                pending_names->push_back(i->first);
            }
            n.hm.erase(i);
        }
        i = n.hm.begin();
    }
}

QoreHashNode* EnumList::getInfo() {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    for (auto& i : hm) {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), nullptr);
        const QoreEnumDecl* ed = i.second;

        // Add base type
        info->setKeyValue("base_type", new QoreStringNode(QoreTypeInfo::getName(ed->getBaseTypeInfo())), nullptr);

        // Add members
        ReferenceHolder<QoreHashNode> members(new QoreHashNode(autoTypeInfo), nullptr);
        QoreEnumMemberIterator mi(*ed);
        while (mi.next()) {
            members->setKeyValue(mi.getName(), mi.getValue().refSelf(), nullptr);
        }
        info->setKeyValue("members", members.release(), nullptr);

        rv->setKeyValue(i.first, info.release(), nullptr);
    }
    return rv.release();
}

bool EnumListIterator::isPublic() const {
    return qore_enum_decl_private::get(*i->second)->isPublic();
}

bool EnumListIterator::isUserPublic() const {
    return qore_enum_decl_private::get(*i->second)->isUserPublic();
}

bool ConstEnumListIterator::isPublic() const {
    return qore_enum_decl_private::get(*i->second)->isPublic();
}

bool ConstEnumListIterator::isUserPublic() const {
    return qore_enum_decl_private::get(*i->second)->isUserPublic();
}

void EnumList::parseRemove(const char* name) {
    hm_qen_t::iterator i = hm.find(name);
    if (i != hm.end()) {
        remove(i);
    }
}
