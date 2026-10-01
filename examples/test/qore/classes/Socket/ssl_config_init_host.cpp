/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */

/*  Driver for ssl-config-init.qtest: initializes the Qore library in a process that may already
    have initialized OpenSSL, and reports whether the OpenSSL configuration file had been processed
    by the time qore_init() returned.

    OpenSSL is reached through dlsym() so that the driver needs neither the OpenSSL headers nor the
    OpenSSL libraries on its link line; libqore brings them into the process.
*/

#include <qore/Qore.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <dlfcn.h>

typedef int (*openssl_init_ssl_t)(uint64_t, const void*);
typedef int (*conf_modules_load_file_t)(const char*, const char*, unsigned long);
typedef unsigned long (*openssl_version_num_t)();

// CONF_MFLAGS_IGNORE_MISSING_FILE | CONF_MFLAGS_DEFAULT_SECTION from openssl/conf.h
static const unsigned long conf_flags = 0x10 | 0x20;

int main(int argc, char** argv) {
    if (argc != 2 || (strcmp(argv[1], "host-first") && strcmp(argv[1], "qore-first"))) {
        fputs("usage: ssl-config-init-host host-first|qore-first\n", stderr);
        return 2;
    }

    openssl_init_ssl_t init_ssl = reinterpret_cast<openssl_init_ssl_t>(dlsym(RTLD_DEFAULT, "OPENSSL_init_ssl"));
    conf_modules_load_file_t load_file = reinterpret_cast<conf_modules_load_file_t>(dlsym(RTLD_DEFAULT,
        "CONF_modules_load_file"));
    openssl_version_num_t version_num = reinterpret_cast<openssl_version_num_t>(dlsym(RTLD_DEFAULT,
        "OpenSSL_version_num"));
    if (!init_ssl || !load_file || !version_num) {
        puts("openssl: unavailable");
        return 0;
    }
    // MNNFFPPS: the major version is the top nibble from OpenSSL 3 on
    printf("openssl: %lu\n", version_num() >> 28);

    if (!strcmp(argv[1], "host-first")) {
        // what an embedding application that uses TLS itself has done before it loads Qore; this
        // processes the OpenSSL configuration file
        printf("host init: %s\n", init_ssl(0, nullptr) ? "ok" : "failed");
    }

    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);

    // The configuration file the test supplies registers an OID, which can be done only once, so
    // processing the file again fails exactly when it has already been processed.
    printf("reload: %s\n", load_file(nullptr, nullptr, conf_flags) > 0 ? "ok" : "failed");

    qore_cleanup();
    return 0;
}
