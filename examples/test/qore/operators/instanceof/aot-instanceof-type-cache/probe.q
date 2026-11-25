%modern
# compiled with qcc: the instanceof checks of the loops below call the AOT runtime with a type path (qore#7416)
class A {}
class B {}
class C {}
class D {}
class E {}
class F {}

# every type path shares one set of the cache, whatever the layout of the compiled code
bool prev = dbg_aot_instanceof_one_set(True);
on_exit dbg_aot_instanceof_one_set(prev);

auto a = new A();
auto b = new B();
int n = 1000;

# two hot paths that share a set: each is resolved once, not on every evaluation
int hits = 0;
int before = dbg_aot_instanceof_resolves();
for (int i = 0; i < n; ++i) {
    if (a instanceof A) {
        ++hits;
    }
    if (b instanceof B) {
        ++hits;
    }
}
printf("two paths: hits=%d resolves=%d\n", hits, dbg_aot_instanceof_resolves() - before);

# more paths than a set has ways, one call site each, in a cycle: with 4 ways kept in the order of their last use,
# each evaluation misses and replaces the least recently used way, so every evaluation resolves its type, except the
# first evaluations of A and B, which the loop above left in the set
before = dbg_aot_instanceof_resolves();
list<auto> objs = (new A(), new B(), new C(), new D(), new E(), new F());
int six = 0;
for (int i = 0; i < n; ++i) {
    if (objs[0] instanceof A) {
        ++six;
    }
    if (objs[1] instanceof B) {
        ++six;
    }
    if (objs[2] instanceof C) {
        ++six;
    }
    if (objs[3] instanceof D) {
        ++six;
    }
    if (objs[4] instanceof E) {
        ++six;
    }
    if (objs[5] instanceof F) {
        ++six;
    }
}
printf("six paths: hits=%d resolves=%d\n", six, dbg_aot_instanceof_resolves() - before);

# every object against every class while ways are replaced: no result is stale or wrong
bool sub is(auto o, int k) {
    switch (k) {
        case 0: return o instanceof A;
        case 1: return o instanceof B;
        case 2: return o instanceof C;
        case 3: return o instanceof D;
        case 4: return o instanceof E;
        case 5: return o instanceof F;
    }
    return False;
}
int mismatches = 0;
for (int i = 0; i < n; ++i) {
    for (int j = 0; j < 6; ++j) {
        for (int k = 0; k < 6; ++k) {
            if (is(objs[j], k) != (j == k)) {
                ++mismatches;
            }
        }
    }
}
printf("matrix: checks=%d mismatches=%d\n", n * 36, mismatches);
