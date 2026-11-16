# -*- mode: qore; indent-tabs-mode: nil -*-
# probes of generic-call-parse-type.qtest (qore#5896): a generic call resolved at run time whose type parameter has a
# bound that converts no value has the type of the bound, which parse-time checks use

%modern

#! the error that parsing the code gives, or "OK" if none
string sub generic_call_parse_type_parse(string code) {
    Program p(PO_NEW_STYLE | PO_STRICT_TYPES);
    try {
        p.parse(code, "generic-call-parse-type");
        return "OK";
    } catch (hash<ExceptionInfo> ex) {
        # the type of the call, without the location of the error
        return ex.err + ": " + (ex.desc =~ x/(type '[^']*'|is [a-z<>]+[^ ]*)$/)[0];
    }
}

string sub probes() {
    list<string> l = (
        generic_call_parse_type_parse("X sub bounded<X: int>(X x) { return x; } "
            "string sub s(auto a) { return bounded(a); }"),
        generic_call_parse_type_parse("X sub bounded<X: int>(X x) { return x; } "
            "sub s(auto a) { list<string> l = (bounded(a),); }"),
        # a bound that converts values gives the call no type
        generic_call_parse_type_parse("X sub soft_bounded<X: softint>(X x) { return x; } "
            "string sub s(auto a) { return soft_bounded(a); }"),
        generic_call_parse_type_parse("X sub identity<X>(X x) { return x; } "
            "string sub s(auto a) { return identity<int>(a); }"),
    );
    return foldl $1 + "|" + $2, l;
}
