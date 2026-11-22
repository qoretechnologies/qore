%modern

# declarations that name a type T when no type T exists are parse errors, also when they are first used in a generic
# function whose type parameter is named T; each source is parsed in a Program of its own
const Sources = (
    "hashdecl": "hashdecl H { *T m; } auto sub f<T>(T a) { hash<H> h(); h.m = a; return h.m; } f<int>(1);",
    "static": "class K { public { static *T sv; } } auto sub f<T>(T a) { K::sv = a; return K::sv; } f<int>(1);",
    "class member": "class K { public { *T m; } } auto sub f<T>(T a) { K k(); k.m = a; return k.m; } f<int>(1);",
    "class constant": "class K { const C = cast<*T>(NOTHING); } auto sub f<T>(T a) { return K::C; } f<int>(1);",
    "namespace constant": "const NC = cast<*T>(NOTHING); auto sub f<T>(T a) { return NC; } f<int>(1);",
);

sub check() {
    foreach hash<auto> i in (Sources.pairIterator()) {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
        try {
            p.parse(i.value, i.key);
            printf("%s: no error\n", i.key);
        } catch (hash<ExceptionInfo> ex) {
            # the message without the suggestions of similar names
            printf("%s: %s: %s\n", i.key, ex.err, (ex.desc =~ x/^([^;]*)/)[0]);
        }
    }
}

check();
