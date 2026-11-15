/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
/* Loads libqore with dlopen(), as an embedding runtime does (the Python module qoreloader, a JVM with the jni module):
 * the process does not link libqore, so on glibc the library's TLS must not need static TLS, which glibc only keeps a
 * small reserve of for the libraries that are loaded with dlopen().  Prints "loaded" if the library was loaded and
 * provides its version string (qore_version_string, a symbol with C linkage), else the error.
 */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <libqore>\n", argv[0]);
        return 2;
    }
    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        printf("dlopen failed: %s\n", dlerror());
        return 1;
    }
    if (!dlsym(h, "qore_version_string")) {
        printf("qore_version_string not found: %s\n", dlerror());
        return 1;
    }
    puts("loaded");
    return 0;
}
