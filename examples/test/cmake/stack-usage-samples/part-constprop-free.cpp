// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
extern int flag;
#define BODY(test, k, N) if (test) return 1; \
    char buf[N]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
int g(int x, int y) { if (flag) return 1; char buf[1000]; memset(buf, x, sizeof buf); use(buf+y); for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i+y, sizeof b2); use(b2); use(b2+y);} use(buf+4); return buf[y]; }
struct K { int m; int f(int x, int y); };
int K::f(int x, int y) { if (m) return 1; char buf[900]; memset(buf, x, sizeof buf); use(buf+y); for (int i = 0; i < x; ++i) { char b2[300]; memset(b2, i+y, sizeof b2); use(b2); use(b2+y);} use(buf+4); return buf[y]; }
int c(K* k, int n, int q) {
  int r = 0;
  for (int j = 0; j < 2; ++j) r += g(n+j, 5) + g(n, 5) + g(n+j, q) + k->f(n, 7) + k->f(n+j, 7) + k->f(n, q);
  return r;
}
