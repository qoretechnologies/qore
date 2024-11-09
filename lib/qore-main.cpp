/*
    qore-main.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>

#include <qore/DBI.h>
#include <qore/QoreHttpClientObject.h>

#include "qore/intern/QoreSignal.h"
#include "qore/intern/ModuleInfo.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#ifdef HAVE_OPENSSL_ENGINE_H
#include <openssl/engine.h>
#endif

#ifdef OPENSSL_3_PLUS
#include <openssl/provider.h>
#endif

// initialized flag
std::atomic<bool> qore_initialized = {false};

// shutdown flag
std::atomic<bool> qore_shutdown = {false};

// exiting flag
std::atomic<bool> qore_exiting = {false};

#ifdef DARWIN
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

int qore_trace = 0;
int debug = 0;
int qore_library_options = QLO_NONE;

// issue #3818: qore SSL app-specific data index
int qore_ssl_data_index = -1;

qore_license_t qore_license;

const QoreStringMaker mpfrInfo("runtime: %s built with: %s (%d.%d.%d)", mpfr_get_version(), MPFR_VERSION_STRING,
    MPFR_VERSION_MAJOR, MPFR_VERSION_MINOR, MPFR_VERSION_PATCHLEVEL);

#ifndef HAVE_OPENSSL_INIT_CRYPTO
// issue #2135: openssl locking functions were deprecated in openssl 1.0.0 and are no longer used in 1.1.0+
// static locks for openssl
typedef std::vector<QoreThreadLock*> mutex_vec_t;
static mutex_vec_t q_openssl_mutex_list;

static unsigned long q_openssl_id_function(void) {
#ifdef _Q_WINDOWS
    return GetCurrentThreadId();
#else
    return (unsigned long)pthread_self();
#endif
}

static void q_openssl_locking_function(int mode, int n, const char* file, int line) {
    if (mode & CRYPTO_LOCK)
        q_openssl_mutex_list[n]->lock();
    else
        q_openssl_mutex_list[n]->unlock();
}
#endif

#ifdef OPENSSL_3_PLUS
OSSL_PROVIDER* ssl_prov_legacy = nullptr,
    * ssl_prov_default = nullptr;
#endif

void qore_init(qore_license_t license, const char* def_charset, bool show_module_errors, int n_qore_library_options) {
    qore_license = license;
    qore_library_options = n_qore_library_options;

    // initialize openssl library
    if (!qore_check_option(QLO_DISABLE_OPENSSL_INIT)) {
#ifdef HAVE_OPENSSL_INIT_CRYPTO
        OPENSSL_init_crypto(0, 0);
#else
        OPENSSL_config(0);
#endif
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        SSL_library_init();
        ERR_load_crypto_strings();

#ifndef HAVE_OPENSSL_INIT_CRYPTO
        // create locks
        for (int i = 0; i < CRYPTO_num_locks(); ++i)
            q_openssl_mutex_list.push_back(new QoreThreadLock());

        // issue #2135: openssl locking functions were deprecated in openssl 1.0.0 and are no longer used in 1.1.0+
        CRYPTO_set_id_callback(q_openssl_id_function);
        CRYPTO_set_locking_callback(q_openssl_locking_function);
#endif
    }

#ifdef OPENSSL_3_PLUS
    // load default provider
    ssl_prov_default = OSSL_PROVIDER_load(nullptr, "default");
    assert(ssl_prov_default);
    // load legacy provider
    ssl_prov_legacy = OSSL_PROVIDER_load(nullptr, "legacy");
    assert(ssl_prov_legacy);
#endif

    qore_ssl_data_index = SSL_get_ex_new_index(0, (void*)"qore data index", NULL, NULL, NULL);

    if (qore_library_options & QLO_DISABLE_GARBAGE_COLLECTION) {
        q_disable_gc = true;
    }

    qore_string_init();
    QoreHttpClientObject::static_init();

    // initialize random number generator
    if (!(qore_library_options & QLO_DO_NOT_SEED_RNG)) {
        unsigned seed = (unsigned)q_clock_getmicros();
#ifdef HAVE_RANDOM
        srandom(seed);
#else
        srand(seed);
#endif
    }

    // init random salt
    qore_init_random_salt();

    // init threading infrastructure
    init_qore_threads();

    // initialize charset encoding support
    QEM.init(def_charset);

    // init character maps
    init_charmaps();

    init_lib_intern(environ);

    // create default type values
    init_qore_types();

    // init module subsystem
    QMM.init(show_module_errors);

#ifdef HAVE_SIGNAL_HANDLING
    // init signals
    QSM.init(qore_library_options & QLO_DISABLE_SIGNAL_HANDLING);
#endif

    // initialize static system namespaces
    staticSystemNamespace = new StaticSystemNamespace;

    // set up pseudo-methods
    pseudo_classes_init();

#ifdef _Q_WINDOWS
    // do windows socket initialization
    WORD wsver = MAKEWORD(2, 2);
    WSADATA wsd;
    int err = WSAStartup(wsver, &wsd);
    if (err) {
        printf("qore_init(): WSAStartup() failed with error: %d; sockets will not be available\n", err);
    }
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif

    // set initialized flag for external modules
    qore_initialized.store(true, std::memory_order_relaxed);
}

// NOTE: we do not cleanup in reverse initialization order
// the threading subsystem is deleted before the modules are
// unloaded in case there are any module-specific thread
// cleanup functions to be run...
void qore_cleanup() {
    // set shutdown flag for external modules
    qore_shutdown.store(true, std::memory_order_relaxed);

    // purge thread resources before deleting modules
    {
        ExceptionSink xsink;
        purge_thread_resources(&xsink);
    }

    // first delete all user modules
    QMM.delUser();

#ifdef _Q_WINDOWS
    // do windows socket cleanup
    WSACleanup();
#endif

#ifdef HAVE_SIGNAL_HANDLING
    // stop signal manager
    QSM.del();
#endif

    // delete all loadable modules
    QMM.cleanup();

    // issue #3045: clear module options
    qore_delete_module_options();

    // delete thread-local data
    delete_thread_local_data();

    // now free memory (like ARGV, QORE_ARGV, ENV, etc)
    delete_global_variables();

    // delete pseudo-methods
    pseudo_classes_del();

    // delete static system namespace after modules
    delete staticSystemNamespace;
#ifdef DEBUG
    staticSystemNamespace = nullptr;
#endif

    // delete default type values
    delete_qore_types();

    // delete threading infrastructure
    delete_qore_threads();

    // only perform openssl cleanup if not performed externally
    if (!qore_check_option(QLO_DISABLE_OPENSSL_CLEANUP)) {
        // cleanup openssl library
        ERR_free_strings();

#ifdef HAVE_OPENSSL_ENGINE_H
        ENGINE_cleanup();
#endif
        EVP_cleanup();

        CONF_modules_finish();
        CONF_modules_free();
        CONF_modules_unload(1);

        CRYPTO_cleanup_all_ex_data();

#ifndef HAVE_OPENSSL_INIT_CRYPTO
        CRYPTO_set_id_callback(0);
        CRYPTO_set_locking_callback(0);

        // delete openssl locks
        for (mutex_vec_t::iterator i = q_openssl_mutex_list.begin(), e = q_openssl_mutex_list.end(); i != e; ++i)
            delete *i;
#endif

#ifdef OPENSSL_3_PLUS
        // unload legacy provider
        if (!OSSL_PROVIDER_unload(ssl_prov_legacy)) {
            assert(false);
        }
        // unload default provider
        if (!OSSL_PROVIDER_unload(ssl_prov_default)) {
            assert(false);
        }
#endif
    }

    printd(5, "qore_cleanup() exiting cleanly\n");
}

extern "C" {
DLLEXPORT int JNI_OnLoad(void* vm, void* reserved) {
    printd(5, "JNI_OnLoad() vm: %p\n", vm);

    // initialize the qore library
    qore_init(QL_MIT);

    // here we pass the VM pointer to the jni module
    qore_set_module_option("jni", "jvm-ptr", reinterpret_cast<int64>(vm));
    ExceptionSink xsink;
    QoreProgramHelper qpgm(0, xsink);
    if (MM.runTimeLoadModule("jni", *qpgm, &xsink)) {
        return 0;
    }

    // return the JNI version
    ValueHolder rc(qore_get_module_option("jni", "jni-version"), &xsink);
    return static_cast<int>(rc->getAsBigInt());
}
}
