# probes of empty-string-order.qtest (qore#6090): an empty string sorts before every other string, with the
# comparison operators, <=>, sort(), sort_descending(), min() and max(), for typed and untyped values and for
# strings in different encodings; probes() returns one line per pass, so that the code runs on every tier with tiered
# execution

%modern

#! the comparison operators and <=> with string-typed operands
string sub typed_ops(string l, string r) {
    return sprintf("%s%s%s%s%d", l < r ? "T" : "F", l <= r ? "T" : "F", l > r ? "T" : "F", l >= r ? "T" : "F",
        l <=> r);
}

#! the comparison operators and <=> with operands of any type
string sub auto_ops(auto l, auto r) {
    return sprintf("%s%s%s%s%d", l < r ? "T" : "F", l <= r ? "T" : "F", l > r ? "T" : "F", l >= r ? "T" : "F",
        l <=> r);
}

string sub pairs(code ops) {
    list<string> l;
    foreach list<string> p in ((("", "a"), ("a", ""), ("", ""), ("a", "b"), ("ab", "a"))) {
        l += ops(p[0], p[1]);
    }
    return l.join(",");
}

string sub probes() {
    list<string> parts;
    parts += pairs(\typed_ops());
    parts += pairs(\auto_ops());
    # an empty string in another encoding
    string latin_empty = convert_encoding("", "ISO-8859-1");
    string latin_a = convert_encoding("a", "ISO-8859-1");
    parts += sprintf("%s,%s", typed_ops(latin_empty, "a"), auto_ops("a", latin_empty));
    parts += sprintf("%s,%s", typed_ops("", latin_a), auto_ops(latin_a, ""));
    list<string> words = ("b", "", "a", "", "ab");
    parts += sprintf("%y", sort(words));
    parts += sprintf("%y", sort_descending(words));
    parts += sprintf("%y", sort(words, int sub (string l, string r) { return l <=> r; }));
    parts += sprintf("%y", sort_stable(words));
    parts += sprintf("%y %y", min(words), max(words));
    return parts.join("; ");
}
