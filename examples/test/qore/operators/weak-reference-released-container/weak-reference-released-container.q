%modern
%allow-weak-references
# the program of weak-reference-released-container.qtest: a weak reference to a list or a hash whose last strong
# reference was released reads as an empty container, also when it is copied, returned or given to a call (qore#7163);
# every execution mode must print the output of the AST interpreter

#! returns a local that holds a weak reference to a released list
auto sub ret_list() {
    list<auto> m;
    m := list(8);
    return m;
}

#! returns a local that holds a weak reference to a released hash
auto sub ret_hash() {
    hash<auto> m;
    # built at run time: a literal is held by the program
    m := mkhash(1);
    return m;
}

#! returns a hash that the caller holds the only reference to
hash<auto> sub mkhash(int v) {
    return {"a": v};
}

#! returns a softlist local whose converted list was released
auto sub ret_softlist() {
    softlist<auto> m;
    m := 8;
    return m;
}

*list<auto> sub id_list(*list<auto> l) { return l; }
*hash<auto> sub id_hash(*hash<auto> h) { return h; }

sub copies() {
    list<auto> l;
    l := list(1, 2);
    hash<auto> h;
    h := mkhash(2);
    printf("in place: %y %y\n", l, h);
    auto cl = l;
    auto ch = h;
    printf("copies: %y %y\n", cl, ch);
    list<auto> tl = l;
    hash<auto> th = h;
    printf("typed copies: %y %y\n", tl, th);
    printf("in a list: %y\n", (l, h));
    printf("arguments: %y %y\n", id_list(l), id_hash(h));
    remove cl;
    remove ch;
    remove tl;
    remove th;
    printf("after the copies: %y %y\n", l, h);
}

