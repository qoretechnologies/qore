/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_enum_decl_private.h

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

#ifndef _QORE_INTERN_QORE_ENUM_DECL_PRIVATE_H

#define _QORE_INTERN_QORE_ENUM_DECL_PRIVATE_H

#include <qore/QoreEnumDecl.h>

#include <map>
#include <string>
#include <vector>

class qore_ns_private;
class QoreEnumTypeInfo;
class QoreEnumOrNothingTypeInfo;

// Internal representation of an enum member
class qore_enum_member_private {
public:
    //! Creates a new enum member; takes ownership of the value (caller must pass an already-referenced value)
    DLLLOCAL qore_enum_member_private(const char* n, QoreValue v) : name(n), val(v) {
    }

    DLLLOCAL qore_enum_member_private(const qore_enum_member_private& old) : name(old.name), val(old.val) {
        val.ref();
    }

    DLLLOCAL ~qore_enum_member_private() {
        val.discard(nullptr);
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL QoreValue getValue() const {
        return val;
    }

private:
    std::string name;
    QoreValue val;
};

class qore_enum_decl_private {
    friend class qore_enum_member_iterator_private;
    friend class QoreEnumDecl;
    friend class QoreEnumMember;

public:
    DLLLOCAL qore_enum_decl_private(const QoreProgramLocation* loc) : loc(loc), orig(this) {
        assignModule();
    }

    DLLLOCAL qore_enum_decl_private(const QoreProgramLocation* loc, const char* n, const char* p,
            const QoreTypeInfo* baseType, QoreEnumDecl* ed);

    DLLLOCAL qore_enum_decl_private(const qore_enum_decl_private& old, QoreEnumDecl* ed);

    DLLLOCAL ~qore_enum_decl_private();

    DLLLOCAL QoreEnumDecl* newEnumDecl(const char* n, const QoreTypeInfo* baseType);

    DLLLOCAL bool equal(const qore_enum_decl_private& other) const;

    DLLLOCAL bool parseEqual(const qore_enum_decl_private& other) const {
        const_cast<qore_enum_decl_private*>(this)->parseInit();
        return equal(other);
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo(bool or_nothing = false) const {
        return or_nothing ? orNothingTypeInfo : typeInfo;
    }

    DLLLOCAL const QoreTypeInfo* getBaseTypeInfo() const {
        return baseTypeInfo;
    }

    DLLLOCAL bool isPublic() const {
        return pub;
    }

    DLLLOCAL bool isUserPublic() const {
        return pub && !sys;
    }

    DLLLOCAL void setPublic() {
        assert(!pub);
        pub = true;
    }

    DLLLOCAL void setPrivate() {
        assert(pub);
        pub = false;
    }

    DLLLOCAL bool isSystem() const {
        return sys;
    }

    DLLLOCAL void ref() const {
        refs.ROreference();
    }

    DLLLOCAL bool deref() {
        if (refs.ROdereference()) {
            assert(ed->priv == this);
            delete ed;
            return true;
        }
        return false;
    }

    DLLLOCAL int parseInit();

    // Add a member with automatic value (for parsed enums)
    DLLLOCAL void parseAddMember(const char* name, QoreValue val);

    // Add a member for builtin enums
    DLLLOCAL void addMember(const char* name, QoreValue val);

    DLLLOCAL const QoreEnumMember* findMember(const char* name) const;

    DLLLOCAL const QoreEnumMember* findMemberByValue(const QoreValue& val) const;

    DLLLOCAL size_t getMemberCount() const {
        return members.size();
    }

    DLLLOCAL bool hasMember(const char* name) const {
        return member_map.find(name) != member_map.end();
    }

    DLLLOCAL void setSystemPublic() {
        assert(!sys);
        assert(!pub);
        sys = true;
        pub = true;
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const char* getPath() const {
        return path.c_str();
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL const QoreProgramLocation* getParseLocation() const {
        return loc;
    }

    DLLLOCAL static qore_enum_decl_private* get(QoreEnumDecl& enumdecl) {
        return enumdecl.priv;
    }

    DLLLOCAL static const qore_enum_decl_private* get(const QoreEnumDecl& enumdecl) {
        return enumdecl.priv;
    }

    DLLLOCAL void setName(const char* n) {
        name = n;
    }

    DLLLOCAL void setNamespace(qore_ns_private* n) {
        ns = n;
    }

    DLLLOCAL const qore_ns_private* getNamespace() const {
        return ns;
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    //! Checks if the value is valid for this enum
    DLLLOCAL bool isValidValue(const QoreValue& val) const;

    //! Returns all members as a vector
    DLLLOCAL const std::vector<QoreEnumMember*>& getMembers() const {
        return members;
    }

protected:
    // references
    mutable QoreReferenceCounter refs;
    const QoreProgramLocation* loc;
    std::string name;
    std::string path;
    QoreEnumDecl* ed = nullptr;
    // parent namespace
    qore_ns_private* ns = nullptr;
    // the module that defined this enum, if any
    std::string from_module;

    // original declaration
    const qore_enum_decl_private* orig;

    // type information
    QoreEnumTypeInfo* typeInfo = nullptr;
    QoreEnumOrNothingTypeInfo* orNothingTypeInfo = nullptr;

    // base type info (int, string, float, number, date, binary)
    const QoreTypeInfo* baseTypeInfo = nullptr;

    // member information - stored in order of declaration
    std::vector<QoreEnumMember*> members;
    // member lookup by name
    std::map<std::string, QoreEnumMember*> member_map;

    bool pub = false;
    bool sys = false;

    bool parse_init_done = false;

    DLLLOCAL void assignModule() {
        const char* mod_name = get_module_context_name();
        if (mod_name) {
            from_module = mod_name;
        }
    }
};

class qore_enum_member_iterator_private {
public:
    DLLLOCAL qore_enum_member_iterator_private(const QoreEnumDecl& ed)
            : priv(qore_enum_decl_private::get(ed)), pos(priv->members.begin()) {
    }

    DLLLOCAL bool next() {
        if (!valid) {
            valid = true;
            return pos != priv->members.end();
        }
        if (pos == priv->members.end()) {
            return false;
        }
        ++pos;
        return pos != priv->members.end();
    }

    DLLLOCAL const QoreEnumMember* getMember() const {
        assert(valid && pos != priv->members.end());
        return *pos;
    }

    DLLLOCAL const char* getName() const {
        assert(valid && pos != priv->members.end());
        return (*pos)->getName();
    }

    DLLLOCAL QoreValue getValue() const {
        assert(valid && pos != priv->members.end());
        return (*pos)->getValue();
    }

private:
    const qore_enum_decl_private* priv;
    std::vector<QoreEnumMember*>::const_iterator pos;
    bool valid = false;
};

#endif
