#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Worker for catch-scope-rethrow.qtest: runs each case and prints one "<case>: <outcome>" line per case.  Every case
# ends with a rethrow in a catch block after code that caught other exceptions while that catch block was running; the
# rethrow must raise the catch block's own exception.  The qtest runs this script in every execution mode and as a
# qcc-compiled executable and compares every line with the expected outcome.

%modern

string sub chain(hash<ExceptionInfo> ex) {
    string rv = sprintf("%s:%s", ex.err, ex.desc);
    *hash<ExceptionInfo> n = ex.next;
    while (n) {
        rv += sprintf(" -> %s:%s", n.err, n.desc);
        n = n.next;
    }
    return rv;
}

# each case runs three times: a catch scope popped too early leaves the catch context of the thread pointing at a
# deleted exception, which a later rethrow reads
sub run(string name, code c) {
    list<string> res = ();
    for (int i = 0; i < 3; ++i) {
        try {
            auto rv = c();
            push res, sprintf("returned %y", rv);
        } catch (hash<ExceptionInfo> ex) {
            push res, "caught " + chain(ex);
        }
    }
    # one result when the runs agree, otherwise all of them
    printf("%s: %s\n", name, (select res, $1 != res[0]) ? (foldl $1 + " | " + $2, res) : res[0]);
    flush();
}

# an on_error handler that rethrows the exception with new arguments, as a REST client redacting an error does
sub on_error_rethrow_args() {
    on_error {
        rethrow $1.err + "-R", $1.desc;
    }
    throw "INNER", "inner";
}

# an on_error handler with a plain rethrow
sub on_error_rethrow_plain() {
    on_error {
        rethrow;
    }
    throw "INNER", "inner";
}

# a callee that catches what its own callee's on_error handler rethrew and returns from its catch block
hash<auto> sub catch_and_return(code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        return {"failure": ex};
    }
    return {};
}

# a callee that throws in a try statement nested in its catch block and returns from the catch block
hash<auto> sub nested_throw_and_return() {
    try {
        throw "CALLEE", "callee";
    } catch (hash<ExceptionInfo> ex) {
        try {
            throw "CALLEE-NESTED", "callee nested";
        } catch (hash<ExceptionInfo> ex2) {
        }
        return {"failure": ex};
    }
}

# the outer catch block calls code, then rethrows its own exception
auto sub outer(code c) {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        auto rv = c();
        rethrow;
    }
}

# the outer catch block calls code that raises an exception, catches it, then rethrows its own exception
auto sub outer_catching(code c) {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        try {
            c();
        } catch (hash<ExceptionInfo> ex2) {
        }
        rethrow;
    }
}

# a rethrow in a try statement nested in a catch block after a throw in another one
string sub nested_throw_then_nested_rethrow() {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        try {
            throw "NESTED", "nested";
        } catch (hash<ExceptionInfo> ex2) {
        }
        try {
            rethrow;
        } catch (hash<ExceptionInfo> ex3) {
            return ex3.err;
        }
    }
    return "none";
}

# a callee that returns the exception of a catch block in which it caught another one; then a rethrow in a nested try
string sub callee_nested_throw_then_nested_rethrow() {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        hash<auto> h = nested_throw_and_return();
        try {
            rethrow;
        } catch (hash<ExceptionInfo> ex3) {
            return ex3.err + "/" + h.failure.err;
        }
    }
    return "none";
}

run("on_error_rethrow_args", sub () { outer_catching(\on_error_rethrow_args()); });
run("on_error_rethrow_plain", sub () { outer_catching(\on_error_rethrow_plain()); });
run("on_error_rethrow_args_returned", sub () { outer(hash<auto> sub () { return catch_and_return(\on_error_rethrow_args()); }); });
run("on_error_rethrow_args_value", string sub () {
    hash<auto> h = catch_and_return(\on_error_rethrow_args());
    return h.failure.err;
});
run("callee_nested_throw", sub () { outer(\nested_throw_and_return()); });
run("callee_nested_throw_value", string sub () { return nested_throw_and_return().failure.err; });
run("nested_throw", auto sub () {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        try {
            throw "NESTED", "nested";
        } catch (hash<ExceptionInfo> ex2) {
        }
        rethrow;
    }
});
run("nested_rethrow", auto sub () {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        string inner;
        try {
            rethrow;
        } catch (hash<ExceptionInfo> ex2) {
            inner = ex2.err;
        }
        rethrow ex.err + "-" + inner, ex.desc;
    }
});
run("nested_rethrow_args", auto sub () {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        try {
            rethrow "NESTED", "nested";
        } catch (hash<ExceptionInfo> ex2) {
        }
        rethrow;
    }
});
run("nested_throw_in_loop", auto sub () {
    try {
        throw "OUTER", "outer";
    } catch (hash<ExceptionInfo> ex) {
        for (int i = 0; i < 2; ++i) {
            try {
                throw "NESTED", "nested";
            } catch (hash<ExceptionInfo> ex2) {
                continue;
            }
        }
        rethrow;
    }
});
run("throw_from_catch", auto sub () {
    try {
        try {
            throw "INNER", "inner";
        } catch (hash<ExceptionInfo> ex) {
            throw "FROM-CATCH", "from catch";
        }
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
});
run("nested_throw_then_nested_rethrow", \nested_throw_then_nested_rethrow());
run("callee_nested_throw_then_nested_rethrow", \callee_nested_throw_then_nested_rethrow());
