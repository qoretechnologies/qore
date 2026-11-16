# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the fixture of unused-post-increment.qtest: post-increments and post-decrements whose value is unused (expression
# statements, the update of a for loop) and whose value is used, of every kind of lvalue; the output must be the same
# in every execution mode and in compiled programs

%modern
%disable-all-warnings

our int gi = 10;
our auto ga;
our float gf = 1.5;

hashdecl HD {
    int a = 1;
    float f = 0.5;
}

class C {
    public {
        int i = 1;
        float f = 0.5;
        number n = 2.5n;
        auto a;
        static int s = 100;
        hash<string, int> h = {"k": 7};
    }

    update() {
        i++;
        i--;
        i++;
        f++;
        n--;
        a++;
        self.i++;
        s++;
        h.k++;
    }

    int used() {
        int r = i++;
        return r + i--;
    }

    static int staticUpdate() {
        s++;
        s--;
        s++;
        return s;
    }
}

sub show(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

sub inc_ref(reference<int> r) {
    r++;
    r++;
    r--;
}

sub inc_ref_auto(reference r) {
    r++;
}

int sub int_local_statements(int value) {
    int total = value;
    total++;
    total++;
    total++;
    total--;
    return total;
}

int sub int_local_used(int value) {
    int total = value;
    int a = total++;
    int b = total--;
    int c = total++ + total++;
    return a * 1000000 + b * 10000 + c * 100 + total;
}

int sub id(int v) {
    return v;
}

#! post forms in contexts whose value is used (which must give the old value), beside unused ones
/** used: the call argument (i++), the if condition (k++), the list elements (i++, j--), the ternaries assigned to
    t_true and t_false, the switch expression (s++), the while condition (w--), the ?? operand (coalesce++), the
    comparison operand (cmp++) and the closure's return value (closure_var++)

    unused: the ternaries as statements (c ? a++ : b--;), s++ before the switch and loops++ in the while loop
*/
hash<auto> sub used_contexts() {
    int i = 5;
    int arg = id(i++) * 100 + i;
    int k = 0;
    int n = 0;
    if (k++) {
        n = 1;
    }
    int j = 1;
    list<int> l = (i++, j--);
    int a = 5;
    int b = 1;
    bool c = True;
    c ? a++ : b--;
    c = False;
    c ? a++ : b--;
    int ta = 5;
    int tb = 1;
    c = True;
    int t_true = c ? ta++ : tb--;
    c = False;
    int t_false = c ? ta++ : tb--;
    int s = 1;
    s++;
    int sw;
    switch (s++) {
        case 2: sw = 2; break;
        default: sw = -1; break;
    }
    int w = 3;
    int loops = 0;
    while (w-- > 0) {
        loops++;
    }
    int coalesce = 5;
    int cv = coalesce++ ?? 0;
    int cmp = 5;
    int cr = cmp++ > 5 ? 1 : 0;
    int closure_var = 5;
    code f = int sub () { return closure_var++; };
    int closure_old = f();
    return {
        "arg": arg,
        "if": n * 100 + k,
        "list": l,
        "i": i,
        "j": j,
        "ternary": a * 100 + b,
        "used ternary": (t_true, t_false, ta, tb),
        "switch": sw * 100 + s,
        "while": loops * 100 + w,
        "coalesce": cv * 100 + coalesce,
        "compare": cr * 100 + cmp,
        "closure": closure_old * 100 + closure_var,
    };
}

int sub int_return_post(int value) {
    int x = value;
    return x++;
}

int sub for_loop(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
    }
    for (int i = n; i > 0; i--) {
        sum += i * 2;
    }
    int j = 0;
    for (int i = 0; i < n; i++, j--) {
        sum += j;
    }
    return sum;
}

int sub for_loop_body_count(int n) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        count++;
        if (i % 2) {
            count--;
        }
    }
    return count;
}

int sub while_loop(int n) {
    int i = 0;
    int k = 0;
    while (i++ < n) {
        k++;
    }
    return k * 100 + i;
}

hash<auto> sub overflow() {
    int max = MAXINT;
    max++;
    int min = MININT;
    min--;
    int max_used = MAXINT;
    int old_max = max_used++;
    int min_used = MININT;
    int old_min = min_used--;
    return {
        "max": max,
        "min": min,
        "max_used": max_used,
        "old_max": old_max,
        "min_used": min_used,
        "old_min": old_min,
    };
}

hash<auto> sub other_types() {
    softint si = 5;
    si++;
    si--;
    si++;
    float f = 1.25;
    f++;
    f++;
    f--;
    number n = 3.5n;
    n++;
    n--;
    n--;
    *int oi;
    oi++;
    *float of;
    of--;
    auto a;
    a++;
    auto s = "41";
    s++;
    auto fl = 2.5;
    fl--;
    auto nn = 7n;
    nn++;
    auto b = True;
    b++;
    auto d = 2024-01-02T03:04:05Z;
    d++;
    auto f_used = 1.5;
    auto f_old = f_used++;
    auto n_used = 1.5n;
    auto n_old = n_used--;
    return {
        "si": si,
        "f": f,
        "n": n,
        "oi": oi,
        "of": of,
        "a": a,
        "s": s,
        "fl": fl,
        "nn": nn,
        "b": b,
        "d": d,
        "f_used": f_used,
        "f_old": f_old,
        "n_used": n_used,
        "n_old": n_old,
    };
}

