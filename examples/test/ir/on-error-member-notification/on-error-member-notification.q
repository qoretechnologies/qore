%modern
# a store into a member or key of a local raises an exception in a block with on_exit, on_error or on_success
# handlers: the handlers and the catch block see the local's value; see on-error-member-notification.qtest, which runs
# this script in every execution mode, on the IR tier and as an AOT executable, and checks that it gives the output of
# the AST interpreter (qore#5950)

#! an object whose member notification rejects a value
class Notified {
    public {
        int d = 1;
    }

    memberNotification(string member) {
        if (d == 13) {
            throw "NOTIFY-ERROR";
        }
    }
}

#! a typed hash for key stores that raise an exception
hashdecl Rec {
    int a = 1;
}

#! an on_error handler in the try block (the issue's case)
string sub on_error_in_try() {
    list<string> log;
    object g = new Notified();
    try {
        on_error log += "error";
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, g.d);
    }
    return log.join("; ");
}

#! an on_exit handler in the try block reads the object
string sub on_exit_in_try() {
    list<string> log;
    object g = new Notified();
    try {
        on_exit log += sprintf("exit d=%d", g.d);
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, g.d);
    }
    return log.join("; ");
}

#! an on_success handler in the try block, which does not run
string sub on_success_in_try() {
    list<string> log;
    object g = new Notified();
    try {
        on_success log += sprintf("success d=%d", g.d);
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, g.d);
    }
    return log.join("; ");
}

#! all three handlers in the try block read the object, and the object is used after the try statement
string sub all_handlers() {
    list<string> log;
    object g = new Notified();
    try {
        on_exit log += sprintf("exit d=%d", g.d);
        on_error log += sprintf("error d=%d %s", g.d, $1.err);
        on_success log += sprintf("success d=%d", g.d);
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, g.d);
    }
    g.d = 2;
    log += sprintf("after d=%d", g.d);
    return log.join("; ");
}

#! the handlers are in a block nested in the try block, and in an inner try statement whose catch block rethrows
string sub nested_try() {
    list<string> log;
    object g = new Notified();
    try {
        {
            on_error log += sprintf("block error d=%d", g.d);
            try {
                on_exit log += sprintf("inner exit d=%d", g.d);
                g.d = 13;
            } catch (hash<ExceptionInfo> ex) {
                log += sprintf("inner caught %s d=%d", ex.err, g.d);
                rethrow;
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("outer caught %s d=%d", ex.err, g.d);
    }
    return log.join("; ");
}

#! the object local has a class type, and the other locals of the function keep their values
string sub typed_locals() {
    list<string> log;
    Notified g = new Notified();
    int i = 7;
    float f = 1.5;
    string s = "str";
    hash<auto> h = {"k": "v"};
    list<int> l = (1, 2);
    try {
        on_error log += sprintf("error i=%d f=%n s=%s h=%y l=%y d=%d", i, f, s, h, l, g.d);
        ++i;
        s += "!";
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s i=%d f=%n s=%s h=%y l=%y d=%d", ex.err, i, f, s, h, l, g.d);
    }
    return log.join("; ");
}

#! the object local is untyped and the member is given by a variable (a dynamic key)
string sub dynamic_key() {
    list<string> log;
    auto g = new Notified();
    string key = "d";
    try {
        on_error log += sprintf("error d=%d", g{key});
        g{key} = 13;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, g{key});
    }
    return log.join("; ");
}

#! a key store into a typed hash with a key that its hashdecl does not have raises an exception
string sub hashdecl_key() {
    list<string> log;
    auto h = <Rec>{"a": 2};
    string key = "bad";
    try {
        on_error log += sprintf("error h=%y", h);
        h{key} = 3;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s h=%y", ex.err, h);
    }
    h.a = 4;
    log += sprintf("after h=%y", h);
    return log.join("; ");
}

#! an element store into a typed list raises a type error
string sub list_element_store() {
    list<string> log;
    list<int> l = (1, 2);
    auto v = "x";
    try {
        on_error log += sprintf("error l=%y", l);
        l[0] = v;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s l=%y", ex.err, l);
    }
    l[1] = 3;
    log += sprintf("after l=%y", l);
    return log.join("; ");
}

#! a key store into a typed hash raises a type error
string sub hash_key_store() {
    list<string> log;
    hash<string, int> h = {"a": 1};
    auto v = "x";
    try {
        on_exit log += sprintf("exit h=%y", h);
        h.b = v;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s h=%y", ex.err, h);
    }
    return log.join("; ");
}

#! a closure-captured local and an on_exit handler of the function, which runs after the catch block, and the
#! on_error handler in the try block
sub captured_locals(reference<list<string>> log) {
    object g = new Notified();
    int n = 0;
    code inc = sub () { ++n; };
    on_exit log += sprintf("exit d=%d n=%d", g.d, n);
    try {
        on_error inc();
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        inc();
        log += sprintf("caught %s d=%d n=%d", ex.err, g.d, n);
    }
}

#! an assignment of a value of the wrong type to a typed local is rejected; the local keeps its value
string sub rejected_assignment() {
    list<string> log;
    list<int> l = (1, 2);
    object o = new Notified();
    auto v = "x";
    try {
        on_error log += sprintf("error l=%y", l);
        l = v;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s l=%y", ex.err, l);
    }
    try {
        on_error log += sprintf("error d=%d", o.d);
        o = v;
    } catch (hash<ExceptionInfo> ex) {
        log += sprintf("caught %s d=%d", ex.err, o.d);
    }
    return log.join("; ");
}

#! the exception leaves the function, whose on_error handler reads the object
string sub function_error_handler(reference<list<string>> log) {
    object g = new Notified();
    on_error log += sprintf("function error d=%d", g.d);
    g.d = 13;
    return "not raised";
}

for (int i = 0; i < 3; ++i) {
    printf("on_error in try: %s\n", on_error_in_try());
    printf("on_exit in try: %s\n", on_exit_in_try());
    printf("on_success in try: %s\n", on_success_in_try());
    printf("all handlers: %s\n", all_handlers());
    printf("nested try: %s\n", nested_try());
    printf("typed locals: %s\n", typed_locals());
    printf("dynamic key: %s\n", dynamic_key());
    printf("hashdecl key: %s\n", hashdecl_key());
    printf("list element store: %s\n", list_element_store());
    printf("hash key store: %s\n", hash_key_store());
    {
        list<string> log;
        captured_locals(\log);
        printf("captured locals: %s\n", log.join("; "));
    }
    printf("rejected assignment: %s\n", rejected_assignment());
    {
        list<string> log;
        try {
            function_error_handler(\log);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught " + ex.err;
        }
        printf("function error handler: %s\n", log.join("; "));
    }
}
