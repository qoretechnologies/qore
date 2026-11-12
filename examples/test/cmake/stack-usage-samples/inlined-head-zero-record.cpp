// Copyright (C) 2026 Qore Technologies, s.r.o.
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
extern void usep(void*);
extern int flag;
#define BODY(test, k, N) if (test) return 1; \
    char buf[N]; memset(buf, x, sizeof buf); usep(this); use(buf); use(buf+1); use(buf+2); use(buf+k); \
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
struct K {
    int m;
    int f(int x);
    int f(long x);
};
static inline int helper(K* k, int x);
int K::f(long x) { usep(this); int y = m; if (x > y) { char b[50]; memset(b, x, 50); use(b); } return y; }
namespace { struct L { int m; int f(int x); int f(long x); }; 
int L::f(int x) { BODY(m, 1, 1000) }
__attribute__((noinline)) int L::f(long x) { usep(this); int y = m; if (x > y) { char b[50]; memset(b, x, 50); use(b); } return y; }
}
void* mk();
int call(int n) {
  L* k = (L*)mk();
  int r = 0;
  for (int j = 0; j < 2; ++j) r += k->f(n+j) + k->f((long)n+j);
  return r;
}
int call2(int n) { L* k = (L*)mk(); return k->f(n*3) + k->f(n*5); }
