%modern

string sub frames(list<auto> cs) {
    list<string> l = ();
    foreach hash<auto> f in (cs) {
        l += sprintf("%s:%s:%s:%d", f.function, f.type, basename(f.file ?? ""), f.line);
    }
    return l.join(" < ");
}

string sub ex(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s @ %s:%d %s", ex.err, ex.desc, basename(ex.file ?? ""), ex.line, frames(ex.callstack));
}

# a builtin base class whose constructor raises an exception
class SB0 inherits StreamBase {
}

class SB1 inherits StreamBase {
    constructor() {
        printf("not reached\n");
    }
}

class SB2 inherits SB1 {
}

# a builtin base class whose destructor raises an exception when the lock is held
class M0 inherits Mutex {
}

class M1 inherits Mutex {
    constructor() {
        printf("M1 frames: %s\n", frames(get_thread_call_stack()));
    }

    destructor() {
        printf("M1 destructor frames: %s\n", frames(get_thread_call_stack()));
    }
}

class S0 inherits Socket {
    public {
        int n;
    }
}

class S1 inherits Socket {
    constructor(int n) {
        printf("S1 %d frames: %s\n", n, frames(get_thread_call_stack()));
    }
}

sub run(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> e) {
        printf("%s: %s\n", label, ex(e));
    }
}

# a closure that returns the value of a method call on its local is left out: the location of the destructor frame of
# the local then differs between the execution modes (qore#7481); the closures below take the value first
for (int i = 0; i < 3; ++i) {
    run("SB0", auto sub () { SB0 o(); });
    run("SB1", auto sub () { SB1 o(); });
    run("SB2", auto sub () { SB2 o(); });
    run("M0 delete locked", auto sub () { M0 m(); m.lock(); delete m; });
    run("M0 scope locked", auto sub () { M0 m(); m.lock(); });
    run("M1", auto sub () { M1 m(); bool b = m.lockOwner(); return b; });
    run("M1 delete locked", auto sub () { M1 m(); m.lock(); delete m; });
    run("S0", auto sub () { S0 s(); return (s.isOpen(), s.n, keys s); });
    run("S1", auto sub () { S1 s(i); return s.isOpen(); });
    run("copy", auto sub () { S0 s(); S0 c = s.copy(); return c.isOpen(); });
    run("Socket", auto sub () { Socket s(); return s.isOpen(); });
    run("Mutex", auto sub () { Mutex m(); m.lock(); bool b = m.lockOwner(); return b; });
    run("deleted", auto sub () { M0 m(); delete m; return m.lockOwner(); });
}
