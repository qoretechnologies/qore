/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEnumDecl.h

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

#ifndef _QORE_QOREENUMDECL_H
#define _QORE_QOREENUMDECL_H

#include <qore/common.h>

#include <string>

// forward declarations
class qore_enum_decl_private;
class QoreNamespace;
class QoreTypeInfo;
class QoreValue;
class ExceptionSink;

//! Represents an enum member with name and value
/** @since %Qore 2.1
*/
class QoreEnumMember {
    friend class qore_enum_decl_private;

public:
    //! returns the member name
    DLLEXPORT const char* getName() const;

    //! returns the member value
    DLLEXPORT QoreValue getValue() const;

private:
    DLLLOCAL QoreEnumMember(const char* name, QoreValue val);
    DLLLOCAL ~QoreEnumMember();

    class qore_enum_member_private* priv;
};

//! Enum declaration
/** @since %Qore 2.1
*/
class QoreEnumDecl {
    friend class qore_enum_decl_private;

public:
    //! creates the enum with the given name and path
    DLLEXPORT QoreEnumDecl(const char* name, const char* path, const QoreTypeInfo* baseType);

    //! copy constructor
    DLLEXPORT QoreEnumDecl(const QoreEnumDecl& old);

    //! returns the type info object for the enum
    DLLEXPORT const QoreTypeInfo* getTypeInfo(bool or_nothing = false) const;

    //! returns the base type info for the enum (int, string, etc.)
    DLLEXPORT const QoreTypeInfo* getBaseTypeInfo() const;

    //! adds a member to a built-in enum
    DLLEXPORT void addMember(const char* name, QoreValue val);

    //! returns the name of the enum
    DLLEXPORT const char* getName() const;

    //! returns true if the enum is a builtin enum
    DLLEXPORT bool isSystem() const;

    //! returns true if the enum has the public (export) flag set
    DLLEXPORT bool isPublic() const;

    //! finds the given member by name or returns nullptr
    DLLEXPORT const QoreEnumMember* findMember(const char* name) const;

    //! finds the given member by value or returns nullptr
    DLLEXPORT const QoreEnumMember* findMemberByValue(const QoreValue& val) const;

    //! returns the number of members
    DLLEXPORT size_t getMemberCount() const;

    //! returns the source location of the enum definition
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

    //! returns the full namespace path of the enum
    DLLEXPORT std::string getNamespacePath(bool anchored = false) const;

    //! returns true if the enum passed as an argument is equal to this enum
    DLLEXPORT bool equal(const QoreEnumDecl* other) const;

    //! Returns the module name the enum was loaded from or nullptr if it is a builtin enum
    DLLEXPORT const char* getModuleName() const;

    //! Returns the namespace owning the enum declaration
    DLLEXPORT const QoreNamespace* getNamespace() const;

    //! Validates that the given value is valid for this enum
    /** @return true if valid, false otherwise
    */
    DLLEXPORT bool isValidValue(const QoreValue& val) const;

protected:
    //! deletes the object and frees all memory
    DLLEXPORT ~QoreEnumDecl();

private:
    DLLEXPORT QoreEnumDecl(qore_enum_decl_private* p);

    qore_enum_decl_private* priv;
};

//! allows for temporary storage of a QoreEnumDecl pointer
/** @since %Qore 2.1
*/
class QoreEnumDeclHolder {
    friend class qore_enum_decl_private;

public:
    //! creates the object
    DLLLOCAL QoreEnumDeclHolder(QoreEnumDecl* ed) : e(ed) {
    }

    //! deletes the QoreEnumDecl object if still managed
    DLLEXPORT ~QoreEnumDeclHolder();

    //! implicit conversion to QoreEnumDecl*
    DLLLOCAL operator QoreEnumDecl*() const {
        return e;
    }

    //! releases the QoreEnumDecl*
    DLLLOCAL QoreEnumDecl* release() {
        QoreEnumDecl* rv = e;
        e = nullptr;
        return rv;
    }

    //! returns the QoreEnumDecl*
    DLLLOCAL QoreEnumDecl* operator->() {
        return e;
    }

    //! returns the QoreEnumDecl*
    DLLLOCAL QoreEnumDecl* operator*() {
        return e;
    }

private:
    QoreEnumDecl* e;
};

//! Iterator for enum members
/** @since %Qore 2.1
*/
class QoreEnumMemberIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreEnumMemberIterator(const QoreEnumDecl& ed);

    //! destroys the iterator
    DLLEXPORT ~QoreEnumMemberIterator();

    //! moves to the next element, returns true if valid
    DLLEXPORT bool next();

    //! returns the current member
    DLLEXPORT const QoreEnumMember* getMember() const;

    //! returns the member name
    DLLEXPORT const char* getName() const;

    //! returns the member value
    DLLEXPORT QoreValue getValue() const;

private:
    class qore_enum_member_iterator_private* priv;
};

#endif // _QORE_QOREENUMDECL_H
