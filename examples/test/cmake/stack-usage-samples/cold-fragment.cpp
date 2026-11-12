// Copyright (C) 2026 Qore Technologies, s.r.o.
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
[[gnu::cold]] extern void oops(char*, int);
extern int flag;
#define BODY(test, k, N) if (test) return 1; \
    char buf[N]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    if (buf[3] == 7) { oops(buf, 1); oops(buf, 2); oops(buf,3); use(buf+11); } \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
int g(int x) { BODY(flag, 9, 1000) }
struct K { int m; int f(int x); };
int K::f(int x) { BODY(m, 9, 1000) }
int c(K* k, int n) {
  int r = 0;
  for (int j = 0; j < 2; ++j) r += g(n+j) + k->f(n+j);
  return r;
}
