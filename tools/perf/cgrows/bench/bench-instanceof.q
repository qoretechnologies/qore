%modern
# instanceof loops (qore#7416): L=io (two instanceof checks of objects of two classes per iteration), iom (six
# instanceof checks of objects of six classes per iteration); N iterations.  In the aot modes each check resolves its
# type through the type cache of the AOT runtime, so the rows measure a hit of the cache
class A {}
class B {}
class C {}
class D {}
class E {}
class F {}
int sub f(int a) { return a + 1; }
int sub run_io(int n) {
    auto a = new A();
    auto b = new B();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        if (a instanceof A) {
            ++t;
        }
        if (b instanceof B) {
            ++t;
        }
    }
    return t;
}
int sub run_iom(int n) {
    list<auto> objs = (new A(), new B(), new C(), new D(), new E(), new F());
    int t = 0;
    for (int i = 0; i < n; ++i) {
        if (objs[0] instanceof A) {
            ++t;
        }
        if (objs[1] instanceof B) {
            ++t;
        }
        if (objs[2] instanceof C) {
            ++t;
        }
        if (objs[3] instanceof D) {
            ++t;
        }
        if (objs[4] instanceof E) {
            ++t;
        }
        if (objs[5] instanceof F) {
            ++t;
        }
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "io": run_io(n); break;
    case "iom": run_iom(n); break;
}
