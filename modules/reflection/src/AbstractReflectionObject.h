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

//! Checks that the caller may read or write values in the Program of a reflection object
/** See QoreReflectionCaller::checkProgramAccess() and design/reflection-caller-privilege.md

    @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_program_access(const QoreReflectionCaller& caller, const QoreProgram* pgm,
        const char* what, ExceptionSink* xsink);

//! Checks declaration navigation without comparing explicitly-allowed domains
/** @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised */
DLLLOCAL int reflection_check_structural_access(const QoreReflectionCaller& caller, const QoreProgram* pgm,
        const char* what, ExceptionSink* xsink);

//! Checks the functional domain of a class for a method, constructor, copy method or destructor called through reflection
/** The caller and the Program of the reflection object must both allow the class's domain.

    @param create true to raise CREATE-OBJECT-ERROR (creating an object), false to raise INVALID-FUNCTION-ACCESS

    @return 0 if allowed, -1 if an exception was raised
*/
DLLLOCAL int reflection_check_class_call(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, bool create, ExceptionSink* xsink);

//! Checks that a class is exported by the user module that declares it, for callers its export boundary applies to
/** Used for the operations that the language allows only with the class's name: construction, static methods,
    static members and constants, and member initializers (see QoreReflectionCaller::canSeeClass())

    @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_class_export(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, ExceptionSink* xsink);

//! Returns true if the caller may see a typed hash: it is checked against the Program that declares it
/** As for a class (QoreReflectionCaller::canSeeClass()): a handle to a typed hash can belong to another Program
    than the one that declares it (Type::getTypedHash() makes it for the calling code's Program when the type has no
    source Program), and the export boundary of a user module is decided on the declaring Program, else a value of a
    typed hash that the module does not export would let the caller run its initializers.  A typed hash that another
    Program imported belongs to that Program.

    @param caller the calling code
    @param target the Program of the reflection handle; used if the typed hash has no Program
    @param th the typed hash
    @param ns the namespace of the typed hash recorded in the handle
*/
DLLLOCAL bool reflection_can_see_typed_hash(const QoreReflectionCaller& caller, const QoreProgram* target,
        const TypedHashDecl* th, const QoreNamespace* ns);

//! Checks that a typed hash is exported by the user module that declares it, for callers its export boundary applies to
/** Used for the operations that evaluate its member initializers: casts and member default values; see
    reflection_can_see_typed_hash()

    @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_typed_hash_export(const QoreReflectionCaller& caller, const QoreProgram* target,
        const TypedHashDecl* th, const QoreNamespace* ns, ExceptionSink* xsink);

//! Checks the functional domain of a class whose members or constants are read or written through reflection
/** @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_class_value_access(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, ExceptionSink* xsink);

//! Checks that the caller may use a method through reflection, whichever of its variants is selected
/** For callers to which the language's member access applies (QoreReflectionCaller::appliesMemberAccess()), every
    variant of the method must be accessible, as the variant is selected for the arguments at run time

    @return 0 if allowed, -1 if REFLECTION-ACCESS-ERROR was raised
*/
DLLLOCAL int reflection_check_method_access(const QoreReflectionCaller& caller, const QoreMethod* m,
        ExceptionSink* xsink);

//! Checks the functional domain of a function or method variant called through reflection
/** For a method variant, also checks that the caller may use it (QoreReflectionCaller::checkClassElementAccess())

    @param cname the class name for a method variant, or nullptr for a function variant
    @param fname the function or method name

    @return 0 if allowed, -1 if INVALID-FUNCTION-ACCESS or REFLECTION-ACCESS-ERROR was raised
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

//! The export boundary of a user module for lookups in a namespace of a reflection object's Program
/** A caller to which the boundary applies (QoreReflectionCaller::appliesModuleExports()) finds in a module's
    namespaces only what the module exports: other declarations are treated as if they did not exist.  See
    @ref reflection_caller_privilege.
*/
class ReflectionExportFilter {
public:
    DLLLOCAL ReflectionExportFilter(const QoreReflectionCaller& caller, const QoreProgram* pgm,
            const QoreNamespace* ns) : limited(caller.appliesModuleExports(pgm)),
            ns_visible(!limited || caller.canSeeNamespace(pgm, ns)) {
    }

