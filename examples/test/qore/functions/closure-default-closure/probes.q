# probes of closure-default-closure.qtest (qore#5903): a closure whose parameter has a closure as its default value,
# which a qcc executable reads when it builds the closure (and crashed reading, without the current Program): at the
# top level, in a function, in a method, nested twice, in a member initializer (read with the closure's IR), with a
# function's default value, capturing a variable of the function or of the top level that creates the closure (which
# the default value binds to by its slot, also with a later local of the same name, and in a closure created in a
# closure's code), and an enum default value; probes() returns one line per pass, so that the code runs on every tier
# with tiered execution

%modern
%disable-warning duplicate-local-vars

enum Color { Red = "red", Green = "green" }

code top = auto sub (code c = auto sub () { return 7; }) { return c(); };
int top_value = 5;
code top_capture = auto sub (code c = auto sub () { return top_value; }) { return c(); };
code enum_default = auto sub (Color c = Color::Green) { return c.toString(); };
code nested = auto sub (code c = auto sub (code d = auto sub () { return 8; }) { return d(); }) { return c(); };

auto sub in_function() {
    code f = auto sub (code c = auto sub () { return 9; }) { return c(); };
    return f();
}

auto sub captures(int n) {
    code f = auto sub (code c = auto sub () { return n; }) { return c(); };
    return f();
}

auto sub shadowed() {
    int n = 1;
    code f = auto sub (code c = auto sub () { return n; }) { return c(); };
    {
        int n = 4;
        n++;
        return f();
    }
}

auto sub in_closure() {
    code outer = auto sub () {
        int n = 2;
        code f = auto sub (code c = auto sub () { return n; }) { return c(); };
        {
            int n = 6;
            n++;
            return f();
        }
    };
    return outer();
}

auto sub with_default(code c = auto sub (code d = auto sub () { return 10; }) { return d(); }) {
    return c();
}

class C {
    public {
        code member = auto sub (code c = auto sub () { return 11; }) { return c(); };
    }

    auto method() {
        code f = auto sub (code c = auto sub () { return 12; }) { return c(); };
        return f();
    }
}

string sub probes() {
    C obj();
    string rv;
    for (int i = 0; i < 8; ++i) {
        top_value = 5 + i;
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y\n", i, top(), nested(), in_function(),
            with_default(), obj.member(), obj.method(), top(auto sub () { return 13; }), captures(20),
            top_capture() - i, enum_default(), shadowed(), in_closure());
    }
    return rv;
}
