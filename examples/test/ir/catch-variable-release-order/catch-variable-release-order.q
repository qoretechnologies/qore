%modern
%disable-warning unreferenced-variable
# the variable of a catch block whose statement raises is released when the catch block is left, before the code and
# the handlers of the enclosing blocks run, also when the catch block is in the body of a loop statement whose
# exception path runs code of its own; see catch-variable-release-order.qtest (qore#6472)
# Copyright (C) 2026 Qore Technologies, s.r.o.

list log;

class D {
    public {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        log += "D " + n + " destroyed";
    }
}

sub thrower() {
    throw "B";
}

# the catch block raises: in the body of a foreach over a reference, of a fused foreach, of a foreach over a list
# that can hold objects, of a foreach over scalars, of a context statement
sub ref_foreach() {
    on_error log += "ref foreach: function on_error";
    list m = (1, 2);
    foreach auto x in (\m) {
        try {
            throw "A", "", new D("ref");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

sub fused_foreach() {
    on_error log += "fused foreach: function on_error";
    foreach auto x in (map $1, (1, 2)) {
        try {
            throw "A", "", new D("fused");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

sub object_foreach() {
    on_error log += "object foreach: function on_error";
    foreach auto x in ((new D("element"), new D("element 2"))) {
        try {
            throw "A", "", new D("object");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

sub scalar_foreach() {
    on_error log += "scalar foreach: function on_error";
    foreach auto x in ((1, 2)) {
        try {
            throw "A", "", new D("scalar");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

sub context_statement() {
    on_error log += "context: function on_error";
    context ({"a": (1, 2)}) {
        try {
            throw "A", "", new D("context");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

# an outer catch instead of a handler, nested loops, and a handler of a block in the loop body
sub outer_catch() {
    try {
        list m = (1, 2);
        foreach auto x in (\m) {
            foreach auto y in (map $1, (1, 2)) {
                try {
                    throw "A", "", new D("nested");
                } catch (hash<ExceptionInfo> ex) {
                    thrower();
                }
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "outer catch: " + ex.err;
    }
}

sub block_handler() {
    list m = (1, 2);
    foreach auto x in (\m) {
        {
            on_error log += "block handler: block on_error";
            try {
                throw "A", "", new D("block");
            } catch (hash<ExceptionInfo> ex) {
                thrower();
            }
        }
    }
}

# the catch variable is released once when the catch block ends normally and the loop goes on
sub normal_exit() {
    list m = (1, 2);
    foreach auto x in (\m) {
        try {
            throw "A", "", new D("normal " + x);
        } catch (hash<ExceptionInfo> ex) {
            log += "normal: caught " + x;
        }
    }
    log += "normal: done";
}

# a rethrow from the catch block
sub rethrow_catch() {
    on_error log += "rethrow: function on_error";
    foreach auto x in (map $1, (1, 2)) {
        try {
            throw "A", "", new D("rethrow");
        } catch (hash<ExceptionInfo> ex) {
            rethrow;
        }
    }
}

# the locals of an if statement's condition
sub if_locals() {
    on_error log += "if locals: function on_error";
    if (auto v = 1) {
        try {
            throw "A", "", new D("if");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}

# a catch block in an on_exit handler that runs at a return in the loop body
sub handler_at_return(string kind) {
    on_error log += "handler at return " + kind + ": function on_error";
    on_exit {
        try {
            throw "A", "", new D("handler " + kind);
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
    switch (kind) {
        case "ref": {
            list m = (1, 2);
            foreach auto x in (\m) {
                return;
            }
            break;
        }
        case "fused": {
            foreach auto x in (map $1, (1, 2)) {
                return;
            }
            break;
        }
        case "context": {
            context ({"a": (1, 2)}) {
                return;
            }
            break;
        }
    }
}

# an outer catch block is still open when the inner one raises in a loop body
sub outer_catch_open() {
    on_error log += "outer catch open: function on_error";
    try {
        throw "O", "", new D("outer");
    } catch (hash<ExceptionInfo> outer) {
        foreach auto x in (map $1, (1, 2)) {
            context ({"a": (1,)}) {
                try {
                    throw "A", "", new D("inner");
                } catch (hash<ExceptionInfo> ex) {
                    thrower();
                }
            }
        }
    }
}

foreach code c in ((\ref_foreach(), \fused_foreach(), \object_foreach(), \scalar_foreach(), \context_statement(),
        \outer_catch(), \block_handler(), \normal_exit(), \rethrow_catch(), \if_locals(),
        sub () { handler_at_return("ref"); }, sub () { handler_at_return("fused"); },
        sub () { handler_at_return("context"); }, \outer_catch_open())) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caller: " + ex.err;
    }
}

foreach string line in (log) {
    printf("%s\n", line);
}
