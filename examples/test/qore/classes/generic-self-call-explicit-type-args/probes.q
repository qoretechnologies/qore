# probes of generic-self-call-explicit-type-args.qtest (qore#5818): a call of a method of the class in class code
# without "self." (a self call, an unqualified static method call, a call of a base class method by its class name)
# keeps its explicit type arguments, concrete or naming a type parameter of the calling code; probes() returns one line
# per pass, and each pass runs every probe, so that the code runs on every tier with tiered execution

%modern

X sub identity<X>(X x) { return x; }

class Base {
    auto baseCheck<X>(auto v) { try { return identity<X>(v); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
}

class Forms inherits Base {
    auto check<X>(auto v) { try { return identity<X>(v); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
    static auto staticCheck<X>(auto v) {
        try { return identity<X>(v); } catch (hash<ExceptionInfo> ex) { return ex.err; }
    }

    # concrete explicit type arguments
    auto selfConcrete(auto v) { return check<int>(v); }
    auto staticConcrete(auto v) { return staticCheck<int>(v); }
    auto baseConcrete(auto v) { return Base::baseCheck<int>(v); }

    # explicit type arguments naming the type parameter of the calling method
    auto selfForward<Y>(auto v) { return check<Y>(v); }
    auto staticForward<Y>(auto v) { return staticCheck<Y>(v); }
    auto baseForward<Y>(auto v) { return Base::baseCheck<Y>(v); }

    # an unqualified static method call in static class code
    static auto staticFromStatic(auto v) { return staticCheck<int>(v); }
    static auto staticForwardFromStatic<Y>(auto v) { return staticCheck<Y>(v); }
}

# a generic class: the explicit type argument names the type parameter of the class
class Box<T> {
    auto check<X>(auto v) { try { return identity<X>(v); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
    auto classForward(auto v) { return check<T>(v); }
}

string sub probes() {
    Forms f();
    Box<int> b();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y\n", i,
            f.selfConcrete(1), f.selfConcrete("a"),
            f.staticConcrete("b"), f.baseConcrete("c"),
            f.selfForward<int>(2), f.selfForward<int>("d"),
            f.staticForward<int>("e"), f.baseForward<int>("f"),
            b.classForward(3), b.classForward("g"),
            Forms::staticFromStatic("h"), Forms::staticForwardFromStatic<int>(4),
            Forms::staticForwardFromStatic<int>("i"));
    }
    return rv;
}
