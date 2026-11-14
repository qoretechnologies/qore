# probes of foreach-enum-element-release.qtest (qore#6091): a foreach loop releases the reference to its declaration
# that each enum element holds when its iteration ends; probes() returns one line per pass, so that the code runs on
# every tier with tiered execution

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
    return parts.join(" ");
}
