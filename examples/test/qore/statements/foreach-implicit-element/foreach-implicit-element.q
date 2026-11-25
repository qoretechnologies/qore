%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a foreach statement sets the thread's implicit index ($#) for each element only if its body reads it (qore#7270):
# every way that a body can read it, loops whose bodies do not, and the index of the enclosing loop or operator after
# a loop that left by break, continue, return, an exception or a rethrow; see foreach-implicit-element.qtest

%disable-warning unreferenced-variable

#! an iterator class of its own: foreach calls next() and getValue()
class Iter inherits AbstractIterator {
    private {
        list<auto> l;
        int i = -1;
    }

    constructor(list<auto> l) {
        self.l = l;
    }

    bool next() {
        return ++i < l.size();
    }

    auto getValue() {
        return l[i];
    }

    bool valid() {
        return i >= 0 && i < l.size();
    }
}

string sub fmt(auto v) {
    return sprintf("%y", v);
}

# the body reads no $#: the sources of every kind
int sub sum_list(list<int> l) {
    int t = 0;
    foreach int v in (l) {
        t += v;
    }
    return t;
}

string sub keys_of(hash<auto> h) {
    string s = "";
    foreach string k in (keys h) {
        s += k;
    }
    return s;
}

string sub pairs_of(hash<auto> h) {
    string s = "";
    foreach hash<auto> p in (h.pairIterator()) {
        s += p.key + "=" + p.value + ";";
    }
    return s;
}

int sub sum_range(int n) {
    int t = 0;
    foreach int v in (xrange(1, n)) {
        t += v;
    }
    return t;
}

int sub sum_iter(list<int> l) {
    int t = 0;
    foreach int v in (new Iter(l)) {
        t += v;
    }
    return t;
}

int sub sum_list_iterator(list<int> l) {
    int t = 0;
    foreach auto v in (new ListIterator(l)) {
        t += v;
    }
    return t;
}

string sub single_value(auto v) {
    string s = "";
    foreach auto e in (v) {
        s += fmt(e);
    }
    return s;
}

list<int> sub ref_no_index(list<int> l) {
    foreach int v in (\l) {
        v *= 10;
    }
    return l;
}

int sub lazy_no_index(list<int> l) {
    int t = 0;
    foreach int v in (map $1 * 2, select l, $1 > 1) {
        t += v;
    }
    return t;
}

# the body reads $#: in each way
list<auto> sub direct(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        r += (s, $#);
    }
    return r;
}

list<auto> sub nested_expr(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        r += ($# * 10 + ($# > 0 ? 1 : 0), s + ":" + $#);
    }
    return r;
}

