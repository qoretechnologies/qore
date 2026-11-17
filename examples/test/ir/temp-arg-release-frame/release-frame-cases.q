%modern
# the cases of temp-arg-release-frame.qtest: the call stack when the temporary argument of a call is released, which
# every execution mode must give as the AST interpreter does: the callee has taken the argument over, and its frame is
# still on the call stack when it releases it, after its own local variables (qore#5882)

# Copyright (C) 2026 Qore Technologies, s.r.o.

class Log {
    public {
        list<string> lines;
    }

    add(string line) {
        lines += line;
    }
}

our Log log();

#! returns the functions of the call stack \a cs up to the frame of the case (\c case_*)
list<string> sub frames(list<hash<CallStackInfo>> cs) {
    list<string> rv;
    foreach hash<CallStackInfo> h in (cs) {
        rv += h.function;
        if (h.function =~ /^case_/) {
            break;
        }
    }
    return rv;
}

#! logs its release with the call stack at that time, without the frame of get_thread_call_stack()
class Rec {
    public {
        string n;
    }

    constructor(string nn) {
        n = nn;
    }

    destructor() {
        log.add(sprintf("rel %s %y", n, frames(get_thread_call_stack()[1..])));
    }
}

#! raises an exception when it is released
class Dtor {
    destructor() {
        throw "DTOR";
    }
}

sub nocall(auto a) {
}

sub two(auto a, auto b) {
}

sub typed(Rec r) {
}

sub va(...) {
}

#! takes an argument more than it declares, which the call gives in the implicit argument list
sub argv_user(auto a) {
    log.add("argv " + argv.size());
}

sub withlocal(auto a) {
    on_exit {
        log.add("on_exit");
    }
    Rec l("local");
    log.add("body");
}

auto sub ret(auto a) {
    return a.n;
}

sub rec(int n, auto a) {
    if (n) {
        rec(n - 1, new Rec("r" + n));
    }
}

class C {
    m(auto a) {
    }

    static sm(auto a) {
    }

    margv(auto a) {
        log.add("argv " + argv.size());
    }

    static smargv(auto a) {
        log.add("argv " + argv.size());
    }

    mlocal(auto a) {
        on_exit {
            log.add("on_exit");
        }
        Rec l("local");
        log.add("body");
    }
}

class K {
    constructor(auto a) {
    }
}

#! a method called by its class's own code without an object (an implicit self call)
class S {
    m(auto a) {
    }

    callSelf() {
        m(new Rec("a"));
    }

    throwSelf() {
        m(new Dtor());
    }
}

#! a method that a subclass overrides, called through a receiver of the base class's type
class B {
    v(auto a) {
    }
}

class D inherits B {
    v(auto a) {
    }
}

#! an abstract method, called through a receiver of the abstract class's type
class AbsB {
    abstract am(auto a);
}

class AbsD inherits AbsB {
    am(auto a) {
    }
}

#! a closure stored in a member, called as a method of the object
class M {
    public {
        code f = sub (auto a) {};
    }
}

#! callees that write the parameter that takes their argument over: the argument is released at the write (#5958),
#! with positional and with named arguments (qore#6486)
class W {
    mw(auto a) {
        a = NOTHING;
        log.add("after write");
    }

    static smw(auto a) {
        a = NOTHING;
        log.add("after write");
    }
}

sub fw(auto a) {
    a = NOTHING;
    log.add("after write");
}

sub catch_log(hash<ExceptionInfo> ex) {
    log.add(sprintf("catch %s %y", ex.err, frames(ex.callstack)));
}

C c();
code cl = sub (auto a) {};
code clv = sub (...) {};
code cr = \nocall();
code mr = \c.m();
code smr = \C::sm();

