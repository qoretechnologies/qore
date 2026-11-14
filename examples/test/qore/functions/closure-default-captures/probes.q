# probes of closure-default-captures.qtest (qore#5923, qore#5924): the default value of a closure's parameter is
# evaluated, when the closure is called, with the variables that the closure captured: a closure (or a local variable)
# of the code that created the closure that it uses is captured by the closure, also when the closure is called after
# that code has left the variable's scope (which crashed: #5923), and a plain local variable as a default value works
# in every mode (IR/JIT raised LVALUE-ERROR: #5924): a block that is left, loop iterations, a closure returned from a
# method, a background thread, a default that is a variable changed before the call, and a later local with the same
# name; probes() returns one line per pass, so that the code runs on every tier with tiered execution

%modern
%disable-warning duplicate-local-vars

auto sub escaped() {
    code f;
    {
        int n = 2;
        f = auto sub (code c = auto sub () { return n; }) { return c(); };
    }
    return f();
}

list<auto> sub loop() {
    list<code> l = ();
    for (int i = 0; i < 3; ++i) {
        int n = i * 10;
        l += auto sub (code c = auto sub () { return n; }) { return c(); };
    }
    return map $1(), l;
}

class Maker {
    public {}
    code make(int m) {
        int n = m + 1;
        return auto sub (code c = auto sub () { return n * 2; }) { return c(); };
    }
}

auto sub in_background() {
    Queue q();
    code f;
    {
        int n = 30;
        f = auto sub (code c = auto sub () { return n; }) { return c(); };
    }
    background q.push(f());
    return q.get();
}

auto sub plain_local() {
    int n = 7;
    code f = auto sub (int x = n) { return x; };
    int r = f();
    n = 8;
    return sprintf("%d %d %d", r, f(), f(5));
}

auto sub shadowed() {
    int n = 1;
    code f = auto sub (code c = auto sub () { return n; }) { return c(); };
    {
        int n = 4;
        n++;
        return f();
    }
}

string sub probes() {
    Maker maker();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y\n", i, escaped(), loop(), maker.make(i)() - i * 2, in_background(),
            plain_local(), shadowed());
    }
    return rv;
}
