%modern
%allow-weak-references
%perl-bool-eval
# the program of weak-reference-size-during-free.qtest: the boolean conversion and the complex type match of a list or
# hash read through a weak reference while another thread frees it read the container as freed, with a reference of
# their own (qore#7375); every execution mode must print the output of the AST interpreter
#
# Each case holds the thread that releases the last reference to the container at the start of its free with a debug
# hook (dbg_hold_container_free()), reads the container's size through the weak reference, then lets the free finish.
# The language reads a weak reference that it evaluates through its target; native code gives the weak reference
# itself to the boolean conversion and the complex type match, which dbg_weak_reference_reads() does.  Before the fix
# these read the size of the container whose free had started, without a reference (the containers read as not empty,
# and the free wrote the size meanwhile).

class Holder {
    public {
        list<auto> l;
        hash<auto> h;
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
#! lets the free finish
sub during_free(string type, code free, code read) {
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
    }
    read();
}

#! the boolean conversion of the weak reference
sub boolean_conversion() {
    Holder c();
    c.l = mklist(5);
    auto wl;
    wl := c.l;
    during_free("list", sub () { c.l = (); }, sub () { printf("list as a boolean: %y\n", boolean(wl)); });
    c.h = mkhash(5);
    auto wh;
    wh := c.h;
    during_free("hash", sub () { c.h = {}; }, sub () { printf("hash as a boolean: %y\n", boolean(wh)); });

    # a condition
    c.l = mklist(5);
    wl := c.l;
    during_free("list", sub () { c.l = (); }, sub () { printf("list as a condition: %s\n", wl ? "set" : "empty"); });
    c.h = mkhash(5);
    wh := c.h;
    during_free("hash", sub () { c.h = {}; }, sub () { printf("hash as a condition: %s\n", wh ? "set" : "empty"); });
}

#! the complex type match: an empty container with no type matches any complex type of its kind
sub type_match() {
    Holder c();
    c.l = mklist(5);
    auto wl;
    wl := c.l;
    during_free("list", sub () { c.l = (); }, sub () {
        printf("list instanceof list<int>: %y\n", wl instanceof list<int>);
    });
    c.h = mkhash(5);
    auto wh;
    wh := c.h;
    during_free("hash", sub () { c.h = {}; }, sub () {
        printf("hash instanceof hash<string, int>: %y\n", wh instanceof hash<string, int>);
    });
}

#! the same reads of a live container
sub live() {
    list<auto> l = mklist(2);
    auto wl;
    wl := l;
    hash<auto> h = mkhash(2);
    auto wh;
    wh := h;
    printf("live: %y %y %y %y\n", boolean(wl), boolean(wh), wl instanceof list<int>, wh instanceof hash<string, int>);
    list<auto> e = ();
    auto we;
    we := e;
    printf("live empty: %y %y\n", boolean(we), we instanceof list<int>);
}

#! the reads of the weak reference as it is held, by native code (dbg_weak_reference_reads())
sub raw_reads() {
    Holder c();
    list<auto!> holder = (NOTHING,);
    c.l = mklist(5);
    holder[0] := c.l;
    during_free("list", sub () { c.l = (); }, sub () {
        printf("raw list: %y\n", call_function("dbg_weak_reference_reads", holder));
    });
    c.h = mkhash(5);
    holder[0] := c.h;
    during_free("hash", sub () { c.h = {}; }, sub () {
        printf("raw hash: %y\n", call_function("dbg_weak_reference_reads", holder));
    });

    # a freed container keeps its type: an empty list<string> matches list<string> only
    list<string> ls = ("a", "b", "c");
    holder[0] := ls;
    during_free("list", sub () { ls = (); }, sub () {
        printf("raw list<string>: %y\n", call_function("dbg_weak_reference_reads", holder));
    });
    hash<string, int> hs = {"a": 1, "b": 2};
    holder[0] := hs;
    during_free("hash", sub () { hs = {}; }, sub () {
        printf("raw hash<string, int>: %y\n", call_function("dbg_weak_reference_reads", holder));
    });

    # live containers
    list<string> lls = ("a",);
    holder[0] := lls;
    printf("raw live list<string>: %y\n", call_function("dbg_weak_reference_reads", holder));
    list<auto> le = ();
    holder[0] := le;
    printf("raw live empty list: %y\n", call_function("dbg_weak_reference_reads", holder));
}

boolean_conversion();
type_match();
live();
raw_reads();