#! runs a change of a released container through a new reference to it and prints the exception it raises
sub change(string name, code c) {
    try {
        c();
        printf("%s: changed\n", name);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
}

#! a released container cannot be changed through a new reference to it, also when the reference is not its only
#! one, since its weak holders hold one too: a copy of it cannot be changed either
sub changes() {
    list<auto> l;
    l := list(1, 2);
    hash<auto> h;
    h := mkhash(3);
    {
        list<auto> x = l;
        change("list +=", sub () { x += 3; });
        change("list index", sub () { x[0] = 3; });
        change("list push", sub () { push x, 3; });
        change("list unshift", sub () { unshift x, 3; });
        list<auto> y = l;
        change("list with two references", sub () { x += 3; });
        printf("list references: %y %y %y\n", x, y, l);
    }
    {
        hash<auto> x = h;
        change("hash member", sub () { x.b = 3; });
        hash<auto> y = h;
        change("hash with two references", sub () { x.b = 3; });
        string k = "c";
        change("hash dynamic key", sub () { y{k} = 3; });
        printf("hash references: %y %y %y\n", x, y, h);
    }
    # a weak reference to the copy of a released list made by a change, released before and after the copy
    list<auto> x = l;
    change("list copy +=", sub () { x += 3; });
    list<auto> wc;
    wc := x;
    remove wc;
    printf("copy after its weak holder: %y\n", x);
    wc := x;
    remove x;
    printf("weak holder after the copy: %y\n", wc);
    x = wc;
    change("copy through its weak holder", sub () { x += 3; });
    remove x;
    remove wc;
    printf("new containers: %y %y\n", (1, 2) + 3, {"a": 1} + {"b": 2});

    # values computed from a released container are new containers, which can be changed
    list<auto> sl;
    sl := list(4, 3);
    list<auto> sorted = sort(sl);
    sorted += 1;
    list<auto> plus = sl + 2;
    plus += 1;
    hash<auto> sh;
    sh := mkhash(4);
    hash<auto> hplus = sh + {"b": 1};
    hplus.c = 2;
    printf("computed: %y %y %y\n", sorted, plus, hplus);
}

#! a released container read through a weak reference is given a weak holder again after its weak holders released
#! theirs
/** Reads through weak references take no reference to a freed container (qore#7198): \c s is a new empty container,
    which its weak holders keep as any other container.  Native code that reads the variable of a foreach loop with a
    borrowed read can still take a reference to the freed container itself, which then gets a weak holder again
    (QoreListNode::weakRef()); that read changes with the AOT batch.
*/
sub weak_holder_again() {
    list<auto> w;
    w := list(1, 2);
    hash<auto> wh;
    wh := mkhash(5);
    auto s = w;
    auto sh = wh;
    # the last weak holders release theirs
    w = ();
    wh = {};
    # a weak holder again, released after the strong reference
    list<auto> w2;
    w2 := s;
    hash<auto> wh2;
    wh2 := sh;
    remove s;
    remove sh;
    printf("weak holder after the reference: %y %y\n", w2, wh2);
    # a weak holder again, released before the strong reference
    auto s2 = w2;
    auto sh2 = wh2;
    w2 = ();
    wh2 = {};
    {
        list<auto> w3;
        w3 := s2;
        hash<auto> wh3;
        wh3 := sh2;
    }
    printf("reference after the weak holder: %y %y\n", s2, sh2);
}

#! the weak holders of containers whose entries' destructors read them while the containers are freed
list<auto> gwl;
hash<auto> gwh;

#! reads the weak holders in its destructor, while the container that holds it is freed
class Reader {
    public { string name; }
    constructor(string name) { self.name = name; }
    destructor() {
        # the entries that the container has while it is freed depend on the order of its free: only that reading
        # it works is printed
        auto l = gwl;
        auto h = gwh;
        printf("%s destructor: %s %s\n", name, l.type(), h.type());
    }
}

#! keeps the reference that it takes through the weak holder after the container is freed
class Keeper {
    destructor() {
        our list<auto> kept = gwl;
    }
}

#! a destructor of an entry takes a reference to the container being freed through a weak holder, in the same thread
sub free_with_readers() {
    list<auto> l = (1, new Reader("list entry"), 2);
    gwl := l;
    remove l;
    printf("list after its free: %y\n", gwl);
    hash<auto> h = {"a": new Reader("hash member"), "b": 2};
    gwh := h;
    remove h;
    printf("hash after its free: %y\n", gwh);
    # a reference kept after the free, released after the holder
    list<auto> k = (new Keeper(),);
    gwl := k;
    remove k;
    printf("kept: %y\n", kept);
    gwl = ();
    printf("kept after the holder: %y\n", kept);
    remove kept;
    # a hash nested deeper than the recursion limit of the free, freed by the iterative loop
    hash<auto!> dh = {"a": new Reader("nested hash member"), "b": 2};
    gwh := dh;
    auto v = dh;
    remove dh;
    for (int i = 0; i < 40; ++i) {
        v = (v,);
    }
    remove v;
    printf("nested hash after its free: %y\n", gwh);
    # a list nested in the same way
    list<auto!> dl = (new Reader("nested list entry"),);
    gwl := dl;
    auto w = dl;
    remove dl;
    for (int i = 0; i < 40; ++i) {
        w = (w,);
    }
    remove w;
    printf("nested list after its free: %y\n", gwl);
}

printf("returned list: %y\n", ret_list());
printf("returned hash: %y\n", ret_hash());
printf("returned softlist: %y\n", ret_softlist());
copies();
changes();
weak_holder_again();
free_with_readers();
# a copy that outlives the weak holder
auto keep = ret_list();
printf("kept: %y %d\n", keep, keep.size());
remove keep;

# threads read and copy a weak reference to a released list while others release theirs: no crash, no leak
Counter done();
for (int t = 0; t < 4; ++t) {
    done.inc();
    background sub () {
        on_exit done.dec();
        for (int i = 0; i < 2000; ++i) {
            list<auto> w;
            w := list(i);
            auto c = w;
            list<auto> c2 = w;
            remove c;
            remove c2;
        }
    }();
}
done.waitForZero();
printf("threads: done\n");

#! a weak holder that threads share: one copies the released container through it while another releases the holder
class Holder {
    public {
        list<auto> l;
        hash<auto> h;
    }
}

list<auto> sub mklist(int i) { return (i, i + 1); }

for (int i = 0; i < 4000; ++i) {
    Holder c();
    c.l := mklist(i);
    c.h := mkhash(i);
    Counter go(1);
    Counter finished(2);
    background sub () {
        on_exit finished.dec();
        go.waitForZero();
        auto l = c.l;
        auto h = c.h;
        remove l;
        remove h;
    }();
    background sub () {
        on_exit finished.dec();
        go.waitForZero();
        c.l = ();
        c.h = {};
    }();
    go.dec();
    finished.waitForZero();
}
printf("shared holders: done\n");
