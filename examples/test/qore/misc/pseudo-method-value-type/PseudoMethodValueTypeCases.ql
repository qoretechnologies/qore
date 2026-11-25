# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of pseudo-method-value-type.qtest (qore#7363): pseudo-method calls resolved at parse time whose receiver has
# a value of another type at run time.  Each call gives the result of the method of the pseudo-class of the value's
# own type, or the exception of a method that the value's pseudo-class does not have; run_cases() gives a line
# "<round> <value> <case>: <result or exception code>" for each.  Included by PseudoMethodValueType.q and parsed into a sandboxed
# Program by PseudoMethodValueTypeSandbox.q.

%modern

class PseudoMethodValueTypeObject {
    string upr() {
        return "object upr";
    }
    int size() {
        return 5;
    }
    string substr(int start, int len) {
        return sprintf("object substr %d %d", start, len);
    }
}

string sub pmvt_result(code c) {
    try {
        # without the addresses of closures and objects
        return regex_subst(sprintf("%y", c()), "0x[0-9a-f]+", "0x", RE_Global);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# calls of methods of <string> (firstKey() is declared to return *string)
auto sub pmvt_fk_size(hash h) { return h.firstKey().size(); }
auto sub pmvt_fk_strlen(hash h) { return h.firstKey().strlen(); }
auto sub pmvt_fk_length(hash h) { return h.firstKey().length(); }
auto sub pmvt_fk_substr(hash h) { return h.firstKey().substr(0, 16); }
auto sub pmvt_fk_upr(hash h) { return h.firstKey().upr(); }
auto sub pmvt_fk_lwr(hash h) { return h.firstKey().lwr(); }
auto sub pmvt_fk_find(hash h) { return h.firstKey().find("b"); }
auto sub pmvt_fk_split(hash h) { return h.firstKey().split(","); }
auto sub pmvt_fk_equal_partial(hash h) { return h.firstKey().equalPartial("a"); }
auto sub pmvt_fk_to_string(hash h) { return h.firstKey().toString(); }
auto sub pmvt_fk_encoding(hash h) { return h.firstKey().encoding(); }
auto sub pmvt_fk_empty(hash h) { return h.firstKey().empty(); }
auto sub pmvt_fk_val(hash h) { return h.firstKey().val(); }
auto sub pmvt_fk_type(hash h) { return h.firstKey().type(); }
# calls of methods of <list> (keys() is declared to return list<string>)
auto sub pmvt_k_size(hash h) { return h.keys().size(); }
auto sub pmvt_k_first(hash h) { return h.keys().first(); }
auto sub pmvt_k_last(hash h) { return h.keys().last(); }
auto sub pmvt_k_join(hash h) { return h.keys().join(","); }
auto sub pmvt_k_contains(hash h) { return h.keys().contains("a"); }
auto sub pmvt_k_lsize(hash h) { return h.keys().lsize(); }
auto sub pmvt_k_empty(hash h) { return h.keys().empty(); }
# calls of methods of <int> (size() is declared to return int)
auto sub pmvt_s_abs(hash h) { return h.size().abs(); }
auto sub pmvt_s_to_base(hash h) { return h.size().toBase(16); }
auto sub pmvt_s_to_string(hash h) { return h.size().toString(); }

#! the cases that every execution mode makes through the dispatch of the call
const PmvtCases = (
    "fk_strlen", "fk_length", "fk_upr", "fk_lwr", "fk_find", "fk_split", "fk_equal_partial", "fk_encoding", "fk_type",
    "k_first", "k_last", "k_join", "k_contains", "k_lsize", "k_empty",
    "s_abs", "s_to_base",
);

#! the cases that compiled code can make without the dispatch of the call, for a receiver of their parse type
const PmvtFastCases = (
    "fk_size", "fk_substr", "fk_to_string", "fk_empty", "fk_val", "k_size", "s_to_string",
);

#! returns the result of each case of PmvtFastCases (\a fast) or PmvtCases for each value
list<string> sub run_cases(bool fast) {
    PseudoMethodValueTypeObject obj();
    hash<string, auto> values = {
        "list": (1, 2),
        "int": 12345,
        "float": 2.5,
        "number": 3.5n,
        "bool": True,
        "binary": <414243>,
        "date": 2026-01-01T00:00:00Z,
        "hash": {"x": 1},
        "object": obj,
        "buffer": new buffer<int64>((1, 2, 3)),
        "closure": sub () {},
        "nothing": NOTHING,
        "string": "abc,b",
    };
    list<string> rv = ();
    # several rounds, so that the compiled modes reach native code
    for (int round = 0; round < 3; ++round) {
        foreach hash<auto> i in (values.pairIterator()) {
            auto v = i.value;
            hash h = {
                "firstKey": auto sub () { return v; },
                "keys": auto sub () { return v; },
                "size": auto sub () { return v; },
            };
            foreach string c in (fast ? PmvtFastCases : PmvtCases) {
                rv += sprintf("%d %s %s: %s", round, i.key, c,
                    pmvt_result(auto sub () { return call_function("pmvt_" + c, h); }));
            }
        }
    }
    return rv;
}
