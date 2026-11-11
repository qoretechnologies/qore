#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# modifications in background expressions through an object, which the thread shares; prints what the code that starts
# the threads sees afterwards, which must be the same in every execution mode

%modern

class BlpC {
    public {
        int x = 0;
        *int y = 5;
        hash<auto> h = {};
        hash<string, int> hi = {};
        list<auto> l = (1, 2, 3);
        string s = "abc ";
        int n = 12;
    }
}

class BlpD {
    public {
        BlpC c();
        *BlpC oc = new BlpC();
    }
}

class BlpM {
    public {
        BlpC o();
        hash<string, BlpC> m = {"a": new BlpC()};
        list<BlpC> lc = (new BlpC(),);
    }

    hash<auto> run() {
        thread_join(background (o.x = 1));
        thread_join(background (m.a.x = 3));
        thread_join(background (lc[0].x = 4));
        thread_join(background (o.h.k = 5));
        return {"o.x": o.x, "m.a.x": m.a.x, "lc[0].x": lc[0].x, "o.h": o.h};
    }
}

#! object steps followed by containers of the object
hash<auto> sub object_then_container() {
    BlpC o();
    thread_join(background (o.h.k = 1));
    list<BlpC> l = (new BlpC(),);
    thread_join(background (l[0].h.k = 3));
    BlpD d();
    thread_join(background (d.c.h.z = 4));
    BlpC o2();
    thread_join(background (cast<hash<string, int>>(o2.hi).a = 1));
    return {"o.h": o.h, "l[0].h": l[0].h, "d.c.h": d.c.h, "o2.hi": o2.hi};
}

#! remove and delete of members of objects reached from locals
hash<auto> sub remove_delete() {
    BlpC o();
    thread_join(background (remove o.y));
    BlpD d();
    thread_join(background (delete d.oc));
    BlpC o2();
    o2.h = {"a": 1, "b": 2};
    thread_join(background (remove o2.h.a));
    return {"o.y": o.y, "d.oc": d.oc, "o2.h": o2.h};
}

#! every modifying operator on a member path of an object reached from a local
hash<auto> sub operators() {
    hash<string, BlpC> objs;
    foreach string op in (("=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "++", "++post", "--",
            "--post", "push", "unshift", "pop", "shift", "splice", "extract", "s///", "tr///", "trim", "chomp",
            "list=", "hash+=")) {
        objs{op} = new BlpC();
    }
    BlpC o;
    o = objs."=";       thread_join(background (o.n = 3));
    o = objs."+=";      thread_join(background (o.n += 3));
    o = objs."-=";      thread_join(background (o.n -= 3));
    o = objs."*=";      thread_join(background (o.n *= 3));
    o = objs."/=";      thread_join(background (o.n /= 3));
    o = objs."%=";      thread_join(background (o.n %= 5));
    o = objs."&=";      thread_join(background (o.n &= 6));
    o = objs."|=";      thread_join(background (o.n |= 1));
    o = objs."^=";      thread_join(background (o.n ^= 5));
    o = objs."<<=";     thread_join(background (o.n <<= 2));
    o = objs.">>=";     thread_join(background (o.n >>= 2));
    o = objs."++";      thread_join(background (++o.n));
    o = objs."++post";  thread_join(background (o.n++));
    o = objs."--";      thread_join(background (--o.n));
    o = objs."--post";  thread_join(background (o.n--));
    o = objs."push";    thread_join(background (push o.l, 4));
    o = objs."unshift"; thread_join(background (unshift o.l, 0));
    o = objs."pop";     thread_join(background (pop o.l));
    o = objs."shift";   thread_join(background (shift o.l));
    o = objs."splice";  thread_join(background (splice o.l, 0, 1));
    o = objs."extract"; thread_join(background (extract o.l, 1, 1));
    o = objs."s///";    thread_join(background (o.s =~ s/a/z/));
    o = objs."tr///";   thread_join(background (o.s =~ tr/b/y/));
    o = objs."trim";    thread_join(background (trim o.s));
    o = objs."chomp";   o.s = "abc\n"; thread_join(background (chomp o.s));
    o = objs."list=";   thread_join(background ((o.x, o.n) = (7, 8)));
    o = objs."hash+=";  thread_join(background (o.h += {"k": 1}));
    return map {$1.key: ($1.value.x, $1.value.y, $1.value.n, $1.value.l, $1.value.s, $1.value.h)}, objs.pairIterator();
}

# each function runs twice, so that tiered execution compiles it before the run printed
for (int i = 0; i < 2; ++i) {
    object_then_container();
    remove_delete();
    operators();
    BlpM m();
    m.run();
}
printf("object then container: %y\n", object_then_container());
printf("remove and delete: %y\n", remove_delete());
foreach hash<auto> i in (operators().pairIterator()) {
    printf("%s: %y\n", i.key, i.value);
}
BlpM m();
printf("member roots: %y\n", m.run());
