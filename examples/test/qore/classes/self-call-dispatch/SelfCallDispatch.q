%modern
# unqualified calls of methods of self (m()) on objects of the class of the call, of the method's class and of
# subclasses, which must dispatch as the AST interpreter does (issue #7267); its output is SelfCallDispatch.expected

class Base {
    public {
        list<string> log;
    }

    constructor() {
        push log, "constructor: " + who();
    }

    destructor() {
        printf("destructor of %s: %s\n", self.className(), who());
    }

    string who() { return "Base::who"; }
    string run(int i) { return m(i); }
    string m(int i) { return sprintf("Base::m(%d)", i); }

    # a private method, which a subclass can override
    string runPrivate(int i) { return p(i); }
    private string p(int i) { return sprintf("Base::p(%d)", i); }

    # a private:internal method, which a subclass cannot override
    string runInternal(int i) { return pi(i); }
    private:internal string pi(int i) { return sprintf("Base::pi(%d)", i); }

    # a static method, which is not virtual
    string runStatic(int i) { return st(i); }
    static string st(int i) { return sprintf("Base::st(%d)", i); }

    # default, named, variable and reference arguments
    string runDefault() { return def(1) + " " + def(1, 2); }
    string def(int a, int b = 10) { return sprintf("Base::def(%d, %d)", a, b); }
    string runNamed() { return def(b: 3, a: 4); }
    string runVarargs() { return va(1, 2, 3) + " " + va(4); }
    string va(int a, ...) { return sprintf("Base::va(%d, %y)", a, argv); }
    int runReference(int i) { int v = i; incr(\v); incr(\v); return v; }
    incr(reference<int> r) { r += 1; }

    # exceptions
    string runThrow(int i) {
        try {
            thrower(i);
        } catch (hash<ExceptionInfo> ex) {
            return sprintf("caught %s: %s", ex.err, ex.desc);
        }
        return "no exception";
    }
    thrower(int i) { throw "BASE-ERROR", sprintf("Base::thrower(%d)", i); }
    # an exception that leaves the calling method
    string runThrowOut(int i) { thrower(i); return "not reached"; }

    # recursion
    int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
    int depth(int n) { return n ? depth(n - 1) + step() : 0; }
    int step() { return 1; }

    # a closure created in a method
    string runClosure(int i) {
        code c = string sub (int j) { return m(j); };
        return c(i) + " " + c(i + 1);
    }

    # a call with a call as argument, and a call in a loop
    string runNested(int i) { return m(len(m(i))); }
    int len(string s) { return s.size(); }
    string runLoop(int n) {
        list<string> l;
        for (int i = 0; i < n; ++i) {
            push l, m(i);
        }
        return l.join(", ");
    }
}

class Derived inherits Base {
    string who() { return "Derived::who"; }
    string m(int i) { return sprintf("Derived::m(%d)", i); }
    private string p(int i) { return sprintf("Derived::p(%d)", i); }
    private:internal string pi(int i) { return sprintf("Derived::pi(%d)", i); }
    static string st(int i) { return sprintf("Derived::st(%d)", i); }
    string def(int a, int b = 20) { return sprintf("Derived::def(%d, %d)", a, b); }
    string va(int a, ...) { return sprintf("Derived::va(%d, %y)", a, argv); }
    incr(reference<int> r) { r += 100; }
    thrower(int i) { throw "DERIVED-ERROR", sprintf("Derived::thrower(%d)", i); }
    int step() { return 10; }
    int len(string s) { return 1; }
}

# a class that inherits the methods without overriding them: the object's class is neither the class of the call
# nor the method's class
class Inherits inherits Base {
    string runOwn(int i) { return m(i) + " " + who(); }
}

# overrides a method of a class that inherited it
class InheritsOverride inherits Inherits {
    string m(int i) { return sprintf("InheritsOverride::m(%d)", i); }
}

