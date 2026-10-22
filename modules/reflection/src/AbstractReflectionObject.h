/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AbstractReflectionObject.h reflection base class definition */
/*
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

#ifndef _QORE_INTERN_ABSTRACTREFLECTIONOBJECT_H

#define _QORE_INTERN_ABSTRACTREFLECTIONOBJECT_H

class AbstractReflectionObject : public AbstractPrivateData {
public:
    QoreProgram* pgm;

    DLLLOCAL AbstractReflectionObject(QoreProgram* pgm);

    DLLLOCAL ~AbstractReflectionObject();
};

//! access code modifiers
enum qore_modifier_t {
    MC_PUBLIC = (1 << 0),
    MC_PRIVATE = (1 << 1),
    MC_PRIVATEINTERNAL = (1 << 2),
    MC_ABSTRACT = (1 << 3),
    MC_STATIC = (1 << 4),
    MC_SYNCHRONIZED = (1 << 5),
    MC_DEPRECATED = (1 << 6),
    MC_FINAL = (1 << 7),
    MC_TRANSIENT = (1 << 8),
    MC_CONST = (1 << 9),
};

DLLLOCAL int get_access(ClassAccess access);

//! Checks that the caller may navigate or read or write values in the Program of a reflection object
/** See QoreReflectionCaller::checkProgramAccess() and design/reflection-caller-privilege.md

    @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_program_access(const QoreReflectionCaller& caller, const QoreProgram* pgm,
        const char* what, ExceptionSink* xsink);

//! Checks the functional domain of a class for a method, constructor, copy method or destructor called through reflection
/** The caller and the Program of the reflection object must both allow the class's domain.

    @param create true to raise CREATE-OBJECT-ERROR (creating an object), false to raise INVALID-FUNCTION-ACCESS

    @return 0 if allowed, -1 if an exception was raised
*/
DLLLOCAL int reflection_check_class_call(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, bool create, ExceptionSink* xsink);

//! Checks the functional domain of a class whose members or constants are read or written through reflection
/** @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_class_value_access(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, ExceptionSink* xsink);

//! Checks the functional domain of a function or method variant called through reflection
/** @param cname the class name for a method variant, or nullptr for a function variant
    @param fname the function or method name

    @return 0 if allowed, -1 if INVALID-FUNCTION-ACCESS was raised
*/
DLLLOCAL int reflection_check_variant_call(const QoreReflectionCaller& caller, const QoreExternalVariant* variant,
        const char* cname, const char* fname, const QoreProgram* target, ExceptionSink* xsink);

//! Returns the namespace in a reflection object's Program that corresponds to a namespace reached from a symbol
/** A symbol declared in one Program and inherited by another keeps referring to the namespace where it was declared;
    navigation from it stays in the reflection object's Program (see qore_program_find_namespace()).

    @return the namespace in \a pgm, or nullptr if \a pgm has none with the namespace's path (UNKNOWN-NAMESPACE
    is raised)
*/
DLLLOCAL const QoreNamespace* reflection_program_namespace(QoreProgram* pgm, const QoreNamespace* ns,
        ExceptionSink* xsink);

//! returns the Program used to resolve reflection lookups: the call's RuntimeConfig Program, else the current one
static inline QoreProgram* get_reflection_lookup_program(RuntimeConfig& runtime_cfg) {
    QoreProgram* pgm = runtime_cfg.getProgram();
    return pgm ? pgm : getProgram();
}

#endif
