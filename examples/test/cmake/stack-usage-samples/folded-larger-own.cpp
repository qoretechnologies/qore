// Copyright (C) 2026 Qore Technologies, s.r.o.
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
#define BODY(test, k, N) if (test) return 1; \
    char buf[N]; memset(buf, x, sizeof buf); use(buf); use(buf+a1); use(buf+a2); use(buf+a3); use(buf+a4); use(buf+a5); use(buf+a6); use(buf+a7); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
struct K {
    int m;
    int f(int x, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
    int f(long x, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
    int z(long x, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
};
int K::z(long x, long a1, long a2, long a3, long a4, long a5, long a6, long a7) { BODY(m, 9, 1000) }
int K::f(long x, long a1, long a2, long a3, long a4, long a5, long a6, long a7) { BODY(m, 9, 1000) }
int K::f(int x, long a1, long a2, long a3, long a4, long a5, long a6, long a7) { BODY(m, 9, 1016) }
int c(K* k, int n) {
  int r = 0;
  for (int j = 0; j < 2; ++j) r += k->f(n+j,1,2,3,4,5,6,7) + k->f((long)n+j,1,2,3,4,5,6,7) + k->z((long)n+j,1,2,3,4,5,6,7);
  return r;
}
