%modern
%allow-weak-references
%allow-opaque-references
# the program of weak-reference-typed-store.qtest: a weak assignment to a typed variable that is not a plain local
# (qore#6979); every execution mode must print the output of the AST interpreter

our int destroyed = 0;

class C {
    destructor() { ++destroyed; }
}

class Other {
}

#! a global and a thread-local of object types
our *object g;
our *C gc;
thread_local *object tl;

#! prints whether the weak reference designates the object, then releases the only strong reference and prints it again
sub check(string what, code get, code release) {
    int before = destroyed;
    printf("%s: %s\n", what, get() ? "set" : "unset");
    release();
    printf("%s after release: %s destroyed=%d\n", what, get() ? "set" : "unset", destroyed - before);
}

sub closure_locals() {
    {
        C c();
        *object w;
        # the closure captures w, so w is not a plain local
        code get = auto sub () { return w; };
        w := c;
        check("closure *object", get, sub () { remove c; });
    }
    {
        C c();
        object w = new Other();
        code get = auto sub () { return w; };
        w := c;
        check("closure object", get, sub () { remove c; });
    }
    {
        C c();
        *C w;
        code get = auto sub () { return w; };
        w := c;
        check("closure *C", get, sub () { remove c; });
    }
    {
        # the value of the assignment
        C c();
        *object w;
        code get = auto sub () { return w; };
        auto v = (w := c);
        printf("closure value: %s\n", v ? v.className() : "none");
        remove v;
        check("closure value", get, sub () { remove c; });
    }
    {
        # the value of the assignment is a strong reference: it keeps the object
        C c();
        *object w;
        code get = auto sub () { return w; };
        auto v = (w := c);
        int before = destroyed;
        remove c;
        printf("closure value kept: %s %s destroyed=%d\n", v ? "set" : "unset", get() ? "set" : "unset",
            destroyed - before);
        remove v;
        printf("closure value released: %s destroyed=%d\n", get() ? "set" : "unset", destroyed - before);
    }
    {
        # assigned in the closure: a closure variable
        C c();
        *object w;
        code set = sub (C o) { w := o; };
        set(c);
        check("closure variable", auto sub () { return w; }, sub () { remove c; });
    }
    {
        # declarations with a weak initializer
        C c();
        *object w := c;
        const *object cw := c;
        code get = auto sub () { return w; };
        code cget = auto sub () { return cw; };
        printf("declaration: %s const %s\n", get() ? "set" : "unset", cget() ? "set" : "unset");
        check("declaration", get, sub () { remove c; });
        printf("const declaration after release: %s\n", cget() ? "set" : "unset");
    }
    {
        # a declaration in a loop, captured by several closures
        list<C> objs = (new C(), new C());
        list<code> gets;
        foreach C o in (objs) {
            *C w := o;
            gets += auto sub () { return w; };
            gets += auto sub () { return w ? w.className() : NOTHING; };
        }
        printf("loop: %y\n", (map $1() ? "set" : "unset", gets));
        int before = destroyed;
        remove objs;
        printf("loop after release: %y destroyed=%d\n", (map $1() ? "set" : "unset", gets), destroyed - before);
    }
    {
        # captured by a thread
        C c();
        *object w;
        Queue q();
        background sub () { w := c; q.push(1); }();
        q.get();
        check("thread closure", auto sub () { return w; }, sub () { remove c; });
    }
}

sub globals() {
    {
        C c();
        g := c;
        check("global *object", auto sub () { return g; }, sub () { remove c; });
    }
    {
        C c();
        gc := c;
        check("global *C", auto sub () { return gc; }, sub () { remove c; });
    }
    {
        C c();
        tl := c;
        check("thread_local *object", auto sub () { return tl; }, sub () { remove c; });
    }
}

thread_local *C tlc;

#! runs code that is expected to raise an exception, and prints it
sub expect_error(string what, code c) {
    try {
        c();
        printf("%s: no exception\n", what);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", what, ex.err, ex.desc);
    }
}

#! a value of a type that the variable does not accept is refused as in a normal assignment, and the variable keeps
#! its value
sub refused() {
    auto other = new Other();
    auto nothing;
    {
        *C w;
        code get = auto sub () { return w; };
        expect_error("closure *C refused", sub () { w := other; });
        printf("closure *C refused: %s\n", get() ? "set" : "unset");
    }
    {
        C c();
        object w = c;
        code get = auto sub () { return w; };
        expect_error("closure object refused NOTHING", sub () { w := nothing; });
        printf("closure object refused NOTHING: %s\n", get() ? "kept" : "unset");
    }
    {
        # assigned in the closure that refuses it
        *C w;
        code set = sub (auto v) { w := v; };
        expect_error("closure variable *C refused", sub () { set(other); });
        printf("closure variable *C refused: %s\n", w ? "set" : "unset");
    }
    {
        # no closure captures it
        *C w;
        try {
            w := other;
            printf("plain local *C refused: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            printf("plain local *C refused: %s: %s\n", ex.err, ex.desc);
        }
        printf("plain local *C refused: %s\n", w ? "set" : "unset");
    }
    expect_error("global *C refused", sub () { gc := other; });
    printf("global *C refused: %s\n", gc ? "set" : "unset");
    expect_error("thread_local *C refused", sub () { tlc := other; });
    printf("thread_local *C refused: %s\n", tlc ? "set" : "unset");
}

#! the weak and opaque assignments that worked before the fix
sub worked() {
    {
        # no closure captures it
        C c();
        *object w;
        w := c;
        int before = destroyed;
        printf("plain local *object: %s\n", w ? "set" : "unset");
        remove c;
        printf("plain local *object after release: %s destroyed=%d\n", w ? "set" : "unset", destroyed - before);
    }
    {
        C c();
        auto w;
        code get = auto sub () { return w; };
        w := c;
        check("closure auto", get, sub () { remove c; });
    }
    {
        hash<auto> h = {"a": 1};
        *hash<auto> w;
        code get = auto sub () { return w; };
        w := h;
        printf("closure *hash: %y\n", get());
    }
    {
        list<auto> l = (1, 2);
        *list<auto> w;
        code get = auto sub () { return w; };
        w := l;
        printf("closure *list: %y\n", get());
    }
    {
        # an opaque reference keeps its target
        C c();
        *object w;
        code get = auto sub () { return w; };
        w @= c;
        check("closure opaque", get, sub () { remove c; });
        remove w;
    }
}

closure_locals();
globals();
refused();
worked();
printf("done destroyed=%d\n", destroyed);