    //! Returns true if a declaration in the namespace is visible; \a exported: it is public or builtin
    DLLLOCAL bool visible(bool exported) const {
        return !limited || (ns_visible && exported);
    }

    //! Returns true if a class found in the namespace is visible
    DLLLOCAL bool visibleClass(const QoreReflectionCaller& caller, const QoreProgram* pgm,
            const QoreClass* cls) const {
        return !limited || (ns_visible && caller.canSeeClass(pgm, cls));
    }

    //! Returns true if a child namespace of the namespace is visible
    DLLLOCAL bool visibleNamespace(const QoreReflectionCaller& caller, const QoreProgram* pgm,
            const QoreNamespace* ns) const {
        return !limited || (ns_visible && caller.canSeeNamespace(pgm, ns));
    }

private:
    bool limited;
    bool ns_visible;
};

//! The caller of a lookup by path or pattern, for the export boundary of user modules
/** A lookup in the calling code's own Program, or in a Program that is not a user module, cannot cross the export
    boundary of a module, so the caller is captured (QoreReflectionCaller) only for a lookup in another user module's
    Program; lookups made without a Program argument thus stay as cheap as before.
*/
class ReflectionLookupCaller {
public:
    //! Captures the caller only if a lookup in \a lookup_pgm can cross the export boundary of a user module
    DLLLOCAL explicit ReflectionLookupCaller(const QoreProgram* lookup_pgm) {
        if (lookup_pgm && lookup_pgm != getProgram()
                && lookup_pgm->getParseOptions().hasAny(QoreParseOptions(PO_IN_MODULE))) {
            own.reset(new QoreReflectionCaller);
            caller = own.get();
        }
    }

    //! Uses a caller that the entry point has already captured
    DLLLOCAL ReflectionLookupCaller(const QoreReflectionCaller& caller) : caller(&caller) {
    }

    DLLLOCAL bool canSeeNamespace(const QoreProgram* pgm, const QoreNamespace* ns) const {
        return !caller || caller->canSeeNamespace(pgm, ns);
    }

    DLLLOCAL bool canSeeDeclaration(const QoreProgram* pgm, const QoreNamespace* ns, bool exported) const {
        return !caller || caller->canSeeDeclaration(pgm, ns, exported);
    }

    DLLLOCAL bool canSeeClass(const QoreProgram* pgm, const QoreClass* cls) const {
        return !caller || caller->canSeeClass(pgm, cls);
    }

private:
    std::unique_ptr<QoreReflectionCaller> own;
    const QoreReflectionCaller* caller = nullptr;
};

//! Returns true if a function is exported by the module that declares it (public), or builtin
static inline bool reflection_exported(const QoreExternalFunction* f) {
    return f->isModulePublic() || f->isBuiltin();
}

//! Returns true if a constant is exported by the module that declares it (public), or builtin
static inline bool reflection_exported(const QoreExternalConstant* c) {
    return c->isModulePublic() || c->isBuiltin();
}

//! Returns true if a global variable is exported by the module that declares it (public), or builtin
static inline bool reflection_exported(const QoreExternalGlobalVar* v) {
    return v->isModulePublic() || v->isBuiltin();
}

//! Returns true if a typed hash is exported by the module that declares it (public), or builtin
static inline bool reflection_exported(const TypedHashDecl* th) {
    return th->isPublic() || th->isSystem();
}

//! Returns true if an enum is exported by the module that declares it (public), or builtin
static inline bool reflection_exported(const QoreEnumDecl* ed) {
    return ed->isPublic() || ed->isSystem();
}

//! returns the Program used to resolve reflection lookups: the call's RuntimeConfig Program, else the current one
static inline QoreProgram* get_reflection_lookup_program(RuntimeConfig& runtime_cfg) {
    QoreProgram* pgm = runtime_cfg.getProgram();
    return pgm ? pgm : getProgram();
}

#endif
