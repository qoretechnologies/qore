%modern
# a class whose own variants of a method are private calls the public variant that it inherits for a caller outside the
# class hierarchy, in every execution mode, whether the call is resolved when it is parsed or when it is made; a
# private variant with the same signature overrides the inherited one (qore#6062); see
# inherited-public-variant.qtest (qore#7074)

class A {
    string get() { return "public A"; }
    private string get(hash<auto> h) { return "private A"; }
}

# a private variant of another signature: the public variant of A is inherited
class B inherits A {
    private string get(hash<auto> h) { return "private B"; }
}

# a private variant of the same signature overrides the public variant of A
class C inherits A {
    private string get() { return "private C"; }
}

sub t(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# top-level code calls the method of an object of a declared class
try {
    printf("top B(): %s\n", (new B()).get());
} catch (hash<ExceptionInfo> ex) {
    printf("top B(): %s\n", ex.err);
}
t("B()", auto sub () { return (new B()).get(); });
object ob = new B();
t("object B()", auto sub () { return ob.get(); });
t("object B({})", auto sub () { return ob.get({}); });
t("call_object_method B()", auto sub () { return call_object_method(ob, "get"); });
t("call reference B()", auto sub () { code c = \ob.get(); return c(); });
t("hasCallableMethod B", auto sub () { return ob.hasCallableMethod("get") ? "yes" : "no"; });
object oc = new C();
t("object C()", auto sub () { return oc.get(); });
t("call_object_method C()", auto sub () { return call_object_method(oc, "get"); });
t("hasCallableMethod C", auto sub () { return oc.hasCallableMethod("get") ? "yes" : "no"; });
