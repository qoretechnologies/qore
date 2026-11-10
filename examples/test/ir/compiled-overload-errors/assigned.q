%modern
# a variant is committed at parse time for arguments that always have a value of their type (values, calls, new
# objects, definitely assigned local variables and parameters); an argument that can have no value at run time keeps
# the call resolved at run time; see compiled-overload-errors.qtest and QoreFunction::parseOtherVariantMayMatch()
string sub f(int a) { return "f(int)"; }
string sub f(*string a) { return sprintf("f(*string) %y", a); }

int sub g() { return 2; }

class B {
    public { string v; }
    constructor(int a) { v = "B(int)"; }
    constructor(*string a) { v = sprintf("B(*string) %y", a); }
}
class D inherits B {
    # a parameter in a base class constructor argument list
    constructor(int p) : B(p) {}
}
class M {
    public { int m; int mi = 1; }
}

# a parameter that is assigned, and one that is removed before the call
sub params(int p, int q) {
    printf("param: %s\n", f(p));
    remove q;
    printf("removed param: %s\n", f(q));
}

# a local removed later in a loop body has no value in the next iteration: the parse analyzes the body once, so the
# call is committed, and its variant is resolved again at run time in the second iteration
sub loop() {
    int x = 1;
    for (int i = 0; i < 2; ++i) {
        printf("loop %d: %s\n", i, f(x));
        remove x;
    }
}

sub clear(reference<int> r) {
    remove r;
}

# a local cleared through a reference, before the call and later in a loop body: the target of a reference can have no
# value at any read, in the compiled modes too
sub reference() {
    int x = 1;
    clear(\x);
    printf("reference-cleared: %s\n", f(x));
    try {
        int v = x;
        printf("reference-cleared store: %y\n", v);
    } catch (hash<ExceptionInfo> ex) {
        printf("reference-cleared store: %s\n", ex.err);
    }
    int y = 1;
    for (int i = 0; i < 2; ++i) {
        printf("reference loop %d: %s\n", i, f(y));
        clear(\y);
    }
}

# a local that a closure removes
sub closure() {
    int y = 1;
    code c = sub () { remove y; };
    c();
    printf("closure-removed: %s\n", f(y));
}

# a local assigned on one path only
sub branch(bool b) {
    int z;
    if (b) {
        z = 1;
    }
    printf("branch %y: %s\n", b, f(z));
}

printf("literal: %s\n", f(1));
printf("call: %s\n", f(g()));
printf("conditional: %s\n", f(True ? 1 : g()));
params(1, 2);
loop();
reference();
closure();
branch(True);
branch(False);
printf("base ctor: %s\n", (new D(1)).v);
M o();
printf("unassigned member: %s\n", f(o.m));
printf("assigned member: %s\n", f(o.mi));
remove o.mi;
printf("removed member: %s\n", f(o.mi));
