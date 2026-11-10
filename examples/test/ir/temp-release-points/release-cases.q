%modern
%disable-warning invalid-operation
# the cases of temp-release-points.qtest: the point at which a value read from a local, or a temporary that holds it,
# is released, when the statement writes the local later (qore#5712); see design/temporary-lifetime.md

# Copyright (C) 2026 Qore Technologies, s.r.o.

class E {
    public {
        string n;
        static string s;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("  dtor %s\n", n);
    }

    string m(auto v) {
        return "m";
    }

    static string sm(auto v) {
        return "sm";
    }
}

#! records a point of the statement
string sub mark(string m) {
    printf("  mark %s\n", m);
    return "";
}

string sub f1(auto v) {
    return "f1";
}

auto sub id(auto v) {
    return v;
}

string sub raise() {
    throw "X";
}

#! runs a case
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s\n", ex.err);
    }
    printf("  end\n");
}

sub main() {
    # a list or hash literal, or the value of an operator, passed to a call is released when the call returns, and the
    # read of the local in it does not keep the value: the write destroys it
    t("list literal", auto sub () { auto o = new E("lit"); string s = f1((o,)) + (o = NOTHING) + mark("lit"); });
    t("list literal written", auto sub () { auto o = new E("litw"); string s = f1((o, o = NOTHING)) + mark("litw"); });
    t("hash literal", auto sub () { auto o = new E("hash"); string s = f1({"a": o}) + (o = NOTHING) + mark("hash"); });
    t("list plus", auto sub () {
        auto o = new E("plus");
        list<auto> l = ();
        string s = f1(l + o) + (o = NOTHING) + mark("plus");
    });
    t("two calls", auto sub () { auto o = new E("two"); string s = f1((o, 1)) + f1(2) + (o = NOTHING) + mark("two"); });
    t("method literal", auto sub () {
        E c("callee");
        auto o = new E("meth");
        string s = c.m((o,)) + (o = NOTHING) + mark("meth");
    });
    t("static literal", auto sub () { auto o = new E("st"); string s = E::sm((o,)) + (o = NOTHING) + mark("st"); });
    t("closure literal", auto sub () {
        code c = string sub (auto v) { return "c"; };
        auto o = new E("clo");
        string s = c((o,)) + (o = NOTHING) + mark("clo");
    });
    # a member read from a local does not keep the object
    t("member", auto sub () { auto o = new E("mem"); string s = o.n + (o = NOTHING) + mark("mem"); });
    t("member twice", auto sub () { auto o = new E("mem2"); string s = o.n + o.n + (o = NOTHING) + mark("mem2"); });
    # without a later write nothing changes: the local keeps the value to the end
    t("no write", auto sub () { auto o = new E("nw"); string s = f1((o,)) + mark("nw"); });
    # the result of an operator that is the receiver of a chain lives until the end of the statement
    t("remove receiver", auto sub () { auto o = new E("rr"); string s = f1(1) + (remove o).n + mark("rr"); });
    t("pop receiver", auto sub () { list<auto> l = (new E("pr"),); string s = f1(1) + (pop l).n + mark("pr"); });
    t("shift receiver", auto sub () { list<auto> l = (new E("sr"),); string s = f1(1) + (shift l).n + mark("sr"); });
    t("extract receiver", auto sub () {
        list<auto> l = (new E("xr"),);
        string s = f1(1) + (extract l, 0, 1).size() + mark("xr");
    });
    t("member remove receiver", auto sub () {
        hash<auto> h = {"e": new E("mr")};
        string s = f1(1) + (remove h.e).n + mark("mr");
    });
    # an argument created by a call
    t("new argument", auto sub () { string s = f1(new E("na")) + mark("na"); });
    t("call argument", auto sub () { auto o = new E("ca"); string s = f1(id(o)) + (o = NOTHING) + mark("ca"); });
    t("remove argument", auto sub () {
        auto o = new E("ra");
        list<auto> l = (o,);
        o = NOTHING;
        string s = f1(remove l[0]) + mark("ra");
    });
    t("pop argument", auto sub () { list<auto> l = (new E("pa"),); string s = f1(pop l) + mark("pa"); });
    t("new in literal", auto sub () { string s = f1((new E("nl"), 1)) + mark("nl"); });
    # the operand of an operator is a temporary of the statement: it lives until the end of the statement
    t("operand of plus", auto sub () {
        auto o = new E("op");
        hash<auto> h = {};
        string s = f1(h + {"a": o}) + (o = NOTHING) + mark("op");
    });
    t("operand of equals", auto sub () { bool b = (new E("eq") == NOTHING); mark("eq"); });
    t("operand of and", auto sub () { bool b = (new E("and") && True); mark("and"); });
    # an exception after the release
    t("exception", auto sub () {
        auto o = new E("ex");
        string s = f1((o,)) + (o = NOTHING) + mark("ex") + raise();
    });
    # in a loop
    t("loop", auto sub () {
        for (int i = 0; i < 2; ++i) {
            auto o = new E("loop" + i);
            string s = f1((o,)) + (o = NOTHING) + mark("loop" + i);
        }
    });
}

main();