# a diamond: the code of B2 calls its own public method, not the common parent's private method (qore#6798)
class P2 {
    private string dm() { return "P2::dm"; }
    string viaP2() { return dm(); }
}
class A2 inherits P2 {}
class B2 inherits P2 {
    string dm() { return "B2::dm"; }
    string viaB2() { return dm(); }
}
class X2 inherits A2, B2 {}
class Y2 inherits B2 {
    string dm() { return "Y2::dm"; }
}

# abstract methods (issue #3070)
class AbstractBase {
    abstract string am(int i);
    string runAbstract(int i) { return am(i); }
}
class Impl1 inherits AbstractBase {
    string am(int i) { return sprintf("Impl1::am(%d)", i); }
}
class Impl2 inherits Impl1 {
    string am(int i) { return sprintf("Impl2::am(%d)", i); }
}

# a final class, whose calls are bound at parse time
final class FinalClass inherits Base {
    string m(int i) { return sprintf("FinalClass::m(%d)", i); }
    string runFinal(int i) { return m(i); }
}

# the code of the child Program
const ChildCode = "class ChildBase {
    string run(int i) { return m(i) + \" \" + p(i); }
    string m(int i) { return sprintf(\"ChildBase::m(%d)\", i); }
    private string p(int i) { return sprintf(\"ChildBase::p(%d)\", i); }
    int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
}
class ChildDerived inherits ChildBase {
    string m(int i) { return sprintf(\"ChildDerived::m(%d)\", i); }
    private string p(int i) { return sprintf(\"ChildDerived::p(%d)\", i); }
}
foreach ChildBase o in ((new ChildBase(), new ChildDerived(), new ChildBase())) {
    printf(\"child %s run: %y\\n\", o.className(), o.run(1));
    printf(\"child %s fib: %y\\n\", o.className(), o.fib(12));
}
";

sub show(string label, auto v) {
    printf("%s: %y\n", label, v);
}

sub run_object(Base o) {
    string c = o.className();
    show(c + " run", o.run(1));
    show(c + " runPrivate", o.runPrivate(2));
    show(c + " runInternal", o.runInternal(3));
    show(c + " runStatic", o.runStatic(4));
    show(c + " runDefault", o.runDefault());
    show(c + " runNamed", o.runNamed());
    show(c + " runVarargs", o.runVarargs());
    show(c + " runReference", o.runReference(5));
    show(c + " runThrow", o.runThrow(6));
    try {
        o.runThrowOut(7);
    } catch (hash<ExceptionInfo> ex) {
        show(c + " runThrowOut", ex.err + ": " + ex.desc);
    }
    show(c + " fib", o.fib(10));
    show(c + " depth", o.depth(5));
    show(c + " runClosure", o.runClosure(8));
    show(c + " runNested", o.runNested(9));
    show(c + " runLoop", o.runLoop(3));
    show(c + " log", o.log);
}

# the same call sites with objects of different classes, alternating
foreach Base o in ((new Base(), new Derived(), new Base(), new Inherits(), new InheritsOverride(), new Derived(),
        new FinalClass())) {
    run_object(o);
}

{
    Inherits i();
    show("Inherits runOwn", i.runOwn(1));
    InheritsOverride io();
    show("InheritsOverride runOwn", io.runOwn(2));
    show("Inherits runOwn again", i.runOwn(3));
}

{
    FinalClass f();
    show("FinalClass runFinal", f.runFinal(1));
}

{
    X2 x();
    B2 b();
    Y2 y();
    show("X2 viaB2", x.viaB2());
    show("X2 viaP2", x.viaP2());
    show("B2 viaB2", b.viaB2());
    show("B2 viaP2", b.viaP2());
    show("Y2 viaB2", y.viaB2());
    show("Y2 viaP2", y.viaP2());
    show("X2 viaB2 again", x.viaB2());
}

{
    Impl1 i1();
    Impl2 i2();
    show("Impl1 runAbstract", i1.runAbstract(1));
    show("Impl2 runAbstract", i2.runAbstract(2));
    show("Impl1 runAbstract again", i1.runAbstract(3));
}

# the same classes in a child Program, with a subclass of its own
{
    Program p(PO_NEW_STYLE);
    p.parse(ChildCode, "child");
    p.run();
}
