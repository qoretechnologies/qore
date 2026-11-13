/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadScratch.h

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

#ifndef _QORE_INTERN_QORETHREADSCRATCH_H
#define _QORE_INTERN_QORETHREADSCRATCH_H

//! the holder of a per-thread scratch object of qore_thread_scratch(), which value-initializes it
template <typename T>
struct QoreThreadScratchHolder {
    T obj{};
};

//! returns a per-thread scratch object of type T, allocated on the heap when a thread first asks for it
/** Large per-thread buffers must not be declared as \c static \c thread_local objects: the static TLS block of
    libqore is reserved in every thread of the process, and glibc takes it from the thread's own stack, so each such
    buffer takes its size from the stack of every thread, also of the threads that never use it (a thread created
    with a small stack size then overflows it).  Only a pointer is in the TLS block here (QoreThreadLocalObject, which
    has no thread-local destructor, so that the object stays valid for Qore code that static destructors run at
    exit; qore#5778); the object is value-initialized (zeroed for a plain struct) on the first call in each thread
    and released when the Qore thread ends.

    Each call site uses a type of its own, so that two users of scratch buffers never share one in a thread.  Needs
    the declarations of qore/Qore.h in libqore (qore_thread_intern.h).

    @tparam T the type of the scratch object; must be default-constructible

    @return the calling thread's scratch object

    @throw std::bad_alloc if the object cannot be allocated
*/
template <typename T>
T& qore_thread_scratch() {
    return QoreThreadLocalObject<QoreThreadScratchHolder<T>, T>::get().obj;
}

#endif
