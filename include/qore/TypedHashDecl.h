/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TypedHashDecl.h

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

#ifndef _QORE_TYPEDHASHDECL_H

#define _QORE_TYPEDHASHDECL_H

// forward references
class typed_hash_decl_private;
class QoreExternalMemberBase;
class QoreExternalProgramLocation;

//! typed hash declaration
/** @since %Qore 0.8.13
 */
class TypedHashDecl {
    friend class typed_hash_decl_private;

public:
    DLLEXPORT TypedHashDecl(const char* name, const char* path);

    DLLEXPORT TypedHashDecl(const TypedHashDecl& old);

    //! returns the type info object for the hashdecl
    DLLEXPORT const QoreTypeInfo* getTypeInfo(bool or_nothing = false) const;

    //! adds an element to a built-in hashdecl
    DLLEXPORT void addMember(const char* name, const QoreTypeInfo* memberTypeInfo, QoreValue init_val);

    //! returns the name of the typed hash
    DLLEXPORT const char* getName() const;

    //! returns true if the typed hash is a builtin typed hash
    DLLEXPORT bool isSystem() const;

    //! returns true if the typed hash has the public (export) flag set
    /** @since %Qore 0.9.3
    */
    DLLEXPORT bool isPublic() const;

    //! Finds the given local member or returns nullptr
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalMemberBase* findLocalMember(const char* name) const;

    //! returns the source location of the typed hash (hashdecl) definition
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

    //! returns the full namespace path of the class
    /** @param anchored if true then the path will always be prefixed by "::" for the unnamed root namespace

        @since %Qore 0.9
    */
    DLLEXPORT std::string getNamespacePath(bool anchored = false) const;

    //! returns true if the hashdecl passed as an arugment is equal to this hashdecl
    /**
        @since %Qore 0.9
    */
    DLLEXPORT bool equal(const TypedHashDecl* other) const;

    //! Returns the module name the class was loaded from or nullptr if it is a builtin class
    /** @since %Qore 0.9
    */
    DLLEXPORT const char* getModuleName() const;

    //! Returns the namespace owning the typed hash declaration
    /** @since %Qore 0.9.4
    */
    DLLEXPORT const QoreNamespace* getNamespace() const;

    //! Performs a runtime cast and returns a typed hash if the has passed is compatible
    /** The caller owns any reference retuned.  Throws a %Qore-language exception if the hash is not compatible with
        with the typed hash

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreHashNode* doRuntimeCast(const QoreHashNode* h, ExceptionSink* xsink) const;

    //! Returns the parent hashdecl if this hashdecl inherits from another, or nullptr if none
    /** @return the parent hashdecl or nullptr

        @since %Qore 2.3
    */
    DLLEXPORT const TypedHashDecl* getParentHashDecl() const;

    //! Returns true if this hashdecl inherits from the given hashdecl
    /** @param parent the hashdecl to check for inheritance

        @return true if this hashdecl is a descendant of the given hashdecl, false otherwise

        @since %Qore 2.3
    */
    DLLEXPORT bool inheritsFrom(const TypedHashDecl* parent) const;

    //! Sets the parent hashdecl for system hashdecl inheritance
    /** @param parent the parent hashdecl

        @note This method is intended for use by system hashdecls only

        @since %Qore 2.3
    */
    DLLEXPORT void setParent(const TypedHashDecl* parent);

protected:
    //! deletes the object and frees all memory
    DLLEXPORT ~TypedHashDecl();

private:
    DLLEXPORT TypedHashDecl(typed_hash_decl_private* p);

    typed_hash_decl_private* priv;
};

//! allows for temporary storage of a TypedHashDecl pointer
/** @since %Qore 0.8.13
 */
class TypedHashDeclHolder {
public:
    //! creates the object
    DLLLOCAL TypedHashDeclHolder(TypedHashDecl* thd) : thd(thd) {
    }

    //! deletes the TypedHashDecl object if still managed
    DLLEXPORT ~TypedHashDeclHolder();

