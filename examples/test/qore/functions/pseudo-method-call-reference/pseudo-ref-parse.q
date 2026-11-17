# issue #995: parse-time checks of call references to pseudo-methods; the output is the same in every execution mode
# and in programs compiled with qcc
%modern

#! parses the code in a new Program and prints the result
sub check(string label, string code) {
    Program p(PO_NEW_STYLE);
    try {
        p.parse(code, label);
        printf("%s: parsed\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

check("list size", "list<int> l = (1,); code c = \\l.size();");
check("unknown list pseudo-method", "list<int> l = (1,); code c = \\l.noSuchMethod();");
check("unknown string pseudo-method", "string s = \"a\"; code c = \\s.noSuchMethod();");
check("unknown int pseudo-method", "int i = 1; code c = \\i.keys();");
check("method of all values", "int i = 1; code c = \\i.typeCode();");
check("union with the pseudo-method", "softlist<int> v = (1,); code c = \\v.size();");
check("auto value", "auto a = 1; code c = \\a.noSuchMethod();");
check("object pseudo-method", "class C {} C o(); code c = \\o.className();");
check("unknown object method", "class C {} C o(); code c = \\o.noSuchMethod();");
check("pseudo-method of an object value", "object o = new Mutex(); code c = \\o.className();");
check("code member of a hash", "hash<auto> h = {}; code c = \\h.f();");
check("code member of an optional hash", "*hash<auto> h; code c = \\h.f();");
check("code member of an object", "class C { public { code f; } } C o(); code c = \\o.f();");
