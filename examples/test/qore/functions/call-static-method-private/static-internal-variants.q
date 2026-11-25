%modern

class Parent {
    static string label(softstring value) { return "parent public"; }
}
class Other {
    static string label(softstring value) { return "other public"; }
}
class Base inherits Parent {
    private:internal static string label(string value) { return "base internal"; }
    static string label(softstring value) { return "base public"; }

    static auto qualified(string value) { return Child::label(value); }
    static auto reference(string value) { code ref = \Child::label(); return ref(value); }
    static auto objectRef(Child child, string value) { code ref = \child.label(); return ref(value); }
    static auto typed(Child child, string value) { return child.label(value); }
    static auto untyped(auto child, string value) { return child.label(value); }
    static string parentCall(string value) { return Parent::label(value); }
    static string unrelatedCall(string value) { return Other::label(value); }
    static auto byName(string value) { return call_static_method("Child", "label", value); }
    static auto publicByName(Child child, string value) { return call_object_method(child, "label", value); }
    static auto publicByArgs(Child child, string value) { return call_object_method_args(child, "label", (value,)); }
}
class Child inherits Base {
    static string label(softstring value) { return "child public"; }
}
Child child();
printf("qualified: %s\nreference: %s\ntyped: %s\nuntyped: %s\nstatic by name: %s\n",
    Base::qualified("x"), Base::reference("x"), Base::typed(child, "x"), Base::untyped(child, "x"), Base::byName("x"));
printf("object by name: %s\nobject by args: %s\noutside: %s\n",
    Base::publicByName(child, "x"), Base::publicByArgs(child, "x"), Child::label("x"));
printf("object reference: %s\n", Base::objectRef(child, "x"));
printf("parent: %s\nunrelated: %s\n", Base::parentCall("x"), Base::unrelatedCall("x"));
