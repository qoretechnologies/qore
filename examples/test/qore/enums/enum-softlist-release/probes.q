# probes of enum-softlist-release.qtest (qore#6355): a soft list type that wraps a single enum value in a list keeps the
# reference to its declaration that the value holds; each probe returns the values it gives and, in a build with
# library debugging, the references to the declaration that ten more runs of it took or released (0 when balanced)

%modern

enum Color : int { Red = 1, Green = 2 }
enum Name : string { A = "a" }

hashdecl HK {
    softlist sl;
    *softlist osl;
}

string sub local_softlist_auto() {
    softlist<auto> l = Color::Red;
    return sprintf("%y", l);
}

string sub local_or_nothing_softlist_auto() {
    *softlist<auto> l = Color::Green;
    return sprintf("%y", l);
}

string sub local_softlist() {
    auto v = Name::A;
    softlist l = v;
    return sprintf("%y", l);
}

string sub local_or_nothing_softlist() {
    auto v = Color::Red;
    *softlist l = v;
    return sprintf("%y", l);
}

string sub param_softlist_auto(softlist<auto> l) {
    return sprintf("%y", l);
}

string sub hashdecl_member() {
    auto v = Color::Red;
    hash<HK> h();
    h.sl = v;
    h.osl = v;
    return sprintf("%y %y", h.sl, h.osl);
}

string sub hashdecl_cast() {
    auto v = Color::Green;
    hash<HK> h = cast<hash<HK>>({"sl": v, "osl": v});
    return sprintf("%y %y", h.sl, h.osl);
}

#! returns the number of references to the declaration of Color, or -1 without library debugging
/** In a function of its own, so that the call releases the enum value that it passes before it returns
*/
int sub color_refs() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_enum_decl_reference_count", Color::Red) : -1;
}

int sub name_refs() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_enum_decl_reference_count", Name::A) : -1;
}

#! runs a probe, then ten more times, and returns its result and the references that the ten runs did not balance
string sub balance(string name, code probe) {
    string result = probe();
    int c = color_refs();
    int n = name_refs();
    for (int i = 0; i < 10; ++i) {
        result = probe();
    }
    return Option::HAVE_LIBRARY_DEBUGGING
        ? sprintf("%s=%s/%d", name, result, (color_refs() - c) + (name_refs() - n))
        : sprintf("%s=%s", name, result);
}

string sub probes() {
    list<string> parts;
    parts += balance("softlist-auto", \local_softlist_auto());
    parts += balance("or-nothing-softlist-auto", \local_or_nothing_softlist_auto());
    parts += balance("softlist", \local_softlist());
    parts += balance("or-nothing-softlist", \local_or_nothing_softlist());
    parts += balance("param", string sub () { return param_softlist_auto(Color::Green); });
    parts += balance("hashdecl-member", \hashdecl_member());
    parts += balance("hashdecl-cast", \hashdecl_cast());
    return parts.join(" ");
}
