// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
#define BODY(test, k) if (test) return 1; \
    char buf[300]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
struct K { int m; [[gnu::abi_tag("cxx11")]] int big(int x); };
int K::big(int x) { BODY(m, 9) }
int c3(K* k, int n) { return k->big(n) + 1; }
int c4(K* k, int n) { return k->big(n * 2) + 2; }
