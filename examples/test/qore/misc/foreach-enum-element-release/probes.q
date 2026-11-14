# probes of foreach-enum-element-release.qtest (qore#6091): a foreach loop releases the reference to its declaration
# that each enum element holds when its iteration ends, as native code does for an enum value read through a reference
# local; probes() returns one line per pass, so that the code runs on every tier with tiered execution

%modern

enum E { One = 1, Two = 2 }
enum S : string { A = "a" }

class Holder {
    private {
        list<auto> values;
    }

    string run() {
        values = (1, E::One, S::A);
        string str;
        foreach auto value in (values) {
            str += sprintf("%y", value == 1);
        }
        return str;
    }
}

string sub literal_list() {
    string str;
    foreach auto value in ((E::One, S::A)) {
        str += sprintf("%y", value);
    }
    return str;
}

string sub enum_list(list<auto> l) {
    string str;
    foreach auto value in (l) {
        str += sprintf("%y", value);
    }
    return str;
}

string sub enum_only_list() {
    string str;
    foreach E value in ((E::One, E::Two, E::One)) {
        str += sprintf("%y", value);
    }
    return str;
}

#! reads an enum value through a reference local four times in one statement
/** Native code keeps the value read at a site and the one read before it until the end of the statement, and
    releases the older one at the next read: it released only a node there, not an enum value (qore#6091)
*/
int sub reference_read() {
    auto x = E::One;
    auto r = \x;
    return foldl $1 + (r == E::One ? 1 : 0), (0, 1, 2, 3, 4);
}

#! returns the number of references to the declaration of E
/** In a function of its own, so that the call releases the enum value that it passes before it returns, in every
    mode
*/
int sub e_refs() {
    return dbg_enum_decl_reference_count(E::One);
}

#! runs a probe a number of times and returns its result and the references to E that the runs did not release
/** The probe runs first so that compiled code and caches take the references that they keep, then the references are
    counted around further runs, which must release all that they take
*/
string sub leaked(string name, code probe) {
    string result;
    for (int i = 0; i < 3; ++i) {
        result = probe();
    }
    int before = e_refs();
    for (int i = 0; i < 10; ++i) {
        result = probe();
    }
    return sprintf("%s=%s/%d", name, result, e_refs() - before);
}

string sub probes() {
    list<string> parts;
    Holder h();
    parts += leaked("member", \h.run());
    parts += leaked("literal", \literal_list());
    parts += leaked("argument", string sub () { return enum_list((E::Two, "x", E::One, S::A, 2)); });
    parts += leaked("enum-only", \enum_only_list());
    parts += leaked("reference-read", string sub () { return sprintf("%d", reference_read()); });
    return parts.join(" ");
}