hash<auto> sub closures() {
    int c = 0;
    code inc = sub () {
        c++;
        c++;
        c--;
    };
    inc();
    inc();
    int captured = c;
    c++;
    float cf = 0.5;
    code incf = sub () {
        cf++;
    };
    incf();
    code used = int sub () {
        return c++;
    };
    int old = used();
    # a closure that changes a local of its own
    code own = int sub (int n) {
        int k = n;
        k++;
        k++;
        return k;
    };
    return {
        "captured": captured,
        "c": c,
        "cf": cf,
        "old": old,
        "own": own(5),
    };
}

hash<auto> sub globals() {
    gi++;
    gi--;
    gi++;
    ga++;
    gf--;
    int old = gi++;
    return {
        "gi": gi,
        "ga": ga,
        "gf": gf,
        "old": old,
    };
}

hash<auto> sub references() {
    int x = 3;
    inc_ref(\x);
    auto y = "9";
    inc_ref_auto(\y);
    auto z;
    inc_ref_auto(\z);
    return {
        "x": x,
        "y": y,
        "z": z,
    };
}

hash<auto> sub hash_keys() {
    hash<string, int> h = {"a": 1, "b": 2};
    h.a++;
    h.a++;
    h.b--;
    string k = "b";
    h{k}++;
    h{k}++;
    h.c++;
    int old = h.a++;
    hash<auto!> ha = {"x": 1.5};
    ha.x++;
    ha.y++;
    ha.z--;
    ha.n.m++;
    hash<string, float> hf = {"f": 0.25};
    hf.f++;
    hash<string, number> hn = {"n": 1n};
    hn.n--;
    hash<string, softint> hs = {"s": 1};
    hs.s++;
    # the pre forms, whose value is used or not
    ++h.a;
    --h{k};
    int pre = ++h.a;
    int pre_dyn = --h{k};
    hash<HD> hd();
    hd.a++;
    ++hd.a;
    hd.f--;
    string m = "a";
    hd{m}++;
    --hd{m};
    *string err;
    string bad = "x";
    try {
        hd{bad}++;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    *string pre_err;
    try {
        ++hd{bad};
    } catch (hash<ExceptionInfo> ex) {
        pre_err = ex.err;
    }
    return {
        "h": h,
        "old": old,
        "pre": pre,
        "pre_dyn": pre_dyn,
        "hd": hd,
        "err": err,
        "pre_err": pre_err,
        "ha": ha,
        "hf": hf,
        "hn": hn,
        "hs": hs,
    };
}

hash<auto> sub members() {
    C o();
    o.update();
    o.i++;
    o.f--;
    o.n++;
    o.a--;
    o.h.k--;
    int used = o.used();
    int s = C::staticUpdate();
    C::s++;
    return {
        "i": o.i,
        "f": o.f,
        "n": o.n,
        "a": o.a,
        "h": o.h,
        "used": used,
        "s": s,
        "s_after": C::s,
    };
}

hash<auto> sub lists() {
    list<int> l = (1, 2, 3);
    l[0]++;
    l[1]--;
    l[2]++;
    l[5]++;
    list<auto> la = (1.5, "2", NOTHING);
    la[0]++;
    la[1]++;
    la[2]--;
    int old = l[0]++;
    list<int> fl = (1, 2, 3);
    foreach int e in (fl) {
        e++;
    }
    list<int> rl = (1, 2, 3);
    foreach int e in (\rl) {
        e++;
        e++;
        e--;
    }
    return {
        "l": l,
        "la": la,
        "old": old,
        "fl": fl,
        "rl": rl,
    };
}

hash<auto> sub handlers() {
    int x = 0;
    try {
        x++;
        x++;
        throw "ERR";
    } catch (hash<ExceptionInfo> ex) {
        x--;
    }
    hash<string, int> h = {"a": 1};
    try {
        h.a++;
        h.a--;
        h.a++;
    } catch () {
    }
    auto a = "x";
    try {
        a++;
    } catch (hash<ExceptionInfo> ex) {
        a = ex.err;
    }
    return {
        "x": x,
        "h": h,
        "a": a,
    };
}

int sub threads() {
    gi = 0;
    int local_count = 0;
    code worker = sub () {
        for (int i = 0; i < 2000; i++) {
            gi++;
            local_count++;
        }
    };
    Counter running();
    for (int t = 0; t < 4; t++) {
        running.inc();
        background sub () {
            on_exit running.dec();
            worker();
        }();
    }
    running.waitForZero();
    return gi * 100000 + local_count;
}

show("int local statements", int_local_statements(5));
show("int local used", int_local_used(5));
show("int return post", int_return_post(5));
show("used contexts", used_contexts());
show("for loop", for_loop(10));
show("for loop body count", for_loop_body_count(9));
show("while loop", while_loop(4));
show("overflow", overflow());
show("other types", other_types());
show("closures", closures());
show("globals", globals());
show("references", references());
show("hash keys", hash_keys());
show("members", members());
show("lists", lists());
show("handlers", handlers());
show("threads", threads());
