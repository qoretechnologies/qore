%modern
# default values whose text contains the markers of the serialized binding of the arguments of a named call: the
# binding is written before the reference of the call (between two 0x01 bytes), so the text of the reference is never
# taken for a binding (an AOT program could not resolve such a call); see compiled-overload-errors.qtest
string sub f(string s = "a\nargnames:1\nx") { return sprintf("f(%y)", s); }
string sub g(int count, string label, string s = "b\nargmap:2:1,0") {
    return sprintf("g(%y, %y, %y)", count, label, s);
}
string sub h(int count, string label, string s = "c\nargnames:2\na\nb") {
    return sprintf("h(%y, %y, %y)", count, label, s);
}
string sub h(string label, float count) { return sprintf("h2(%y, %y)", label, count); }
printf("%s\n", f());
printf("%s\n", g(label: "l", count: 1));
printf("%s\n", g(1, "l"));
auto i = 3;
printf("%s\n", h(label: "l", count: i));
printf("%s\n", h(label: "l", count: 1.5));
