%modern

# the paths that put a value that needs a scan for recursive references (an object, a closure that captured a
# variable) into a list in place; the scan count of the list must count it (qore#7159): a debug build checks it when
# the value is taken out of the list again, and the list that reverse() returns is scanned for the cycle that it
# closes

class Obj {
    public {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }
}

class Holder {
    public {
        list<auto> l;
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        printf("%s: destroyed\n", name);
    }
}

class Caller {
    m(Obj o, int x) {
        printf("%s: %d\n", o.name, x);
    }
}

sub callWith(code c, int x) {
    c(x);
}

sub withObj(Obj o, int x) {
    printf("%s: %d\n", o.name, x);
}

sub withDefault(*int a, Obj o = new Obj("default object"), int x = 1) {
    printf("%s: %d\n", o.name, x);
}

sub withClosureDefault(*int a, code c = sub (int v) { printf("default closure: %d\n", v); }, int x = 1) {
    c(x);
}

int x = 1;

# an argument that needs evaluation makes the call evaluate its arguments into a list of its own, into which the
# other arguments are moved
withObj(new Obj("direct object"), x + 1);
{
    string label = "direct closure";
    callWith(sub (int v) { printf("%s: %d\n", label, v); }, x + 1);
}

# the copy of a background call holds the values of the local variables, and evaluates its arguments in the new thread
{
    Counter done(1);
    code c = sub (int v) { printf("background closure: %d\n", v); done.dec(); };
    background callWith(c, x + 1);
    done.waitForZero();
}
{
    Counter done(1);
    Obj o("background object");
    background sub (Obj o, int v) { withObj(o, v); done.dec(); }(o, x + 1);
    done.waitForZero();
}

# the arguments of a named call bound at parse time are evaluated into the positions of their parameters
{
    Counter done(1);
    code c = sub (int v) { printf("background named: %d\n", v); done.dec(); };
    background callWith(x: x + 1, c: c);
    done.waitForZero();
}
withObj(x: x + 1, o: new Obj("named object"));

# a default value that is an object or a closure replaces the argument NOTHING in the list
withDefault(NOTHING, NOTHING, x + 1);
withClosureDefault(NOTHING, NOTHING, x + 1);

# the arguments of a named call of a call reference are bound to the parameters at run time
{
    code ref = \withObj();
    ref(x: x + 1, o: new Obj("runtime named object"));
}

# a named call of a method; its native code in a qcc executable can make it as a dynamic call, which binds the named
# arguments at run time
{
    Caller caller();
    caller.m(x: x + 1, o: new Obj("runtime named method"));
}

# the list that reverse() returns holds the same values as the list reversed: the cycle that it closes is found
{
    Holder h("reverse");
    h.l = reverse((h, 1));
}
print("end\n");
