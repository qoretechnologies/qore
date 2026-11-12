// Copyright (C) 2026 Qore Technologies, s.r.o.
// a sample of GCC's records of .part clones for examples/test/cmake/test_check_stack_usage.py (issue #5852)
#include <cstring>
extern void use(char*);
struct ExceptionSink;
struct K { int m; int b(const char* p, ExceptionSink* xsink, int x); };
int K::b(const char* p, ExceptionSink* xsink, int x) { if (m) return 1;
    char buf[300]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2);
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1);} use(buf+4); return buf[*p]; }
int c(K* k, int n) { return k->b("a", 0, n) + k->b("b", 0, n*2); }