sub case_function() { nocall(new Rec("a")); }
sub case_function_two() { two(new Rec("a"), new Rec("b")); }
sub case_function_typed() { typed(new Rec("a")); }
sub case_function_varargs() { va(new Rec("a"), new Rec("b")); }
sub case_function_argv() { argv_user(new Rec("a"), new Rec("b")); }
sub case_method_argv() { c.margv(new Rec("a"), new Rec("b")); }
sub case_static_method_argv() { C::smargv(new Rec("a"), new Rec("b")); }
sub case_function_locals() { withlocal(new Rec("a")); }
sub case_function_return() { ret(new Rec("a")); }
sub case_function_recursive() { rec(2, new Rec("r")); }
sub case_method() { c.m(new Rec("a")); }
sub case_method_locals() { c.mlocal(new Rec("a")); }
sub case_static_method() { C::sm(new Rec("a")); }
sub case_closure() { cl(new Rec("a")); cl(new Rec("b")); }
sub case_closure_varargs() { clv(new Rec("a")); clv(new Rec("b")); }
sub case_call_reference() { cr(new Rec("a")); cr(new Rec("b")); }
sub case_method_call_reference() { mr(new Rec("a")); mr(new Rec("b")); }
sub case_static_call_reference() { smr(new Rec("a")); smr(new Rec("b")); }
sub case_constructor() { new K(new Rec("a")); }
sub case_loop() { for (int i = 0; i < 2; ++i) { nocall(new Rec("a" + i)); } }
# the call kinds of qore#6063
sub case_self_call() { S s(); s.callSelf(); }
sub case_virtual() { B b = new D(); b.v(new Rec("a")); }
sub case_abstract() { AbsB a = new AbsD(); a.am(new Rec("a")); }
sub case_auto_receiver() { auto o = new C(); o.m(new Rec("a")); }
sub case_object_receiver() { object o = new C(); o.m(new Rec("a")); }
sub case_polymorphic_loop() { foreach B o in ((new B(), new D(), new B(), new D())) { o.v(new Rec("a" + $#)); } }
sub case_local_closure() { code f = sub (auto a) {}; f(new Rec("a")); f(new Rec("b")); }
sub case_immediate_closure() { (sub (auto a) {})(new Rec("a")); }
sub case_member_closure() { M mm(); mm.f(new Rec("a")); }
# a parameter written by the callee (qore#6486)
sub case_method_write() { W w(); w.mw(new Rec("a")); }
sub case_named_method_write() { W w(); w.mw(a: new Rec("a")); }
sub case_static_method_write() { W::smw(new Rec("a")); }
sub case_named_static_method_write() { W::smw(a: new Rec("a")); }
sub case_function_write() { fw(new Rec("a")); }
sub case_named_function_write() { fw(a: new Rec("a")); }

sub case_try_function() { try { nocall(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_method() { try { c.m(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_static_method() { try { C::sm(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_closure() { try { cl(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_closure_varargs() { try { clv(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_call_reference() { try { cr(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_method_call_reference() { try { mr(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_static_call_reference() { try { smr(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_constructor() { try { new K(new Dtor()); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }
sub case_try_locals() { try { withlocal(new Rec("a")); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } }

sub case_throw_function() { nocall(new Dtor()); }
sub case_throw_method() { c.m(new Dtor()); }
sub case_throw_static_method() { C::sm(new Dtor()); }
sub case_throw_closure() { cl(new Dtor()); }
sub case_throw_call_reference() { cr(new Dtor()); }
sub case_throw_method_call_reference() { mr(new Dtor()); }
sub case_throw_static_call_reference() { smr(new Dtor()); }
sub case_throw_constructor() { new K(new Dtor()); }
sub case_throw_self_call() { S s(); s.throwSelf(); }
sub case_throw_virtual() { B b = new D(); b.v(new Dtor()); }
sub case_throw_local_closure() { code f = sub (auto a) {}; f(new Dtor()); }
sub case_throw_member_closure() { M mm(); mm.f(new Dtor()); }

sub begin() {
    log.lines = ();
}

sub end(string name) {
    printf("%s: %s\n", name, log.lines.join("; "));
}

# the cases are called directly, so that each call is made by the code of the case, compiled in its execution mode
sub main() {
    begin(); try { case_function(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("function");
    begin(); try { case_function_two(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("function two");
    begin(); try { case_function_typed(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("function typed");
    begin(); try { case_function_varargs(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("function varargs");
    begin(); try { case_function_argv(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("function argv");
    begin(); try { case_method_argv(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("method argv");
    begin(); try { case_static_method_argv(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("static method argv");
    begin(); try { case_function_locals(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("function locals");
    begin(); try { case_function_return(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("function return");
    begin(); try { case_function_recursive(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("function recursive");
    begin(); try { case_method(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("method");
    begin(); try { case_method_locals(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("method locals");
    begin(); try { case_static_method(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("static method");
    begin(); try { case_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("closure");
    begin(); try { case_closure_varargs(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("closure varargs");
    begin(); try { case_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("call reference");
    begin(); try { case_method_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("method call reference");
    begin(); try { case_static_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("static call reference");
    begin(); try { case_constructor(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("constructor");
    begin(); try { case_loop(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("loop");
    begin(); try { case_self_call(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("self call");
    begin(); try { case_virtual(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("virtual");
    begin(); try { case_abstract(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("abstract");
    begin(); try { case_auto_receiver(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("auto receiver");
    begin(); try { case_object_receiver(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("object receiver");
    begin(); try { case_polymorphic_loop(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("polymorphic loop");
    begin(); try { case_local_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("local closure");
    begin(); try { case_immediate_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("immediate closure");
    begin(); try { case_member_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("member closure");
    begin(); try { case_method_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("method write");
    begin(); try { case_named_method_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("named method write");
    begin(); try { case_static_method_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("static method write");
    begin(); try { case_named_static_method_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("named static method write");
    begin(); try { case_function_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("function write");
    begin(); try { case_named_function_write(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("named function write");
    begin(); case_try_function(); end("try function");
    begin(); case_try_method(); end("try method");
    begin(); case_try_static_method(); end("try static method");
    begin(); case_try_closure(); end("try closure");
    begin(); case_try_closure_varargs(); end("try closure varargs");
    begin(); case_try_call_reference(); end("try call reference");
    begin(); case_try_method_call_reference(); end("try method call reference");
    begin(); case_try_static_call_reference(); end("try static call reference");
    begin(); case_try_constructor(); end("try constructor");
    begin(); case_try_locals(); end("try locals");
    begin(); try { case_throw_function(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("throw function");
    begin(); try { case_throw_method(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("throw method");
    begin(); try { case_throw_static_method(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw static method");
    begin(); try { case_throw_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("throw closure");
    begin(); try { case_throw_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw call reference");
    begin(); try { case_throw_method_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw method call reference");
    begin(); try { case_throw_static_call_reference(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw static call reference");
    begin(); try { case_throw_constructor(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw constructor");
    begin(); try { case_throw_self_call(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw self call");
    begin(); try { case_throw_virtual(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); } end("throw virtual");
    begin(); try { case_throw_local_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw local closure");
    begin(); try { case_throw_member_closure(); } catch (hash<ExceptionInfo> ex) { catch_log(ex); }
    end("throw member closure");
}

main();
