// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
extern int flag;
extern int g(int*);
#define BODY(test, k) if (test) return 1; \
    char buf[300]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
namespace N {
struct K {
    int m;
    int big(int x);
    int big(int x, int y);   // overload: constprop/isra only
};
int K::big(int x) { BODY(m, 9) }
int __attribute__((noinline)) K::big(int a, int b) { int x[400]; for (int i = 0; i < b; ++i) x[i & 63] = a + i + m; return g(x); }
}
namespace M { struct K { int m; int big(int a, int b); };
int __attribute__((noinline)) K::big(int a, int b) { int x[80]; for (int i = 0; i < b; ++i) x[i & 63] = a + i*2 + m; return g(x); } }
template <typename T> int tf(T x) { BODY(flag, sizeof(T) + 10) }
template <typename T> __attribute__((noinline)) int tf2(T a, int b) { int x[70]; for (int i = 0; i < b; ++i) x[i & 63] = a + i + sizeof(T); return g(x); }
int c3(N::K* k, int n) { return k->big(n) + 1; }
int c4(N::K* k, int n) { return k->big(n * 2) + 2; }
int v1(N::K* s, int a) { return s->big(a, 5); }
int v2(N::K* s, int a) { return s->big(a, 5) + 1; }
int v3(N::K* s, int a, int b) { return s->big(a, b); }
int w1(M::K* s, int a) { return s->big(a, 5); }
int w2(M::K* s, int a) { return s->big(a, 5) + 1; }
int w3(M::K* s, int a, int b) { return s->big(a, b); }
int c5(int n) { return tf<int>(n) + tf<int>(n * 3) + tf<char>(n) + tf<char>(n*2); }
int c6(int n) { return tf2<int>(n,5) + tf2<int>(n,5)+tf2<int>(n,n); }
