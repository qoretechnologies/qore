#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Applies the compound assignments that an or-nothing lvalue (*T) accepts at parse time to locals, globals, class
# members and hashdecl members, and prints one line per operation with the value and type of the lvalue afterwards.
# The cases run three times, so that tiered execution compiles them to native code, and print in the last run.

%modern

#! a class with or-nothing members
class OrNothingMembers {
    public {
        *int i;
        *string s;
        *float f;
    }
}

#! a hashdecl with or-nothing members
hashdecl OrNothingHash {
    *int i;
    *string s;
    *list<auto> l;
}

our *int gi;
our *string gs;
our *date gd;

#! true in the last run, which prints the results
our bool print_results;

#! prints one result
sub show(string label, auto v) {
    if (print_results) {
        printf("%s: %y %s\n", label, v, v.type());
    }
}

sub run_locals() {
    *int i;
    i += 1;
    show("*int += 1", i);
    i -= NOTHING;
    show("*int -= NOTHING", i);
    i *= 6;
    show("*int *= 6", i);
    i /= 2;
    show("*int /= 2", i);
    i -= 1;
    show("*int -= 1", i);
    i %= 2;
    show("*int %= 2", i);
    i |= 4;
    show("*int |= 4", i);
    i &= 5;
    show("*int &= 5", i);
    i ^= 1;
    show("*int ^= 1", i);
    i <<= 2;
    show("*int <<= 2", i);
    i >>= 1;
    show("*int >>= 1", i);

    *int none;
    none -= NOTHING;
    show("*int NOTHING -= NOTHING", none);
    none -= 2;
    show("*int NOTHING -= 2", none);

    *string s;
    s += "a";
    show("*string += \"a\"", s);
    *string no_string;
    s += no_string;
    show("*string += NOTHING", s);
    s += 1;
    show("*string += 1", s);

    *list<auto> l;
    l += 1;
    show("*list += 1", l);
    l += (2, 3);
    show("*list += (2, 3)", l);

    *hash<auto> h;
    h += {};
    show("*hash += {}", h);
    h += {"a": 1, "b": 2};
    show("*hash += {a, b}", h);
    h -= "a";
    show("*hash -= \"a\"", h);

    *date d;
    d += 1D;
    show("*date += 1D", d);
    d -= 2h;
    show("*date -= 2h", d);

    *float f;
    f += 1;
    show("*float += 1", f);
    f -= 1.5;
    show("*float -= 1.5", f);
    f *= 2;
    show("*float *= 2", f);
    f /= 4;
    show("*float /= 4", f);
    f += 1n;
    show("*float += 1n", f);

    *number n;
    n += 1;
    show("*number += 1", n);
    n *= 1.5;
    show("*number *= 1.5", n);
    n -= 1n;
    show("*number -= 1n", n);
    n /= 2;
    show("*number /= 2", n);

    *binary b;
    b += <01>;
    show("*binary += <01>", b);
    b += "a";
    show("*binary += \"a\"", b);
}

sub run_globals() {
    remove gi;
    remove gs;
    remove gd;
    gi += 2;
    gi *= 3;
    show("global *int", gi);
    gs += "x";
    gs += 2;
    show("global *string", gs);
    gd += 1D;
    show("global *date", gd);
}

sub run_members() {
    OrNothingMembers o();
    o.i += 3;
    o.i -= 1;
    show("member *int", o.i);
    o.s += "m";
    o.s += 1.5;
    show("member *string", o.s);
    o.f -= 1;
    o.f *= 3;
    show("member *float", o.f);

    hash<OrNothingHash> h();
    h.i += 4;
    h.i /= 2;
    show("hashdecl *int", h.i);
    h.s += "h";
    show("hashdecl *string", h.s);
    h.l += 1;
    h.l += "two";
    show("hashdecl *list", h.l);
}

for (int run = 0; run < 3; ++run) {
    print_results = run == 2;
    run_locals();
    run_globals();
    run_members();
}