    //! implicit conversion to TypedHashDecl*
    DLLLOCAL TypedHashDecl* operator*() const {
        return thd;
    }

    //! implicit conversion to TypedHashDecl*
    DLLLOCAL TypedHashDecl* operator->() const {
        return thd;
    }

    //! assign new TypedHashDecl value; any managed object is deleted if still managed
    DLLLOCAL TypedHashDecl* operator=(TypedHashDecl* nhd);

    //! releases the TypedHashDecl*
    DLLLOCAL TypedHashDecl* release() {
        auto rv = thd;
        thd = nullptr;
        return rv;
    }

private:
    //! the object being managed
    TypedHashDecl* thd;
};

//! Allows iteration of a hashdecl's members
class TypedHashDeclMemberIterator {
public:
    DLLEXPORT TypedHashDeclMemberIterator(const TypedHashDecl& thd);

    DLLEXPORT ~TypedHashDeclMemberIterator();

    DLLEXPORT bool next();

    DLLEXPORT const QoreExternalMemberBase& getMember() const;

    DLLEXPORT const char* getName() const;

private:
    class typed_hash_decl_member_iterator* priv;
};

//! StatInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclStatInfo;

//! DirStatInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclDirStatInfo;

//! FilesystemStatInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclFilesystemInfo;

//! DateTimeInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclDateTimeInfo;

//! IsoWeekInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclIsoWeekInfo;

//! CallStackInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclCallStackInfo;

//! ExceptionInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclExceptionInfo;

//! StatementInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclStatementInfo;

//! NetIfInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclNetIfInfo;

//! SourceLocationInfo hashdecl
DLLEXPORT extern const TypedHashDecl* hashdeclSourceLocationInfo;

//! SerializationInfo hashdecl
/** @since %Qore 0.9
*/
DLLEXPORT extern const TypedHashDecl* hashdeclSerializationInfo;

//! ObjectSerializationInfo hashdecl
/** @since %Qore 0.9
*/
DLLEXPORT extern const TypedHashDecl* hashdeclObjectSerializationInfo;

//! IndexedObjectSerializationInfo hashdecl
/** @since %Qore 0.9
*/
DLLEXPORT extern const TypedHashDecl* hashdeclIndexedObjectSerializationInfo;

//! HashSerializationInfo hashdecl
/** @since %Qore 0.9
*/
DLLEXPORT extern const TypedHashDecl* hashdeclHashSerializationInfo;

//! ListSerializationInfo hashdecl
/** @since %Qore 0.9.1
*/
DLLEXPORT extern const TypedHashDecl* hashdeclListSerializationInfo;

//! UrlInfo hashdecl
/** @since %Qore 0.9.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclUrlInfo;

//! FtpResponseInfo hashdecl
/** @since %Qore 0.9.4
*/
DLLEXPORT extern const TypedHashDecl* hashdeclFtpResponseInfo;

//! SocketPollInfo hashdecl
/** @since %Qore 0.9.11
*/
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollInfo;

//! PipeInfo hashdecl
/** @since %Qore 1.12
*/
DLLEXPORT extern const TypedHashDecl* hashdeclPipeInfo;

//! SseMessageInfo hashdecl
/** @since %Qore 2.0
*/
DLLEXPORT extern const TypedHashDecl* hashdeclSseMessageInfo;

//! PortRangeInfo hashdecl
/** @since %Qore 2.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclPortRangeInfo;

//! FilesystemPathInfo hashdecl
/** @since %Qore 2.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclFilesystemPathInfo;

//! FilesystemSecurityConfigInfo hashdecl
/** @since %Qore 2.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclFilesystemSecurityConfigInfo;

//! NetworkSecurityConfigInfo hashdecl
/** @since %Qore 2.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclNetworkSecurityConfigInfo;

//! SandboxConfigInfo hashdecl
/** @since %Qore 2.3
*/
DLLEXPORT extern const TypedHashDecl* hashdeclSandboxConfigInfo;

#endif
