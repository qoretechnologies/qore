/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEnumDecl.cpp

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

#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreTypeInfo.h"

// QoreTypeSpec::getType() implementation
qore_type_t QoreTypeSpec::getType() const {
    switch (typespec) {
        case QTS_TYPE:
        case QTS_EMPTYLIST:
        case QTS_EMPTYHASH:
            return u.t;
        case QTS_CLASS:
            return NT_OBJECT;
        case QTS_COMPLEXHASH:
        case QTS_HASHDECL:
            return NT_HASH;
        case QTS_COMPLEXLIST:
        case QTS_COMPLEXSOFTLIST:
            return NT_LIST;
        case QTS_HARDREF:
        case QTS_COMPLEXHARDREF:
        case QTS_COMPLEXREF:
            return NT_REFERENCE;
        case QTS_ENUM:
            // Enum types return the base type of the enum
            return QoreTypeInfo::getBaseType(u.ed->getBaseTypeInfo());
    }
    assert(false);
    return NT_NOTHING;
}

// QoreEnumTypeInfo constructor implementation
// Only accepts enum type - base type values require explicit cast
QoreEnumTypeInfo::QoreEnumTypeInfo(const QoreEnumDecl* ed, const char* name, const char* path)
        : QoreTypeInfo(q_accept_vec_t {
            {ed, nullptr, true},
        }, q_return_vec_t {{ed, true}},
        QoreStringMaker("enum<%s>", name)) {
    assert(path && *path);
    pname = "enum<";
    pname.append(path);
    pname.append(">");
}

