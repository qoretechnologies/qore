/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadLocalStorage.h

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

#ifndef _QORE_QORETHREADLOCALSTORAGE_H

#define _QORE_QORETHREADLOCALSTORAGE_H

#include <cassert>
#include <pthread.h>

DLLEXPORT void qore_thread_local_storage_destroy(void* qtls);
DLLEXPORT void qore_thread_local_storage_set(void* qtls, void* p);
DLLEXPORT void* qore_thread_local_storage_get(void* qtls);
DLLEXPORT void qore_thread_local_storage_cleanup();

//! provides access to thread-local storage
/** This classes uses a single static pthread_key_t and a map to the actual
    data. It does not provide any special logic for checking for correct
    usage, etc
 */
template<typename T>
class QoreThreadLocalStorage {
public:
    //! creates the key
    DLLLOCAL QoreThreadLocalStorage() {
        // intentionally empty
    }

    //! destroys the key
    DLLLOCAL ~QoreThreadLocalStorage() {
        destroy();
    }

    //! creates the key
    DLLLOCAL void create() {
        // intentionally empty
    }

    //! destroys the key
    DLLLOCAL void destroy() {
        qore_thread_local_storage_destroy(this);
    }

    //! retrieves the key's value
    DLLLOCAL T* get() {
        return (T*)qore_thread_local_storage_get(this);
    }

    //! sets the key's value
    DLLLOCAL void set(T* ptr) {
        qore_thread_local_storage_set(this, const_cast<void*>((const void*)ptr));
    }

private:
    DLLLOCAL QoreThreadLocalStorage(const QoreThreadLocalStorage&) = delete;
    DLLLOCAL QoreThreadLocalStorage& operator=(const QoreThreadLocalStorage&) = delete;
};

#endif // _QORE_QORETHREADLOCALSTORAGE_H
