# probes of generic-background-type-args.qtest (qore#5732): a background expression started in generic code runs
# with the type arguments of that code's call, as it would in that code; probes() returns one line per pass, and each
# pass runs every probe, so that the code runs on every tier with tiered execution

%modern

X sub identity<X>(X x) { return x; }

class G {
    static hash<string, auto> ctorF<T>(T v) { return {"v": v, "t": v.fullType()}; }

    # the background expression calls a generic static method with the method's type parameter
    static auto outer<U>(U v) {
        Queue q();
        background q.push(G::ctorF<U>(v));
        return q.get(10s);
    }

    # the background expression calls a function with the method's type parameter
    static auto outerCall<U>(U v) {
        Queue q();
        background qpush(q, identity<U>(v));
        return q.get(10s);
    }
}

# the type parameter of a generic class
class Box<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    auto get() {
        Queue q();
        background q.push(identity<T>(v));
        return q.get(10s);
    }
}

sub qpush(Queue q, auto v) { q.push(v); }

# the call that starts the thread has returned when the thread makes its call
sub late<X>(Queue start, Queue q, X v) {
    background late_body<X>(start, q, v);
}
sub late_body<X>(Queue start, Queue q, X v) {
    start.get(10s);
    try {
        q.push(identity<X>(v));
    } catch (hash<ExceptionInfo> ex) {
        q.push(ex.err);
    }
}

# a background expression of generic code started by a background expression of generic code
auto sub nested<X>(X v) {
    Queue q();
    background q.push(inner<list<X>>((v,)));
    return q.get(10s);
}
auto sub inner<Y>(Y v) {
    Queue q();
    background q.push(identity<Y>(v));
    return q.get(10s);
}

# the type argument is checked in the thread
auto sub checked<X>(auto v) {
    Queue q();
    background check_push<X>(q, v);
    return q.get(10s);
}
sub check_push<X>(Queue q, auto v) {
    try {
        q.push(identity<X>(v));
    } catch (hash<ExceptionInfo> ex) {
        q.push(ex.err);
    }
}

# the background call itself has explicit type arguments: a function, a static method, a self method and a method of
# an object
class Forms {
    static checkStatic<X>(Queue q, auto v) { check_push<X>(q, v); }
    checkMethod<X>(Queue q, auto v) { check_push<X>(q, v); }
    auto selfCallInBackground<X>(auto v) {
        Queue q();
        background checkMethod<X>(q, v);
        return q.get(10s);
    }
}
auto sub bg_forms<X>(auto v) {
    list<auto> rv;
    Queue q();
    background check_push<X>(q, v);
    rv += q.get(10s);
    background Forms::checkStatic<X>(q, v);
    rv += q.get(10s);
    Forms f();
    rv += f.selfCallInBackground<X>(v);
    background f.checkMethod<X>(q, v);
    rv += q.get(10s);
    return rv;
}

string sub t(code c) {
    try {
        auto v = c();
        return sprintf("%y %s", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s\n", i,
            t(auto sub () { return G::outer<int>(1); }),
            t(auto sub () { return G::outerCall<string>("2"); }),
            t(auto sub () { Box<int> b(3); return b.get(); }),
            t(auto sub () {
                Queue start();
                Queue q();
                late<string>(start, q, "4");
                start.push(True);
                return q.get(10s);
            }),
            t(auto sub () { return nested<int>(5); }),
            t(auto sub () { return checked<int>(6); }),
            t(auto sub () { return checked<int>("x"); }),
            t(auto sub () { return bg_forms<int>(7); }),
            t(auto sub () { return bg_forms<int>("y"); }));
    }
    return rv;
}
