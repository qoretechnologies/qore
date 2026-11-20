%modern
# the temporaries of a statement that an exception ends are released before its handler runs, in the order of the
# AST interpreter, also by the IR of a closure that the IR interpreter runs from an image (qore#6742)
class D {
    constructor(string n) { self.n = n; }
    destructor() { printf("dtor %s\n", n); }
    public { string n; }
}
int sub f(D d, int x) { return 10 / x; }
code c = sub (int x) {
    try {
        printf("r %d\n", f(new D("a"), x) + f(new D("b"), 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("after\n");
};
c(0);
c(1);
