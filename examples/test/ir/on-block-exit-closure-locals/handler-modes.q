%modern
# the cases of handler-locals.q that AOT cannot run yet, which are checked with the execution modes only: a local that
# the on_error handler of a closure declares and references (qcc cannot compile it: qore#5657), and a loop in a handler
# (AOT gives a wrong result: qore#5658); see on-block-exit-closure-locals.qtest

#! a closure whose on_error handler declares a local that it references
string sub closure_ref_error() {
    list<string> log;
    code c = sub () {
        on_error {
            int i = 1;
            reference<int> r = \i;
            r += 10;
            log += sprintf("error i=%d", i);
        }
        throw "Y";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

#! a loop in an on_exit handler whose body declares a captured local: each closure captures the value of its
#! iteration
string sub loop_in_handler() {
    list<string> log;
    code c = sub () {
        on_exit {
            list<code> cl;
            foreach int i in (xrange(3)) {
                int v = i * 10;
                cl += int sub () { return v; };
            }
            log += sprintf("loop %y", (map $1(), cl));
        }
        throw "L";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

sub main() {
    # runs more than once so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("closure ref error: %s\n", closure_ref_error());
        printf("loop in handler: %s\n", loop_in_handler());
    }
}

main();
