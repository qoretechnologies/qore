/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LexicalReflection.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_LEXICALREFLECTION_H
#define _QORE_LEXICALREFLECTION_H

#include <qore/Qore.h>
#include <qore/QoreReflection.h>
#include <qore/intern/LexicalSemanticPolicy.h>
#include <utility>

#if defined(__GNUC__) || defined(__clang__)
#define QORE_LEXICAL_REFLECTION_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define QORE_LEXICAL_REFLECTION_INLINE __forceinline
#else
#define QORE_LEXICAL_REFLECTION_INLINE inline
#endif

//! Internal core/reflection interface, versioned with the module API.
/** Registration is scoped to the initializing thread. The immutable alternate body is copied into the method
    variant before publication; no registry lookup, policy check or TLS read occurs on ordinary calls.
*/
class QoreLexicalMethodRegistrationHelper {
public:
    using Entry = std::pair<q_method_t, q_method_t>;

    DLLEXPORT QoreLexicalMethodRegistrationHelper(const Entry* entries, size_t count);
    DLLEXPORT ~QoreLexicalMethodRegistrationHelper();

    template <size_t N>
    explicit QoreLexicalMethodRegistrationHelper(const Entry (&entries)[N])
            : QoreLexicalMethodRegistrationHelper(entries, N) {
    }

    DLLLOCAL static q_method_t find(q_method_t method);

    QoreLexicalMethodRegistrationHelper(const QoreLexicalMethodRegistrationHelper&) = delete;
    QoreLexicalMethodRegistrationHelper& operator=(const QoreLexicalMethodRegistrationHelper&) = delete;

private:
    const Entry* entries;
    size_t count;
    const QoreLexicalMethodRegistrationHelper* previous;
    static thread_local const QoreLexicalMethodRegistrationHelper* active;
};

//! Explicitly source-bound entries for reflection; the public C++ call APIs remain canonical.
DLLEXPORT QoreObject* qore_lexical_reflection_copy(LexicalSemanticPolicy policy, const QoreClass& cls,
    QoreObject* object, ExceptionSink* xsink);
DLLEXPORT QoreObject* qore_lexical_reflection_construct(LexicalSemanticPolicy policy, const QoreClass& cls,
    const QoreExternalMethodVariant* variant, const QoreListNode* args, const QoreTypeInfo* object_type,
    ExceptionSink* xsink);
DLLEXPORT QoreValue qore_lexical_reflection_method(LexicalSemanticPolicy policy, QoreObject* self,
    const QoreMethod& method, const QoreClass* class_ctx, const QoreExternalMethodVariant* variant,
    const QoreListNode* args, ExceptionSink* xsink);
DLLEXPORT QoreValue qore_lexical_reflection_pseudo(LexicalSemanticPolicy policy, const QoreClass& cls,
    const QoreMethod* method, const QoreExternalMethodVariant* variant, QoreValue value,
    const QoreListNode* args, ExceptionSink* xsink);
DLLEXPORT QoreValue qore_lexical_reflection_function(LexicalSemanticPolicy policy,
    const QoreExternalFunction& function, const QoreExternalVariant* variant, const QoreListNode* args,
    QoreProgram* pgm, const QoreNamespace* ns, const QoreReflectionCaller& caller, ExceptionSink* xsink);

#endif
