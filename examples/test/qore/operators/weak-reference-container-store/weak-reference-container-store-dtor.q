%modern
%allow-weak-references
# an element or key store through a local that holds a weak reference holds its own reference to the container until
# the store is done; here that reference is the last strong one when the store ends (the destructor of an element that
# the store replaces removes the global variable that owned the container), so releasing it destroys the container,
# and the destructor of another element throws: the exception is that of the store, raised to the enclosing try block
# or to the caller's, in every mode (qore#7355: IR and JIT raised it after the store's exception handling, so it
# escaped the try block)

our *list gl;
our *hash gh;

class Thrower {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        throw "DTOR-" + n, "destructor of " + n;
    }
}

class Dropper {
    destructor() {
        remove ::gl;
        remove ::gh;
    }
}

sub list_case() {
    gl = (new Thrower("L0"), new Dropper());
    list l;
    l := gl;
    try {
        # the store is the last statement of the try block
        l[1] = 2;
    } catch (hash<ExceptionInfo> ex) {
        printf("list: caught %s\n", ex.err);
    }
    printf("list: after try\n");
}

sub hash_case() {
    gh = {"a": new Thrower("H0"), "b": new Dropper()};
    hash h;
    h := gh;
    try {
        h.b = 2;
    } catch (hash<ExceptionInfo> ex) {
        printf("hash: caught %s\n", ex.err);
    }
    printf("hash: after try\n");
}

sub hash_dynamic_key_case() {
    gh = {"a": new Thrower("D0"), "b": new Dropper()};
    hash h;
    h := gh;
    string k = "b";
    try {
        h{k} = 2;
    } catch (hash<ExceptionInfo> ex) {
        printf("hash dynamic key: caught %s\n", ex.err);
    }
    printf("hash dynamic key: after try\n");
}

# no try block in the function: the exception reaches the caller's
sub no_local_catch() {
    gl = (new Thrower("N0"), new Dropper());
    list l;
    l := gl;
    l[1] = 2;
    printf("no local catch: continued\n");
}

list_case();
hash_case();
hash_dynamic_key_case();
try {
    no_local_catch();
} catch (hash<ExceptionInfo> ex) {
    printf("no local catch: caller caught %s\n", ex.err);
}
printf("done\n");
