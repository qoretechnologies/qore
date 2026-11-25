%modern
%allow-debugger
# the body locals of a method that are passed to a call of a method of self (m(x), Base::m(x)) are not kept on the
# runtime stack of local variables by compiled code (issue #7267): get_local_vars(1) in the callee reports them as
# unavailable in a program compiled with qcc (see "Local Variables of AOT-Compiled Code" in the language reference);
# its output is SelfCallLocals.expected

class Base {
    # the locals of the calling frame that the probe reports
    string probe(auto v) {
        hash<auto> lv = get_local_vars(1);
        return sprintf("%y: %s", v, (map sprintf("%s=%s", $1.key, exists $1.value.value ? sprintf("%y", $1.value.value)
            : ($1.value.unavailable ? "unavailable" : "nothing")), lv.pairIterator(), $1.key =~ /^(i|s)$/).join(", "));
    }
}

class Derived inherits Base {
    string runUnqualified() {
        int i = 5;
        i += 1;
        return probe(i);
    }

    string runQualifiedInt() {
        int i = 5;
        i += 1;
        return Base::probe(i);
    }

    string runString() {
        string s = "a";
        s += "b";
        return probe(s);
    }
}

Derived d();
printf("%s\n", d.runUnqualified());
printf("%s\n", d.runQualifiedInt());
printf("%s\n", d.runString());
