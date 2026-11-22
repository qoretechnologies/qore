%modern
%allow-weak-references
# the program of weak-reference-read-during-free.qtest: a list or hash read through a weak reference while another
# thread frees it reads as freed (qore#7198); every execution mode must print the output of the AST interpreter
#
# Each case holds the thread that releases the last reference to the container at the start of its free with a debug
# hook (dbg_hold_container_free()), reads the container through a weak reference, then lets the free finish: a value
# read with a reference of its own must not change under it.  Before the fix the read referenced the container whose
# free had started, and its entries were freed under the reference (the sizes printed were 5 and 0).

class Holder {
    public {
        list<auto> l;
        list<auto> wl;
        hash<auto> h;
        hash<auto> wh;
    }
}

list<auto> sub mklist(int n) {
    list<auto> r = ();
    for (int i = 0; i < n; ++i) {
        r += sprintf("s%d", i);
    }
    return r;
}

hash<auto> sub mkhash(int n) {
    hash<auto> r = {};
    for (int i = 0; i < n; ++i) {
        r{"k" + i} = sprintf("s%d", i);
    }
    return r;
}

#! runs \a free in a thread that is held when the free of a container of the given type starts, then \a read, then
#! lets the free finish and runs \a after
sub during_free(string type, code free, code read, code after) {
    Counter held(1);
    Counter release(1);
    Counter done(1);
    call_function("dbg_hold_container_free", type, held, release);
    background sub () {
        on_exit done.dec();
        free();
    }();
    held.waitForZero();
    on_exit {
        release.dec();
        done.waitForZero();
        after();
    }
    read();
}

our list<auto> gwl;
our hash<auto> gwh;

#! an object member that holds a weak reference
sub member() {
    Holder c();
    c.l = mklist(5);
    c.wl := c.l;
    list<auto> x;
    int before;
    during_free("list", sub () { c.l = (); }, sub () { x = c.wl; before = x.size(); },
        sub () { printf("member list: %d %d\n", before, x.size()); });
    c.h = mkhash(5);
    c.wh := c.h;
    hash<auto> y;
    during_free("hash", sub () { c.h = {}; }, sub () { y = c.wh; before = y.size(); },
        sub () { printf("member hash: %d %d\n", before, y.size()); });
}

#! a global variable that holds a weak reference
sub global_var() {
    Holder c();
    c.l = mklist(5);
    gwl := c.l;
    list<auto> x;
    int before;
    during_free("list", sub () { c.l = (); }, sub () { x = gwl; before = x.size(); },
        sub () { printf("global list: %d %d\n", before, x.size()); });
    c.h = mkhash(5);
    gwh := c.h;
    hash<auto> y;
    during_free("hash", sub () { c.h = {}; }, sub () { y = gwh; before = y.size(); },
        sub () { printf("global hash: %d %d\n", before, y.size()); });
}

#! a member of a hash read through a weak reference to the hash, and a pseudo-method called through it
sub hash_member() {
    Holder c();
    c.h = mkhash(5);
    c.wh := c.h;
    *string v;
    int size;
    during_free("hash", sub () { c.h = {}; }, sub () { v = c.wh.k1; size = c.wh.size(); },
        sub () { printf("hash member: %y %d\n", v, size); });
}

#! a list element that holds a weak reference, and a hash value that holds one
sub element() {
    Holder c();
    c.l = mklist(5);
    list<auto!> holder = (1,);
    holder[0] := c.l;
    list<auto> x;
    int before;
    during_free("list", sub () { c.l = (); }, sub () { x = holder[0]; before = x.size(); },
        sub () { printf("list element: %d %d\n", before, x.size()); });
    c.h = mkhash(5);
    hash<auto!> hh = {"a": 1};
    hh.a := c.h;
    hash<auto> y;
    during_free("hash", sub () { c.h = {}; }, sub () { y = hh.a; before = y.size(); },
        sub () { printf("hash value: %d %d\n", before, y.size()); });
}

#! a freed container read through a weak reference cannot be changed, and has its type
sub freed_value() {
    Holder c();
    c.l = mklist(5);
    c.wl := c.l;
    list<auto> x;
    during_free("list", sub () { c.l = (); }, sub () { x = c.wl; }, sub () {
        try {
            x += 1;
            printf("freed list: changed\n");
        } catch (hash<ExceptionInfo> ex) {
            printf("freed list: %s %s\n", ex.err, x.type());
        }
    });
}

member();
global_var();
hash_member();
element();
freed_value();