list<auto> sub keys_index(hash<auto> h) {
    list<auto> r = ();
    foreach string k in (keys h) {
        r += (k, $#);
    }
    return r;
}

list<auto> sub range_index(int n) {
    list<auto> r = ();
    foreach int v in (xrange(5, n)) {
        r += (v, $#);
    }
    return r;
}

list<auto> sub iter_index(list<int> l) {
    list<auto> r = ();
    foreach int v in (new Iter(l)) {
        r += (v, $#);
    }
    return r;
}

list<auto> sub ref_index(list<int> l) {
    list<auto> r = ();
    foreach int v in (\l) {
        v += $#;
        r += $#;
    }
    return (l, r);
}

list<auto> sub lazy_index(list<int> l) {
    list<auto> r = ();
    foreach int v in (map $1 * 2 + $#, select l, $1 > 1) {
        r += (v, $#);
    }
    return r;
}

# $# in the source of an operator in the body is the loop's index; the code that the operator runs for each element
# reads the operator's own
list<auto> sub operator_source(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        r += fmt(map $1 + $#, (100, $#));
        r += fmt(select (10, 11, 12), $# != 1);
        r += fmt(map {$1: $#}, ("x", "y"));
        r += fmt((foldl $1 + $2, ($#, 1, 2)));
    }
    return r;
}

# only the operator in the body reads $#: the loop sets none of its own, and the operator's index is its own
list<auto> sub operator_only(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        r += fmt(map $# * 10, (7, 8, 9));
        r += fmt((first $# == 1, ("a", "b")));
        r += fmt((count $# > 0, (1, 2, 3)));
    }
    return r;
}

# a background expression in the body reads the loop's index in the new thread
list<auto> sub background_index(list<string> l) {
    list<auto> r = ();
    Mutex m();
    Counter c();
    foreach string s in (l) {
        c.inc();
        background (sub (string s, int i) { m.lock(); on_exit m.unlock(); r += sprintf("%s:%d", s, i); c.dec(); })(s,
            $#);
    }
    c.waitForZero();
    return sort(r);
}

# a context statement and a find expression in the body run their expressions as AST at run time, where $# reads the
# thread's implicit index
list<auto> sub context_index(list<int> l) {
    list<auto> r = ();
    hash<auto> data = {"a": (0, 1, 2, 3), "b": ("w", "x", "y", "z")};
    foreach int v in (l) {
        context (data) where (%a == $#) {
            r += (v, %b);
        }
        r += find %b in data where (%a == $# + 1);
    }
    return r;
}

# on_exit, on_success and on_error blocks, switch and try in the body
list<auto> sub blocks_index(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        on_exit {
            r += "exit" + $#;
        }
        on_success {
            r += "success" + $#;
        }
        on_error {
            r += "error" + $#;
        }
        switch ($#) {
            case 0:
                r += "zero";
                break;
            case 1:
                r += "one";
                break;
            default:
                r += "many" + $#;
        }
        try {
            if ($# == 1) {
                throw "ERR", "x";
            }
            r += "try" + $#;
        } catch (hash<ExceptionInfo> ex) {
            r += "catch" + $#;
        }
    }
    return r;
}

# a call in the body does not change the loop's index
int sub get_ten() {
    return 10;
}

list<auto> sub after_call(list<string> l) {
    list<auto> r = ();
    foreach string s in (l) {
        r += ($#, get_ten(), $#, sum_list((1, 2)), $#);
    }
    return r;
}

# nested loops: the inner index in the inner body, and the outer index in the outer body before and after the inner
# loop, whether or not the inner body reads its own
list<auto> sub nested(list<string> outer, list<string> inner) {
    list<auto> r = ();
    foreach string o in (outer) {
        r += ("o" + $#);
        foreach string i in (inner) {
            r += ("i" + $#);
        }
        r += ("a" + $#);
        foreach string i in (inner) {
            r += i;
        }
        r += ("b" + $#);
        foreach string i in (inner) {
            foreach string j in (inner) {
                r += ("j" + $#);
            }
            r += ("m" + $#);
        }
        r += ("c" + $#);
    }
    return r;
}

# break, continue, return, an exception and a rethrow in the body of an inner loop, which does or does not read its
# index: the outer index after the inner loop
list<auto> sub inner_exits(list<int> l, bool read_index) {
    list<auto> r = ();
    foreach int o in (l) {
        if (read_index) {
            foreach int i in (l) {
                if (i == 2) {
                    break;
                }
                r += "ib" + $#;
            }
        } else {
            foreach int i in (l) {
                if (i == 2) {
                    break;
                }
            }
        }
        r += "b" + $#;
        if (read_index) {
            foreach int i in (l) {
                if (i == 1) {
                    continue;
                }
                r += "ic" + $#;
            }
        } else {
            foreach int i in (l) {
                if (i == 1) {
                    continue;
                }
            }
        }
        r += "c" + $#;
        r += "r" + inner_return(l, read_index) + ":" + $#;
        try {
            inner_throw(l, read_index);
        } catch (hash<ExceptionInfo> ex) {
            r += "t" + ex.desc + ":" + $#;
        }
        try {
            inner_rethrow(l, read_index);
        } catch (hash<ExceptionInfo> ex) {
            r += "rt" + ex.desc + ":" + $#;
        }
        try {
            if (read_index) {
                foreach int i in (l) {
                    if (i == 2) {
                        throw "LOCAL", $#;
                    }
                }
            } else {
                foreach int i in (l) {
                    if (i == 2) {
                        throw "LOCAL", i;
                    }
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            r += "l" + ex.desc + ":" + $#;
        }
    }
    return r;
}

int sub inner_return(list<int> l, bool read_index) {
    if (read_index) {
        foreach int i in (l) {
            if (i == 2) {
                return $# + 100;
            }
        }
    } else {
        foreach int i in (l) {
            if (i == 2) {
                return i + 100;
            }
        }
    }
    return -1;
}

sub inner_throw(list<int> l, bool read_index) {
    if (read_index) {
        foreach int i in (l) {
            if (i == 2) {
                throw "ERR", $#;
            }
        }
    } else {
        foreach int i in (l) {
            if (i == 2) {
                throw "ERR", i;
            }
        }
    }
}

sub inner_rethrow(list<int> l, bool read_index) {
    if (read_index) {
        foreach int i in (l) {
            try {
                if (i == 2) {
                    throw "ERR", $#;
                }
            } catch (hash<ExceptionInfo> ex) {
                rethrow;
            }
        }
    } else {
        foreach int i in (l) {
            try {
                if (i == 2) {
                    throw "ERR", i;
                }
            } catch (hash<ExceptionInfo> ex) {
                rethrow;
            }
        }
    }
}

# the index of an operator after a call of code whose loop left by return or an exception
list<auto> sub operator_after_exits(list<int> l) {
    list<auto> r = ();
    r += fmt(map ($#, inner_return(l, False), $#, inner_return(l, True), $#), (5, 6));
    r += fmt(map ($#, (sub () { try { inner_throw(l, False); } catch () {} })(), $#,
        (sub () { try { inner_throw(l, True); } catch () {} })(), $#), (5, 6));
    return r;
}

# break, continue and return in the body of the loop itself
list<auto> sub own_exits(list<int> l) {
    list<auto> r = ();
    foreach int v in (l) {
        if (v == 1) {
            continue;
        }
        if (v == 3) {
            break;
        }
        r += (v, $#);
    }
    return r;
}

# operators in the body of an inner loop that reads no index of its own: their own indexes, and the outer loop's
# (not 0 from the second element on) after each
list<auto> sub operators_in_inner(list<string> outer) {
    list<auto> r = ();
    hash<auto> data = {"a": (0, 1, 2), "b": ("x", "y", "z")};
    foreach string o in (outer) {
        foreach int i in ((1,)) {
            r += fmt(select (5, 6, 7), $# != 1);
            r += fmt(map $# + 10, (5, 6, 7), $# != 0);
            r += fmt(map {$1: $#}, ("p", "q"));
            r += fmt(map {$1: $#}, ("p", "q", "s"), $# > 0);
            r += fmt((takewhile $# < 2, (1, 2, 3)));
            r += fmt((takeuntil $# == 1, (1, 2, 3)));
            r += fmt(map $# + 100, "single");
            r += fmt(select "single", $# == 0);
            r += fmt(map $# + 200, new ListIterator((1, 2)));
            r += fmt(map (find %b in data where (%a == $#)), (1, 2));
        }
        r += "o" + $#;
    }
    return r;
}

# the loop variable is a member
class Holder {
    public {
        int m;
        list<auto> seen = ();
    }

    run(list<int> l) {
        foreach m in (l) {
            seen += (m, $#);
        }
        foreach m in (l) {
            seen += m;
        }
    }
}

list<auto> sub member_target(list<int> l) {
    Holder o();
    o.run(l);
    return o.seen;
}

list<string> out = ();
out += "sum " + sum_list((1, 2, 3));
out += "keys " + keys_of({"a": 1, "b": 2});
out += "pairs " + pairs_of({"a": 1, "b": 2});
out += "range " + sum_range(4);
out += "iter " + sum_iter((4, 5));
out += "list iterator " + sum_list_iterator((6, 7));
out += "single " + single_value("s") + " " + single_value(NOTHING) + " " + single_value({"a": 1});
out += "ref " + fmt(ref_no_index((1, 2)));
out += "lazy " + lazy_no_index((1, 2, 3));
out += "direct " + fmt(direct(("a", "b", "c")));
out += "nested expr " + fmt(nested_expr(("a", "b")));
out += "keys index " + fmt(keys_index({"a": 1, "b": 2}));
out += "range index " + fmt(range_index(7));
out += "iter index " + fmt(iter_index((4, 5)));
out += "ref index " + fmt(ref_index((10, 20, 30)));
out += "lazy index " + fmt(lazy_index((1, 2, 3)));
out += "operator source " + fmt(operator_source(("a", "b")));
out += "operator only " + fmt(operator_only(("a", "b")));
out += "background " + fmt(background_index(("a", "b", "c")));
out += "context " + fmt(context_index((7, 8)));
out += "blocks " + fmt(blocks_index(("a", "b", "c")));
out += "after call " + fmt(after_call(("a", "b")));
out += "nested " + fmt(nested(("A", "B"), ("x", "y")));
out += "inner exits " + fmt(inner_exits((1, 2, 3), False));
out += "inner exits index " + fmt(inner_exits((1, 2, 3), True));
out += "operator after exits " + fmt(operator_after_exits((1, 2, 3)));
out += "own exits " + fmt(own_exits((0, 1, 2, 3, 4)));
out += "member " + fmt(member_target((1, 2)));
out += "operators in inner " + fmt(operators_in_inner(("A", "B", "C")));
# the same loops in a map: the operator's index after each
out += "in map " + fmt(map (direct(("p", "q")), $#, sum_list((1, 2)), $#, nested(("A",), ("x",)), $#), (0, 1));
foreach string line in (out) {
    printf("%s\n", line);
}
