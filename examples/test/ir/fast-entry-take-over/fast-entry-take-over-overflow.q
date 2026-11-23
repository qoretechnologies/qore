%modern
%allow-weak-references
# temporaries given to a callee whose parameter takes them over, in a recursion that ends with a stack overflow in
# the entry of the callee: the entry releases the references that it was given, so no object given to the recursion
# outlives it (issue #6105); ra() hands two temporaries over, one to a parameter that it does not use (an `any`
# parameter, to which the entry takes no reference) beside the one that it writes

#! the number of objects made before the recursion starts, more than its depth in every mode
const Pool = 200000;

#! the objects that the recursion gives to its calls, taken out of the list as it goes: a call's temporary is the
#! only reference to its object
our list<auto> pool = ();
#! weak references to the objects of the pool, which do not keep them alive
our list<auto> weaks = ();
#! the index in the pool of the last object given to the recursion
our int last = 0;

class T {
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

#! runs the recursion and prints its exception and the number of the last objects given to it that still exist once
#! it has unwound and the pool is released
/** the objects are made before the recursion, so that the stack overflows in the entry of the callee (or in the thunk
    of its frame) and not in the construction of an object
*/
sub run(string name, code c) {
    last = 0;
    for (int i = 0; i < Pool; ++i) {
        pool[i] = new T();
        weaks[i] := pool[i];
    }
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
    pool = ();
    printf("%s: %d alive\n", name, (select xrange(max(0, last - 16), min(Pool - 1, last + 2)),
        alive(weaks[$1])).size());
}

int sub r(list<auto> l, int d) {
    last = d + 1;
    r((0, remove pool[d + 1]), d + 1);
    l = ();
    return d;
}

int sub ra(any a, list<auto> l, int d) {
    last = 2 * d + 3;
    ra((0, remove pool[2 * d + 2]), (0, remove pool[2 * d + 3]), d + 1);
    l = ();
    return d;
}

run("r", sub () { r((0, remove pool[0]), 0); });
run("ra", sub () { ra((0, remove pool[0]), (0, remove pool[1]), 0); });
