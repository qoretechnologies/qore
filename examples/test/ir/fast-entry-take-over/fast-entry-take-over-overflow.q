%modern
%allow-weak-references
# temporaries given to a callee whose parameter takes them over, in a recursion that ends with a stack overflow in
# the entry of the callee: the entry releases the references that it was given, so no object made outlives the
# recursion (issue #6105); ra() has a parameter that it does not use beside the one that it writes

const Ring = 16;

# weak references to the last objects made, which are those given to the calls that end the recursion
our list<auto> weaks = ();
our int made = 0;

class T {
}

#! makes an object and keeps a weak reference to it, which does not keep it alive
T sub mk() {
    T t();
    weaks[made++ % Ring] := t;
    return t;
}

#! accepts only an object that exists
bool sub is_object(object o) {
    return True;
}

#! returns True if the weak reference still gives an object that exists
bool sub alive(auto w) {
    try {
        return is_object(w);
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "RUNTIME-TYPE-ERROR" || ex.err == "RUNTIME-OVERLOAD-ERROR") {
            return False;
        }
        rethrow;
    }
}

#! runs the recursion and prints its exception and the number of the last objects made that still exist
sub run(string name, code c) {
    weaks = ();
    made = 0;
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
    printf("%s: %d alive\n", name, (select weaks, alive($1)).size());
}

int sub r(list<auto> l, int d) { r((0, mk()), d + 1); l = (); return d; }
int sub ra(any a, list<auto> l, int d) { ra((0, mk()), (0, mk()), d + 1); l = (); return d; }

run("r", sub () { r((0, mk()), 0); });
run("ra", sub () { ra((0, mk()), (0, mk()), 0); });
