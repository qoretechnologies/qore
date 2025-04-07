/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadLocalStorage.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#include "qore/Qore.h"
#include "qore/QoreThreadLocalStorage.h"

#include <map>

typedef std::map<void*, void*> storage_map_t;

static pthread_key_t qore_storage_key;

static void qore_thread_local_storage_destructor(void* p) {
    if (p) {
        storage_map_t* sm = (storage_map_t*)p;
        delete sm;
    }
}

void qore_thread_local_storage_init() {
#ifdef DEBUG
    assert(!pthread_key_create(&qore_storage_key, qore_thread_local_storage_destructor));
#else
    pthread_key_create(&qore_storage_key, qore_thread_local_storage_destructor);
#endif
}

void qore_thread_local_storage_destroy() {
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (sm) {
        delete sm;
    }
    pthread_key_delete(qore_storage_key);
}

void qore_thread_local_storage_destroy(void* qtls) {
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (sm) {
        storage_map_t::iterator i = sm->find(qtls);
        if (i != sm->end()) {
            sm->erase(i);
        }
    }
}

void qore_thread_local_storage_set(void* qtls, void* p) {
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (!sm) {
        sm = new storage_map_t;
        pthread_setspecific(qore_storage_key, (void*)sm);
    }
    storage_map_t::iterator i = sm->lower_bound(qtls);
    if (i == sm->end() || i->first != qtls) {
        sm->insert(i, storage_map_t::value_type(qtls, p));
    } else {
        i->second = p;
    }
}

void* qore_thread_local_storage_get(void* qtls) {
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (!sm) {
        return nullptr;
    }
    storage_map_t::iterator i = sm->find(qtls);
    return i != sm->end() ? i->second : nullptr;
}
