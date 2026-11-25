%modern
%allow-weak-references
# the second program of weak-reference-read-during-free.qtest, run by the AST and IR interpreters only: the variable of
# a foreach loop that holds a weak reference, and the elements of a list read by an identity map, which hold weak
# references, read the list or hash with a reference of its own while another thread frees it (qore#7198).  Native
# code reads them borrowed, as the code generator expects these runtime helpers to return them; their ownership
# changes with the AOT batch, and until then the reference that the weak holders of a container being freed hold keeps
# such a read from deleting it (qore#7163).

class Holder {
    public {
        list<auto> l;
    }
}

list<auto> sub mklist(int n) {
    list<auto> r = ();
    for (int i = 0; i < n; ++i) {
        r += sprintf("s%d", i);
    }
    return r;
}

#! starts \a free in a thread that is held when the free of a list starts, and returns the Counter that releases it;
#! the reads are made in the caller's own body, not in a closure, so that they are those of its function
Counter sub start_free(code free, Counter done) {
    Counter held(1);
    Counter release(1);
    call_function("dbg_hold_container_free", "list", held, release);
    background sub () {
        on_exit done.dec();
        free();
    }();
    held.waitForZero();
    return release;
}

#! the variable of a foreach loop over a list of weak references
sub foreach_var() {
    Holder c();
    c.l = mklist(5);
    list<auto!> holder = (1,);
    holder[0] := c.l;
    list<auto> x;
    int before;
    Counter done(1);
    Counter release = start_free(sub () { c.l = (); }, done);
    # the free is released, and waited for, also if a read raises an exception
    on_exit {
        release.dec();
        done.waitForZero();
        printf("foreach list: %d %d\n", before, x.size());
    }
    foreach auto e in (holder) {
        x = e;
    }
    before = x.size();
}

#! an identity map of a list of weak references
sub identity_map() {
    Holder c();
    c.l = mklist(5);
    list<auto!> holder = (1,);
    holder[0] := c.l;
    list<auto> m;
    int before;
    Counter done(1);
    Counter release = start_free(sub () { c.l = (); }, done);
    # the free is released, and waited for, also if a read raises an exception
    on_exit {
        release.dec();
        done.waitForZero();
        printf("identity map: %d %d\n", before, m[0].size());
    }
    m = map $1, holder;
    before = m[0].size();
}

foreach_var();
identity_map();
