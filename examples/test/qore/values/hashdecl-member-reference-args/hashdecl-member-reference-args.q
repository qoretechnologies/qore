#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the member initializers of hashdecls and classes pass references to global variables, their elements and static
# class members as the arguments of calls; in a qcc executable, a reference to a global variable in a hashdecl member
# initializer was NOTHING (qore#7493)

%modern

our auto g = "5";
our hash<auto> gh = {"k": 7};
our list<auto> gl = (1, 11);
our softint gi = 4;
our auto w;
our auto w2;
our code gc = int sub (reference<softint> r) { return r + 1000; };

int sub f(reference<softint> r) {
    return r;
}

# writes the referenced lvalue
int sub set_ref(reference<auto> r, auto v) {
    r = v;
    return 1;
}

class RefHelper {
    public {
        static softint sv = 3;
    }

    static int sget(reference<softint> r) {
        return r;
    }

    int get(reference<softint> r) {
        return r + 100;
    }
}

hashdecl RefDefaults {
    # a reference to a global variable as the argument of a function
    int fg = f(\g);
    # a reference to an element of a global hash
    int fh = f(\gh.k);
    # a reference to an element of a global list
    int fl = f(\gl[1]);
    # a reference to a static class member
    int fs = f(\RefHelper::sv);
    # a reference as the argument of a static method
    int sm = RefHelper::sget(\g);
    # a reference as the argument of a method of an object
    int om = (new RefHelper()).get(\g);
    # a reference as the argument of a closure
    int cl = gc(\g);
    # a global variable that is read
    softint rd = gi;
    # a reference that the call writes
    int wr = set_ref(\w, "w");
}

# a generic hashdecl, which a function signature instantiates
hashdecl GenRef<T> {
    int a = f(\g);
    T b;
}

hash<GenRef<string>> sub make_gen() {
    hash<GenRef<string>> h({"b": "x"});
    return h;
}

class RefMembers {
    public {
        int fg = f(\g);
        int fh = f(\gh.k);
        int fl = f(\gl[1]);
        int fs = f(\RefHelper::sv);
        int sm = RefHelper::sget(\g);
        int om = (new RefHelper()).get(\g);
        int cl = gc(\g);
        softint rd = gi;
        int wr = set_ref(\w2, "w2");
    }
}

# a hashdecl that inherits a hashdecl whose member initializers pass references
hashdecl RefChild inherits RefDefaults {
    int x = f(\g) + 1;
}

# a generic hashdecl that inherits a generic hashdecl, which a function signature instantiates
hashdecl GenChild<T> inherits GenRef<T> {
    int c = f(\gl[1]);
}

hash<GenChild<string>> sub make_gen_child() {
    hash<GenChild<string>> h({"b": "y"});
    return h;
}

hash<RefDefaults> h = cast<hash<RefDefaults>>({});
printf("hashdecl: %y\n", h);
printf("g: %y w: %y\n", g, w);
printf("generic hashdecl: %y\n", make_gen());
RefMembers o();
printf("class: fg=%y fh=%y fl=%y fs=%y sm=%y om=%y cl=%y rd=%y wr=%y\n", o.fg, o.fh, o.fl, o.fs, o.sm, o.om, o.cl,
    o.rd, o.wr);
printf("w2: %y\n", w2);
printf("inherited hashdecl: %y\n", cast<hash<RefChild>>({}));
printf("generic child hashdecl: %y\n", make_gen_child());
