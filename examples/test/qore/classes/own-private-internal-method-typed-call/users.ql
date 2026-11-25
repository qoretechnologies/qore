# -*- mode: qore; indent-tabs-mode: nil -*-
# the calls of own-private-internal-method-typed-call.qtest, included by probe.q and module-user.q

sub t(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

OITC::K k();
OITC::K2 k2();
OITC::C c();
t("private subclass typed", auto sub () { return OITC::B::typedK(k); });
t("private subclass untyped", auto sub () { return OITC::B::untyped(k); });
t("private subclass call reference", auto sub () { return OITC::B::refK(k); });
t("public subclass typed", auto sub () { return OITC::B::typedK2(k2); });
t("public subclass untyped", auto sub () { return OITC::B::untyped(k2); });
t("created method typed", auto sub () { return OITC::B::typedC(c); });
t("created method overload", auto sub () { return OITC::B::overloadC(c); });
t("created method call reference", auto sub () { return OITC::B::refC(c); });
t("created method untyped", auto sub () { return OITC::B::untyped(c); });
OITC::NK nk();
t("no private:internal variants typed", auto sub () { return OITC::N::typedNK(nk); });
t("no private:internal variants untyped", auto sub () { return OITC::N::untyped(nk); });
OITC::PA pa();
t("parent class object typed", auto sub () { return OITC::PB::typedA(pa); });
t("parent class object untyped", auto sub () { return OITC::PB::untypedA(pa); });
t("parent class object call reference", auto sub () { return OITC::PB::refA(pa); });
t("decorator", auto sub () { return (new OITC::PB(new OITC::PA())).m("k"); });
