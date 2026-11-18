%modern

# a reflection Type of a value whose type names a retired declaration references the declaration again; releasing
# the Type must leave it retired (qore#5790)

%requires reflection

const Opts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION;

sub try_print(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

hash<auto> sub make() {
    Program p(Opts);
    p.parse("class Foo { }
        hashdecl HD { int x = 1; }
        enum Color { Red = 1 }
        hash<auto> sub make() {
            list<*Foo> ol = (NOTHING,);
            list<*hash<HD>> hl = (NOTHING,);
            list<Color> cl = (Color::Red,);
            return {\"ol\": ol, \"hl\": hl, \"cl\": cl};
        }", "child");
    return p.callFunction("make");
}

hash<auto> r = make();
foreach string key in (("ol", "hl", "cl")) {
    {
        Type t = Type::getType(r{key});
        printf("%s: %s\n", key, t.getName());
        # the element type, whose declaration is retired
        Type et = t.getElementType();
        printf("%s element: %s\n", key, et.getName());
    }
    try_print(key + " += NOTHING", auto sub () { r{key} += NOTHING; return r{key}.size(); });
    try_print(key + " fullType", auto sub () { return r{key}.fullType(); });
}
