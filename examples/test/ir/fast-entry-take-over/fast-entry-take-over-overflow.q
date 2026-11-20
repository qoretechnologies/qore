%modern
# temporaries given to a callee whose parameter takes them over, in a recursion that ends with a stack overflow in
# the entry of the callee, which releases the references it was given (issue #6105); the objects that it frees are
# counted under valgrind: the destructors that run at the end of the stack cannot run their code, so the program does
# not count them; ra() has a parameter that it does not use beside the one that it writes

our int made = 0;
our int destroyed = 0;

class T {
    constructor() { ++made; }
    destructor() { ++destroyed; }
}

int sub r(list<auto> l, int d) { r((0, new T()), d + 1); l = (); return d; }
int sub ra(any a, list<auto> l, int d) { ra((0, new T()), (0, new T()), d + 1); l = (); return d; }

sub run(string name, code c) {
    made = 0;
    destroyed = 0;
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
    printf("%s: done\n", name);
}

run("r", sub () { r((0, new T()), 0); });
run("ra", sub () { ra((0, new T()), (0, new T()), 0); });
