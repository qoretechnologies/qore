%modern
%allow-weak-references

# a program of weak-assignment-own-path.qtest: a weak assignment does not keep the list or hash that an lvalue holds
# alive while it resolves its target (qore#7405): another thread that releases the source in that window leaves the
# weak reference designating a freed container, which reads as empty and cannot be changed; every mode must print the
# output of the AST interpreter
#
# The window is forced without timing: the source is read first, then the key of the target is evaluated, which has
# another thread release the source and waits until it has

our hash<auto!> g;
our list<auto!> gl;

#! has another thread release the global hash, and returns the key
string sub release_hash() {
    Counter done(1);
    background sub () {
        remove g;
        done.dec();
    }();
    done.waitForZero();
    return "k";
}

#! has another thread release the global list, and returns the index
int sub release_list() {
    Counter done(1);
    background sub () {
        remove gl;
        done.dec();
    }();
    done.waitForZero();
    return 0;
}

#! runs a change, printing its exception
sub run(string label, code c) {
    try {
        c();
        printf("%s: changed\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

sub hash_source() {
    g = {"a": 1};
    hash<auto!> t = {};
    t{release_hash()} := g;
    printf("hash released: %y exists: %y\n", t.k, exists t.k);
    run("hash released change", sub () { t.k.b = 2; });
    printf("hash released after: %y\n", t.k);
}

sub list_source() {
    gl = (1, 2);
    list<auto!> t = (NOTHING,);
    t[release_list()] := gl;
    printf("list released: %y\n", t[0]);
    run("list released change", sub () { t[0][2] = 3; });
    printf("list released after: %y\n", t[0]);
}

#! the source is not released: the weak reference designates it
sub kept_source() {
    g = {"a": 1};
    hash<auto!> t = {};
    t.k := g;
    run("kept change", sub () { t.k.b = 2; });
    printf("kept: %y %y\n", t.k, g);
}

hash_source();
list_source();
kept_source();
