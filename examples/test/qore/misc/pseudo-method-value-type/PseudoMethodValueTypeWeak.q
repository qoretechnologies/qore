#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# pseudo-method calls on weak references to lists, hashes and objects in locals, hash members, list elements and
# class members: each gives the result of the method of the target (pseudo-method-value-type.qtest, qore#7363); each
# line gives "<round> <case>: <result or exception code>"

%modern
%allow-weak-references

hash g = {"a": 1, "b": 2};
list gl = (1, 2, 3);

class WeakObjectTarget {
    public {
        int x = 1;
    }
}

WeakObjectTarget go();

string sub result(code c) {
    try {
        return regex_subst(sprintf("%y", c()), "0x[0-9a-f]+", "0x", RE_Global);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

class WeakMembers {
    public {
        list wlm = ();
        hash wh = {};
        hash<string, hash<auto>> hhm = {"x": {}};
        object wo;
    }

    list<string> run(int round) {
        wlm := gl;
        wh := g;
        hhm.x := g;
        wo := go;
        list<string> rv = ();
        rv += sprintf("%d member list size: %s", round, result(auto sub () { return wlm.size(); }));
        rv += sprintf("%d member list first: %s", round, result(auto sub () { return wlm.first(); }));
        rv += sprintf("%d member list last: %s", round, result(auto sub () { return wlm.last(); }));
        rv += sprintf("%d member list empty: %s", round, result(auto sub () { return wlm.empty(); }));
        rv += sprintf("%d member list val: %s", round, result(auto sub () { return wlm.val(); }));
        rv += sprintf("%d member list join: %s", round, result(auto sub () { return wlm.join(","); }));
        rv += sprintf("%d member list typeCode: %s", round, result(auto sub () { return wlm.typeCode(); }));
        rv += sprintf("%d member list type: %s", round, result(auto sub () { return wlm.type(); }));
        rv += sprintf("%d member hash keys: %s", round, result(auto sub () { return wh.keys(); }));
        rv += sprintf("%d member hash firstKey: %s", round, result(auto sub () { return wh.firstKey(); }));
        rv += sprintf("%d member hash size: %s", round, result(auto sub () { return wh.size(); }));
        rv += sprintf("%d member hash member keys: %s", round, result(auto sub () { return hhm.x.keys(); }));
        rv += sprintf("%d member hash member size: %s", round, result(auto sub () { return hhm.x.size(); }));
        rv += sprintf("%d member hash member typeCode: %s", round, result(auto sub () { return hhm.x.typeCode(); }));
        rv += sprintf("%d member hash member type: %s", round, result(auto sub () { return hhm.x.type(); }));
        rv += sprintf("%d member object className: %s", round, result(auto sub () { return wo.className(); }));
        rv += sprintf("%d member object typeCode: %s", round, result(auto sub () { return wo.typeCode(); }));
        return rv;
    }
}

auto sub param_first(list l) { return l.first(); }
auto sub param_size(list l) { return l.size(); }
auto sub param_empty(list l) { return l.empty(); }
auto sub param_last(list l) { return l.last(); }
auto sub weak_local_first(hash<string, list> hl) { list l := hl.x; return l.first(); }
auto sub weak_local_size(hash<string, list> hl) { list l := hl.x; return l.size(); }

list<string> sub run(int round) {
    list<string> rv = ();
    list wl := gl;
    hash wh := g;
    rv += sprintf("%d local list size: %s", round, result(auto sub () { return wl.size(); }));
    rv += sprintf("%d local list first: %s", round, result(auto sub () { return wl.first(); }));
    rv += sprintf("%d local list join: %s", round, result(auto sub () { return wl.join(","); }));
    rv += sprintf("%d local list typeCode: %s", round, result(auto sub () { return wl.typeCode(); }));
    rv += sprintf("%d local list type: %s", round, result(auto sub () { return wl.type(); }));
    rv += sprintf("%d local hash typeCode: %s", round, result(auto sub () { return wh.typeCode(); }));
    rv += sprintf("%d local hash type: %s", round, result(auto sub () { return wh.type(); }));
    rv += sprintf("%d local hash keys: %s", round, result(auto sub () { return wh.keys(); }));
    rv += sprintf("%d local hash firstKey: %s", round, result(auto sub () { return wh.firstKey(); }));
    hash<string, hash<auto>> hm = {"x": {}};
    hm.x := g;
    rv += sprintf("%d hash member keys: %s", round, result(auto sub () { return hm.x.keys(); }));
    rv += sprintf("%d hash member firstKey: %s", round, result(auto sub () { return hm.x.firstKey(); }));
    rv += sprintf("%d hash member size: %s", round, result(auto sub () { return hm.x.size(); }));
    rv += sprintf("%d hash member typeCode: %s", round, result(auto sub () { return hm.x.typeCode(); }));
    rv += sprintf("%d hash member type: %s", round, result(auto sub () { return hm.x.type(); }));
    rv += sprintf("%d hash member empty: %s", round, result(auto sub () { return hm.x.empty(); }));
    rv += sprintf("%d hash member val: %s", round, result(auto sub () { return hm.x.val(); }));
    hash<string, list> hl = {"x": ()};
    hl.x := gl;
    rv += sprintf("%d hash list member size: %s", round, result(auto sub () { return hl.x.size(); }));
    rv += sprintf("%d hash list member first: %s", round, result(auto sub () { return hl.x.first(); }));
    rv += sprintf("%d hash list member last: %s", round, result(auto sub () { return hl.x.last(); }));
    rv += sprintf("%d hash list member empty: %s", round, result(auto sub () { return hl.x.empty(); }));
    rv += sprintf("%d hash list member join: %s", round, result(auto sub () { return hl.x.join(","); }));
    rv += sprintf("%d hash list member lsize: %s", round, result(auto sub () { return hl.x.lsize(); }));
    rv += sprintf("%d hash list member typeCode: %s", round, result(auto sub () { return hl.x.typeCode(); }));
    rv += sprintf("%d hash list member type: %s", round, result(auto sub () { return hl.x.type(); }));
    rv += sprintf("%d hash list member val: %s", round, result(auto sub () { return hl.x.val(); }));
    rv += sprintf("%d hash list member join named: %s", round,
        result(auto sub () { return hl.x.join(separator: "-"); }));
    rv += sprintf("%d hash list member join reference: %s", round, result(auto sub () {
        code c = \hl.x.join();
        return c(",");
    }));
    rv += sprintf("%d param first: %s", round, result(auto sub () { return param_first(hl.x); }));
    rv += sprintf("%d param size: %s", round, result(auto sub () { return param_size(hl.x); }));
    rv += sprintf("%d param empty: %s", round, result(auto sub () { return param_empty(hl.x); }));
    rv += sprintf("%d param last: %s", round, result(auto sub () { return param_last(hl.x); }));
    rv += sprintf("%d weak local first: %s", round, result(auto sub () { return weak_local_first(hl); }));
    rv += sprintf("%d weak local size: %s", round, result(auto sub () { return weak_local_size(hl); }));
    # weak references whose targets have been freed
    {
        list tmp = (7, 8);
        hash th = {"z": 1};
        hl.x := tmp;
        hm.x := th;
        remove tmp;
        remove th;
    }
    rv += sprintf("%d freed list size: %s", round, result(auto sub () { return hl.x.size(); }));
    rv += sprintf("%d freed list first: %s", round, result(auto sub () { return hl.x.first(); }));
    rv += sprintf("%d freed list typeCode: %s", round, result(auto sub () { return hl.x.typeCode(); }));
    rv += sprintf("%d freed list type: %s", round, result(auto sub () { return hl.x.type(); }));
    rv += sprintf("%d freed list empty: %s", round, result(auto sub () { return hl.x.empty(); }));
    rv += sprintf("%d freed hash size: %s", round, result(auto sub () { return hm.x.size(); }));
    rv += sprintf("%d freed hash keys: %s", round, result(auto sub () { return hm.x.keys(); }));
    rv += sprintf("%d freed hash typeCode: %s", round, result(auto sub () { return hm.x.typeCode(); }));
    rv += sprintf("%d freed hash val: %s", round, result(auto sub () { return hm.x.val(); }));
    list<auto!> ll = (NOTHING, (), {});
    ll[0] := gl;
    rv += sprintf("%d list element size: %s", round, result(auto sub () { return ll[0].size(); }));
    rv += sprintf("%d list element join: %s", round, result(auto sub () { return ll[0].join(","); }));
    WeakMembers m();
    rv += m.run(round);
    return rv;
}

# several rounds, so that the compiled modes reach native code
for (int round = 0; round < 3; ++round) {
    foreach string line in (run(round)) {
        printf("%s\n", line);
    }
}
