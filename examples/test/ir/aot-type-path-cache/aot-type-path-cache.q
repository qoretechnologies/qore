%modern
%disable-warning unreferenced-variable

# typed container literals and typed locals in compiled code (qore#5979): a type path of only builtin type keywords
# is resolved once and kept at the address of the path; the other paths (a hashdecl, a class, a type argument) keep the
# receiver- and instantiation-aware cache.  The results must be those of the AST interpreter in every execution.

hashdecl Point {
    int x;
    int y;
}

class Widget {
    public {
        int v = 3;
    }
}

class Box<T> {
    public {
        list<T> items = ();
    }

    add(T t) {
        items += t;
    }

    list<T> get() {
        list<T> copy = items;
        return copy;
    }
}

auto sub text() {
    return "text";
}

sub show(string label, auto value) {
    printf("%s: %y (%s)\n", label, value, value.fullType());
}

sub builtin() {
    # each literal has a path of its own, run several times: the first run fills the cache, the others read it
    for (int i = 0; i < 3; ++i) {
        hash<string, int> h1 = {"a": i};
        list<int> l1 = (i, i + 1);
        list<*int> l2 = (i, NOTHING);
        hash<string, list<int>> h2 = {"k": (i,)};
        hash<string, hash<string, float>> h3 = {"o": {"f": 1.5 + i}};
        list<list<string>> l3 = (("x", "y"),);
        hash<auto> h4 = {"z": "s", "n": i};
        list<auto> l4 = (1, "two", 3.0);
        list<softint> l5 = ("4", 5);
        if (i == 2) {
            show("hash<string, int>", h1);
            show("list<int>", l1);
            show("list<*int>", l2);
            show("hash<string, list<int>>", h2);
            show("nested hash", h3);
            show("list<list<string>>", l3);
            show("hash<auto>", h4);
            show("list<auto>", l4);
            show("list<softint>", l5);
        }
    }
}

sub typeError() {
    # a value that the type does not accept raises the error every time, also after the type was cached
    for (int i = 0; i < 3; ++i) {
        try {
            hash<string, int> h = cast<hash<string, int>>({"a": text()});
            show("no error", h);
        } catch (hash<ExceptionInfo> ex) {
            printf("%d: %s\n", i, ex.err);
        }
        try {
            list<int> l = cast<list<int>>((text(), i));
            show("no error", l);
        } catch (hash<ExceptionInfo> ex) {
            printf("%d: %s\n", i, ex.err);
        }
    }
}

sub nonBuiltin() {
    for (int i = 0; i < 3; ++i) {
        hash<Point> p = cast<hash<Point>>({"x": i, "y": i + 1});
        list<Widget> w = (new Widget(), new Widget());
        list<hash<Point>> lp = (cast<hash<Point>>({"x": 1, "y": 2}),);
        Box<int> b();
        b.add(i);
        b.add(i + 1);
        if (i == 2) {
            show("hashdecl", p);
            printf("class list: %d\n", w.size());
            show("list of hashdecl", lp);
            show("generic class", b.get());
        }
    }
}

# the same function body run with different types of the receiver: a generic class instantiated twice
sub generic() {
    Box<string> bs();
    bs.add("a");
    Box<float> bf();
    bf.add(1.5);
    show("Box<string>", bs.get());
    show("Box<float>", bf.get());
}

builtin();
typeError();
nonBuiltin();
generic();
