%modern
%allow-weak-references

# the program of weak-reference-bind.qtest: a variable that holds a weak reference is bound to a reference parameter
# of the type of its target (qore#7433): the weak reference has the type of its target, the variable keeps it, and a
# change through the reference changes the target; every mode must print the output of the AST interpreter

class C {
    public {
        int x = 1;
    }
}

sub set_object(reference<object> r) {
    r.x = 2;
}

sub set_class(reference<C> r) {
    r.x = 3;
}

sub set_hash(reference<hash> r) {
    r.b = 2;
}

sub set_list(reference<list> r) {
    r[1] = 2;
}

sub set_auto_hash(reference<hash<auto>> r) {
    r.c = 3;
}

hashdecl D {
    int a = 1;
}

sub set_decl(reference<hash<D>> r) {
    r.a = 5;
}

sub set_typed_hash(reference<hash<string, int>> r) {
    r.z = 1;
}

sub set_typed_list(reference<list<int>> r) {
    r += 1;
}

sub set_soft_object(reference<*object> r) {
    r.x = 4;
}

sub run(string label, code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

sub locals() {
    C o();
    object w := o;
    set_object(\w);
    printf("object: %d\n", o.x);
    set_class(\w);
    printf("class: %d\n", o.x);

    hash h = {"a": 1};
    hash wh := h;
    set_hash(\wh);
    printf("hash: %y\n", h);
    set_auto_hash(\wh);
    printf("hash<auto>: %y\n", h);
    # the variable keeps the weak reference: it does not keep the hash alive
    h = {"other": 1};
    printf("hash after the variable changed: %y\n", wh);

    list l = (1,);
    list wl := l;
    set_list(\wl);
    printf("list: %y\n", l);
}

sub destroyed_and_freed() {
    # a destroyed object is the target of the weak reference, and is bound as for a variable that holds it: the plain
    # object type refuses it, a class type and *object accept it, and a change through the reference raises
    # OBJECT-ALREADY-DELETED
    C o();
    object w := o;
    delete o;
    run("destroyed object", sub () { set_object(\w); });
    run("destroyed object of a class", sub () { set_class(\w); });
    auto wa;
    {
        C o2();
        wa := o2;
        delete o2;
    }
    run("destroyed object, *object", sub () { set_soft_object(\wa); });

    # a hash or a list freed under the weak reference is an empty one, which cannot be changed
    hash wh;
    {
        hash h = {"a": 1};
        wh := h;
    }
    run("freed hash", sub () { set_hash(\wh); });
    printf("freed hash: %y\n", wh);
    list wl;
    {
        list l = (1,);
        wl := l;
    }
    run("freed list", sub () { set_list(\wl); });
    printf("freed list: %y\n", wl);

    # a hashdecl hash, live and freed
    hash<D> d();
    auto wd;
    wd := d;
    set_decl(\wd);
    printf("hashdecl: %y\n", d);
    {
        hash<D> d2();
        wd := d2;
    }
    run("freed hashdecl", sub () { set_decl(\wd); });
}

sub refused() {

    # the type would convert the target to another value, which the weak reference would not designate
    hash<string, int> hi = {"a": 1};
    auto wa;
    wa := hi;
    run("converted hash", sub () { set_hash(\wa); });
    printf("converted hash: %y\n", hi);

    # an untyped container that a complex type would convert
    hash ha = {"a": 1};
    auto wb;
    wb := ha;
    run("converted to hash<string, int>", sub () { set_typed_hash(\wb); });
    printf("converted to hash<string, int>: %y\n", ha);
    list la = (1,);
    auto wc;
    wc := la;
    run("converted to list<int>", sub () { set_typed_list(\wc); });
    printf("converted to list<int>: %y\n", la);
}

our C ro_obj();
our object ro_weak;

#! a read-only variable imported into a Program is bound without being written (qore#5664)
sub read_only_import() {
    ro_weak := ro_obj;
    Program p(PO_NEW_STYLE);
    p.importGlobalVariable("ro_weak", True);
    p.parse("int sub get_x(reference<object> r) { return r.x; } int sub run() { return get_x(\\ro_weak); }", "ro");
    printf("read-only import: %d\n", p.callFunction("run"));
}

our C go();
our object gw;

sub global_variable() {
    gw := go;
    set_object(\gw);
    printf("global: %d\n", go.x);
}

locals();
destroyed_and_freed();
refused();
read_only_import();
global_variable();
