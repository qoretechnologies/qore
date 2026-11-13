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

#include <qore/Qore.h>

#include <cassert>

//! the holder of a per-thread scratch object of QoreThreadScratch, which value-initializes it
template <typename T>
struct QoreThreadScratchHolder {
    T obj{};
};

//! gives access to a per-thread scratch object of type T, allocated on the heap when a thread first asks for it
/** Large per-thread buffers must not be declared as \c static \c thread_local objects: the static TLS block of
    libqore is reserved in every thread of the process, and glibc takes it from the thread's own stack, so each such
    buffer takes its size from the stack of every thread, also of the threads that never use it (a thread created
    with a small stack size then overflows it).  Only a pointer is in the TLS block here (QoreThreadLocalObject, which
    has no thread-local destructor, so that the object stays valid for Qore code that static destructors run at
    exit; qore#5778); the object is value-initialized (zeroed for a plain struct) on the first use in each thread
    and released when the Qore thread ends.

    A thread without Qore thread data (a foreign thread that is not registered with the library) does not keep the
    object, as nothing would release it when the thread ends: each use there allocates an object of its own, which
    is freed when this object goes out of scope.

    An allocation failure does not throw: the object is then false, and the caller reports the error (the caller is
    often called by an I/O thread, where nothing catches \c std::bad_alloc).

    Each call site uses a type of its own, so that two users of scratch buffers never share one in a thread.

    @tparam T the type of the scratch object; must be default-constructible
*/
template <typename T>
class QoreThreadScratch {
public:
    //! gets the calling thread's scratch object, or one of its own if the thread has no thread data
    DLLLOCAL QoreThreadScratch() {
        bool owned;
        QoreThreadScratchHolder<T>* h = QoreThreadLocalObject<QoreThreadScratchHolder<T>, T>::getNoThrow(owned);
        if (h) {
            if (owned) {
                own = h;
            }
            obj = &h->obj;
        }
    }

    //! frees the scratch object if it is not kept by the thread
    DLLLOCAL ~QoreThreadScratch() {
        delete own;
    }

    QoreThreadScratch(const QoreThreadScratch&) = delete;
    QoreThreadScratch& operator=(const QoreThreadScratch&) = delete;

    //! returns true if the scratch object could be allocated
    DLLLOCAL explicit operator bool() const {
        return obj != nullptr;
    }

    //! returns the scratch object; the object must be valid
    DLLLOCAL T& operator*() const {
        assert(obj);
        return *obj;
    }

    //! returns the scratch object; the object must be valid
    DLLLOCAL T* operator->() const {
        assert(obj);
        return obj;
    }

private:
    //! the scratch object, or nullptr if it could not be allocated
    T* obj = nullptr;
    //! the holder of the scratch object if this object owns it (the thread has no thread data), otherwise nullptr
    QoreThreadScratchHolder<T>* own = nullptr;
};

#endif
