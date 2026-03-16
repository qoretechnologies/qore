/*
    qore-main.cpp

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

#include <qore/Qore.h>

#include <qore/DBI.h>
#include <qore/QoreHttpClientObject.h>

#include "qore/intern/QoreSignal.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/EncryptionTransforms.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QuicSessionTicketCache.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <set>
#include <string>
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

#if defined(DARWIN)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <link.h>
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
std::atomic<int> qore_global_http2_mode{1};  // HTTP2_MODE_AUTO by default

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

#if defined(DARWIN) || defined(__linux__)
static bool qore_is_libqore_basename(const char* path) {
    if (!path || !*path) {
        return false;
    }
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return !strncmp(base, "libqore.", 8) || !strncmp(base, "libqore.so", 10);
}

static std::string qore_canonicalize_path(const char* path) {
    if (!path || !*path) {
        return std::string();
    }
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        return std::string(resolved);
    }
    return std::string(path);
}

static bool qore_check_duplicate_libqore() {
    std::set<std::string> paths;

    Dl_info info;
    if (dladdr((void*)&qore_check_duplicate_libqore, &info) && info.dli_fname) {
        std::string self_path = qore_canonicalize_path(info.dli_fname);
        if (!self_path.empty()) {
            paths.insert(self_path);
        }
    }

#if defined(DARWIN)
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = _dyld_get_image_name(i);
        if (!qore_is_libqore_basename(name)) {
            continue;
        }
        std::string p = qore_canonicalize_path(name);
        if (!p.empty()) {
            paths.insert(p);
        }
    }
#elif defined(__linux__)
    auto callback = [](struct dl_phdr_info* info, size_t, void* data) -> int {
        auto* setp = static_cast<std::set<std::string>*>(data);
        if (!info || !info->dlpi_name || !info->dlpi_name[0]) {
            return 0;
        }
        if (!qore_is_libqore_basename(info->dlpi_name)) {
            return 0;
        }
        std::string p = qore_canonicalize_path(info->dlpi_name);
        if (!p.empty()) {
            setp->insert(p);
        }
        return 0;
    };
    dl_iterate_phdr(callback, &paths);
#endif

    if (paths.size() <= 1) {
        return false;
    }

    fprintf(stderr, "qore_init(): multiple libqore images loaded; exiting to avoid crashes:\n");
    for (const auto& p : paths) {
        fprintf(stderr, "  %s\n", p.c_str());
    }
    fflush(stderr);
    return true;
}
#endif

void qore_init(qore_license_t license, const char* def_charset, bool show_module_errors, int n_qore_library_options) {
    qore_license = license;
    qore_library_options = n_qore_library_options;

#if defined(DARWIN) || defined(__linux__)
    static bool duplicate_checked = false;
    if (!duplicate_checked) {
        duplicate_checked = true;
        if (qore_check_duplicate_libqore()) {
            _exit(1);
        }
    }
#endif

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

    // initialize thread-local storage
    qore_thread_local_storage_init();

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

    // clean up global async I/O logger before modules and threading are torn down
    qore_async_io_logger_cleanup();

    // purge thread resources before deleting modules
    {
        ExceptionSink xsink;
        purge_thread_resources(&xsink);
    }

    // first delete all user modules (runs module del handlers, stops ThreadPools)
    QMM.delUser();

    // wait for ThreadPool threads (QTF_EXTERNAL_LIFECYCLE) to fully exit after
    // being stopped during module cleanup above
    {
        ExceptionSink xsink;
        tp_thread_counter.waitForZero(&xsink);
    }

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

#ifdef HAVE_MPFR_BUILDOPT_TLS_T
    // ensure MPFR caches are released for the main thread
    mpfr_free_cache2(static_cast<mpfr_free_cache_t>(MPFR_FREE_LOCAL_CACHE | MPFR_FREE_GLOBAL_CACHE));
    mpfr_free_pool();
    mpfr_mp_memory_cleanup();
#endif

    // issue #3045: clear module options
    qore_delete_module_options();

    // NOTE: cleanupAllPrograms removed - programs should be cleaned up by module deletion
    // Forcing cleanup here causes crashes due to interdependencies between programs

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

    // destroy thread-local storage
    qore_thread_local_storage_destroy();

    // Clear QUIC session ticket cache before OpenSSL cleanup to avoid
    // calling SSL_SESSION_free() on already-freed OpenSSL state
    QuicSessionTicketCache::instance().clear();

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

#ifdef OPENSSL_3_PLUS
        // refs #4910: free cached cipher and digest references
        crypto_cache_cleanup();
#endif

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
        // output error information to help diagnose module loading failures
        if (xsink.isException()) {
            const QoreValue err = xsink.getExceptionErr();
            const QoreValue desc = xsink.getExceptionDesc();
            const char* err_str = err.getType() == NT_STRING
                ? err.get<const QoreStringNode>()->c_str() : "unknown error";
            const char* desc_str = desc.getType() == NT_STRING
                ? desc.get<const QoreStringNode>()->c_str() : "no description";
            fprintf(stderr, "JNI_OnLoad: failed to load jni module: %s: %s\n", err_str, desc_str);
            fflush(stderr);
        } else {
            fprintf(stderr, "JNI_OnLoad: failed to load jni module (no exception info)\n");
            fflush(stderr);
        }
        return 0;
    }

    // return the JNI version
    ValueHolder rc(qore_get_module_option("jni", "jni-version"), &xsink);
    return static_cast<int>(rc->getAsBigInt());
}
}
