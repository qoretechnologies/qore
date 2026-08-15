/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadLocalStorage.cpp

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

#include "qore/Qore.h"
#include "qore/QoreThreadLocalStorage.h"

#include <map>

typedef std::map<void*, void*> storage_map_t;

static pthread_key_t qore_storage_key;

//! set only while \c qore_storage_key holds a key created by pthread_key_create()
/** The key must not be used before it is created; \c pthread_getspecific() with an uncreated key
    returns an unspecified value, not nullptr.  On glibc an all-zero key happens to read an empty
    TSD slot and yields nullptr, but on musl key 0 is a valid key index whose slot can hold another
    consumer's pointer, which is then used as a storage_map_t* and dereferenced.

    A zero key value cannot serve as the "not created" marker: pthread_key_create() may legitimately
    return key 0.  This flag is therefore the only reliable test.
*/
static bool qore_storage_key_valid = false;

struct qore_tls_cache_t {
    void* key = nullptr;
    void* value = nullptr;
};

static thread_local qore_tls_cache_t qore_tls_cache;

static void qore_thread_local_storage_clear_cache() {
    qore_tls_cache = {};
}

static void qore_thread_local_storage_destructor(void* p) {
    qore_thread_local_storage_clear_cache();
    if (p) {
        storage_map_t* sm = (storage_map_t*)p;
        delete sm;
    }
}

void qore_thread_local_storage_init() {
    if (qore_storage_key_valid) {
        return;
    }
#ifdef DEBUG
    assert(!pthread_key_create(&qore_storage_key, qore_thread_local_storage_destructor));
#else
    pthread_key_create(&qore_storage_key, qore_thread_local_storage_destructor);
#endif
    qore_storage_key_valid = true;
}

void qore_thread_local_storage_destroy() {
    if (!qore_storage_key_valid) {
        return;
    }
    qore_thread_local_storage_clear_cache();
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (sm) {
        delete sm;
    }
    pthread_key_delete(qore_storage_key);
    // the key is gone; any later access must not read it
    qore_storage_key_valid = false;
}

void qore_thread_local_storage_destroy(void* qtls) {
    if (!qore_storage_key_valid) {
        return;
    }
    if (qore_tls_cache.key == qtls) {
        qore_thread_local_storage_clear_cache();
    }
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (sm) {
        storage_map_t::iterator i = sm->find(qtls);
        if (i != sm->end()) {
            sm->erase(i);
        }
    }
}

void qore_thread_local_storage_cleanup() {
    if (!qore_storage_key_valid) {
        return;
    }
    qore_thread_local_storage_clear_cache();
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (sm) {
        delete sm;
        pthread_setspecific(qore_storage_key, nullptr);
    }
}

void qore_thread_local_storage_set(void* qtls, void* p) {
    // a value can be stored before qore_thread_local_storage_init() is reached; create the key
    // rather than dropping the value silently
    if (!qore_storage_key_valid) {
        qore_thread_local_storage_init();
    }
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
    qore_tls_cache.key = qtls;
    qore_tls_cache.value = p;
}

void* qore_thread_local_storage_get(void* qtls) {
    if (qore_tls_cache.key == qtls) {
        return qore_tls_cache.value;
    }
    // nothing can have been stored yet if the key does not exist; reading it would return an
    // unspecified value.  This runs before qore_thread_local_storage_init(): q_gettid() is reached
    // from lock acquisitions made while the command line is parsed.
    if (!qore_storage_key_valid) {
        qore_tls_cache.key = qtls;
        qore_tls_cache.value = nullptr;
        return nullptr;
    }
    storage_map_t* sm = (storage_map_t*)pthread_getspecific(qore_storage_key);
    if (!sm) {
        qore_tls_cache.key = qtls;
        qore_tls_cache.value = nullptr;
        return nullptr;
    }
    storage_map_t::iterator i = sm->find(qtls);
    void* rv = i != sm->end() ? i->second : nullptr;
    qore_tls_cache.key = qtls;
    qore_tls_cache.value = rv;
    return rv;
}
