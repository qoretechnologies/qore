// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
extern int gm;
#define BODY(test, k, N) if (test) return 1; \
    char buf[N]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
#define SBODY(N) char buf[N]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); \
    for (int i = 0; i < x; ++i) { char b2[20]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[9];
template<class T> struct S { static int h(int x); };
template<class T> int S<T>::h(int x) { int m = gm; BODY(m, 9, 1000) }
int c3(int n) { int r = 0; for (int j = 0; j < 2; ++j) r += S<int>::h(n+j); return r; }
template struct S<int>;
static int z(int x) { int m = gm; if (m) return 1; SBODY(16) }
int call(int n) { return z(n) + z(n*3); }
int g(int x) { int m = gm; BODY(m, 9, 1000) }
int c2(int n) { int r = 0; for (int j = 0; j < 2; ++j) r += g(n+j); return r; }
