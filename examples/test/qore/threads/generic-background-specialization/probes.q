# probes of generic-background-specialization.qtest (qore#5815): generic code with background expressions of every
# form (a call of a function, of a closure, of a method of an object, of a self method and of a static method, and an
# expression evaluated by a helper), each specialized with two type arguments; the background expressions only use
# values of the starting code, so that the probes do not depend on the type arguments of the starting code in the new
# thread

%modern

sub push_value(Queue q, auto v) { q.push(v); }

# background expressions that call a function, a method of an object and a closure, and one evaluated by a helper
list<auto> sub forms<X>(X v) {
    Queue q();
    background push_value(q, v);
    background q.push(v);
    code c = sub () { q.push(v); };
    background c();
    background q.push(sprintf("%y", v));
    list<auto> rv;
    for (int i = 0; i < 4; ++i) {
        rv += q.get(10s);
    }
    return sort(rv, int sub (auto l, auto r) { return sprintf("%y", l) <=> sprintf("%y", r); });
}

class G {
    static pushStatic(Queue q, auto v) { q.push(v); }

    static list<auto> forms<U>(U v) {
        Queue q();
        background push_value(q, v);
        background q.push(v);
        background G::pushStatic(q, v);
        list<auto> rv;
        for (int i = 0; i < 3; ++i) {
            rv += q.get(10s);
        }
        return rv;
    }
}

class Box<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    private pushSelf(Queue q) { q.push(v); }
    list<auto> forms() {
        Queue q();
        background push_value(q, v);
        background q.push(v);
        background pushSelf(q);
        list<auto> rv;
        for (int i = 0; i < 3; ++i) {
            rv += q.get(10s);
        }
        return rv;
    }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 4; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y\n", i, forms<int>(1), forms<string>("s"), G::forms<int>(2),
            G::forms<string>("t"), (new Box<int>(3)).forms(), (new Box<string>("u")).forms());
    }
    return rv;
}