// QoreEnumOrNothingTypeInfo constructor implementation
// Only accepts enum type or NOTHING - base type values require explicit cast
QoreEnumOrNothingTypeInfo::QoreEnumOrNothingTypeInfo(const QoreEnumDecl* ed, const char* name, const char* path)
        : QoreEnumTypeInfo(q_accept_vec_t {
            {ed, nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{ed}, {NT_NOTHING}}, QoreStringMaker("*enum<%s>", name), path) {
    pname = "*enum<";
    pname.append(path);
    pname.append(">");
}

// QoreEnumTypeInfo::getDefaultQoreValueImpl() implementation
QoreValue QoreEnumTypeInfo::getDefaultQoreValueImpl() const {
    const QoreEnumDecl* ed = accept_vec[0].spec.getEnum();
    // Return a TAG_ENUM value of the first member if there are any
    if (ed->getMemberCount() > 0) {
        QoreEnumMemberIterator it(*ed);
        if (it.next()) {
            return QoreValue::makeEnum(it.getMember());
        }
    }
    // Otherwise return the default value for the base type
    return QoreTypeInfo::getDefaultQoreValue(ed->getBaseTypeInfo());
}

// QoreEnumMember implementation
QoreEnumMember::QoreEnumMember(const char* name, QoreValue val, const QoreEnumDecl* parent)
        : priv(new qore_enum_member_private(name, val, parent)) {
}

QoreEnumMember::~QoreEnumMember() {
    delete priv;
}

const char* QoreEnumMember::getName() const {
    return priv->getName();
}

QoreValue QoreEnumMember::getValue() const {
    return priv->getValue();
}

const QoreEnumDecl* QoreEnumMember::getEnumDecl() const {
    return priv->getEnumDecl();
}

// qore_enum_decl_private implementation
qore_enum_decl_private::qore_enum_decl_private(const QoreProgramLocation* loc, const char* n, const char* p,
        const QoreTypeInfo* baseType, QoreEnumDecl* ed)
        : loc(loc), name(n), path(p), ed(ed), orig(this), baseTypeInfo(baseType),
          typeInfo(new QoreEnumTypeInfo(ed, n, p)),
          orNothingTypeInfo(new QoreEnumOrNothingTypeInfo(ed, n, p)) {
    assignModule();
}

qore_enum_decl_private::qore_enum_decl_private(const qore_enum_decl_private& old, QoreEnumDecl* ed)
        : loc(old.loc), name(old.name), path(old.path), ed(ed),
          from_module(old.from_module), orig(old.orig), baseTypeInfo(old.baseTypeInfo),
          typeInfo(new QoreEnumTypeInfo(ed, old.name.c_str(), old.path.c_str())),
          orNothingTypeInfo(new QoreEnumOrNothingTypeInfo(ed, old.name.c_str(), old.path.c_str())),
          pub(old.pub), sys(old.sys) {
    // Copy members - must ref the value since constructor takes ownership
    for (auto* member : old.members) {
        QoreEnumMember* new_member = new QoreEnumMember(member->getName(), member->getValue().refSelf(), ed);
        members.push_back(new_member);
        member_map[new_member->getName()] = new_member;
    }
}

qore_enum_decl_private::~qore_enum_decl_private() {
    assert(!refs.reference_count());
    delete typeInfo;
    delete orNothingTypeInfo;
    for (auto* member : members) {
        delete member;
    }
}

QoreEnumDecl* qore_enum_decl_private::newEnumDecl(const char* n, const QoreTypeInfo* baseType) {
    assert(name.empty());
    assert(!ed);
    name = n;
    path = get_ns_path(n);
    baseTypeInfo = baseType;
    ed = new QoreEnumDecl(this);
    typeInfo = new QoreEnumTypeInfo(ed, name.c_str(), path.c_str());
    orNothingTypeInfo = new QoreEnumOrNothingTypeInfo(ed, name.c_str(), path.c_str());
    return ed;
}

bool qore_enum_decl_private::equal(const qore_enum_decl_private& other) const {
    if (name != other.name || members.size() != other.members.size()) {
        return false;
    }
    if (!QoreTypeInfo::equal(baseTypeInfo, other.baseTypeInfo)) {
        return false;
    }
    for (size_t i = 0; i < members.size(); ++i) {
        if (strcmp(members[i]->getName(), other.members[i]->getName()) != 0) {
            return false;
        }
        // Compare values - use isEqualHard for simple value types
        if (!members[i]->getValue().isEqualHard(other.members[i]->getValue())) {
            return false;
        }
    }
    return true;
}

int qore_enum_decl_private::parseInit() {
    if (parse_init_done || sys) {
        return 0;
    }
    parse_init_done = true;

    int err = 0;
    // Evaluate expression-valued members (e.g. "Default = -1" stores a unary minus expression)
    for (auto* member : members) {
        QoreValue& val = member->priv->val;
        if (val.needsEval()) {
            ExceptionSink xsink;
            ValueEvalOptimizedRefHolder v(val, &xsink);
            if (xsink) {
                qore_program_private::addParseException(getProgram(), xsink);
                err = -1;
                continue;
            }
            // Replace expression with resolved value
            val.discard(nullptr);
            val = v.takeReferencedValue();
        }
    }
    return err;
}

void qore_enum_decl_private::parseAddMember(const char* name, QoreValue val) {
    if (hasMember(name)) {
        // Duplicate member error - handled at parse time
        return;
    }
    QoreEnumMember* member = new QoreEnumMember(name, val, ed);
    members.push_back(member);
    member_map[name] = member;
}

void qore_enum_decl_private::addMember(const char* name, QoreValue val) {
    assert(!hasMember(name));
    QoreEnumMember* member = new QoreEnumMember(name, val, ed);
    members.push_back(member);
    member_map[name] = member;
}

const QoreEnumMember* qore_enum_decl_private::findMember(const char* name) const {
    auto it = member_map.find(name);
    if (it != member_map.end()) {
        return it->second;
    }
    return nullptr;
}

const QoreEnumMember* qore_enum_decl_private::findMemberByValue(const QoreValue& val) const {
    // Handle TAG_ENUM input: compare member pointers first
    if (val.isEnum()) {
        const QoreEnumMember* input_member = val.getEnumMember();
        // If from the same enum, find by pointer
        if (input_member->getEnumDecl() == ed) {
            return input_member;
        }
        // Different enum: compare by base value
        return findMemberByValue(input_member->getValue());
    }
    for (const auto* member : members) {
        if (member->getValue().isEqualHard(val)) {
            return member;
        }
    }
    return nullptr;
}

bool qore_enum_decl_private::isValidValue(const QoreValue& val) const {
    // Handle TAG_ENUM input
    if (val.isEnum()) {
        const QoreEnumMember* member = val.getEnumMember();
        if (member->getEnumDecl() == ed) {
            return true;
        }
        // Different enum: check the base value
        return findMemberByValue(member->getValue()) != nullptr;
    }
    return findMemberByValue(val) != nullptr;
}

// QoreEnumDecl public API implementation
QoreEnumDecl::QoreEnumDecl(const char* name, const char* path, const QoreTypeInfo* baseType)
        : priv(new qore_enum_decl_private(&loc_builtin, name, path, baseType, this)) {
}

QoreEnumDecl::QoreEnumDecl(const QoreEnumDecl& old)
        : priv(new qore_enum_decl_private(*old.priv, this)) {
}

QoreEnumDecl::QoreEnumDecl(qore_enum_decl_private* p) : priv(p) {
}

QoreEnumDecl::~QoreEnumDecl() {
    delete priv;
}

const QoreTypeInfo* QoreEnumDecl::getTypeInfo(bool or_nothing) const {
    return priv->getTypeInfo(or_nothing);
}

const QoreTypeInfo* QoreEnumDecl::getBaseTypeInfo() const {
    return priv->getBaseTypeInfo();
}

void QoreEnumDecl::addMember(const char* name, QoreValue val) {
    priv->addMember(name, val);
}

const char* QoreEnumDecl::getName() const {
    return priv->getName();
}

bool QoreEnumDecl::isSystem() const {
    return priv->isSystem();
}

bool QoreEnumDecl::isPublic() const {
    return priv->isPublic();
}

const QoreEnumMember* QoreEnumDecl::findMember(const char* name) const {
    return priv->findMember(name);
}

const QoreEnumMember* QoreEnumDecl::findMemberByValue(const QoreValue& val) const {
    return priv->findMemberByValue(val);
}

size_t QoreEnumDecl::getMemberCount() const {
    return priv->getMemberCount();
}

const QoreExternalProgramLocation* QoreEnumDecl::getSourceLocation() const {
    return reinterpret_cast<const QoreExternalProgramLocation*>(priv->getParseLocation());
}

std::string QoreEnumDecl::getNamespacePath(bool anchored) const {
    if (!priv->getNamespace()) {
        return anchored ? std::string("::") + priv->getName() : priv->getName();
    }
    std::string path;
    priv->getNamespace()->getPath(path, anchored);
    path += "::";
    path += priv->getName();
    return path;
}

bool QoreEnumDecl::equal(const QoreEnumDecl* other) const {
    if (!other) {
        return false;
    }
    return priv->equal(*other->priv);
}

const char* QoreEnumDecl::getModuleName() const {
    return priv->getModuleName();
}

const QoreNamespace* QoreEnumDecl::getNamespace() const {
    return priv->getNamespace() ? priv->getNamespace()->ns : nullptr;
}

bool QoreEnumDecl::isValidValue(const QoreValue& val) const {
    return priv->isValidValue(val);
}

// QoreEnumMemberIterator implementation
QoreEnumMemberIterator::QoreEnumMemberIterator(const QoreEnumDecl& ed)
        : priv(new qore_enum_member_iterator_private(ed)) {
}

QoreEnumMemberIterator::~QoreEnumMemberIterator() {
    delete priv;
}

bool QoreEnumMemberIterator::next() {
    return priv->next();
}

const QoreEnumMember* QoreEnumMemberIterator::getMember() const {
    return priv->getMember();
}

const char* QoreEnumMemberIterator::getName() const {
    return priv->getName();
}

QoreValue QoreEnumMemberIterator::getValue() const {
    return priv->getValue();
}

// QoreEnumDeclHolder implementation
QoreEnumDeclHolder::~QoreEnumDeclHolder() {
    if (e) {
        qore_enum_decl_private::get(*e)->deref();
    }
}
