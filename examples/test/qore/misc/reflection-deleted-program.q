#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# driver of reflection-deleted-program.qtest: prints the result of each case on a line of its own

%modern
%requires reflection

# the declarations of the Program that is deleted: hashdecls and classes whose member initializers are code of it
const SrcA = "
public hashdecl H { int a = 1; string b = sprintf('%d', 2); string c = f(); }
public hashdecl HL { int a = 1; string s = 'lit'; }
public string sub f() { return 'x'; }
public class C { public { string m = sprintf('m'); string m2 = f(); int n = 1; } }
public class D { public { string m = sprintf('m'); } constructor() { } }
public class L { public { int a = 1; } }
public hashdecl EK { string s = f(); }
public hashdecl EL { list<hash<EK>> l; }
public hashdecl EM { hash<string, hash<EK>> m; }
hashdecl NP { string s = f(); }
public hashdecl PC inherits NP { int i = 1; }
public class S inherits Serializable { public { string v = 'x'; transient string t = f(); } }
hash<auto> sub mk() { return <H>{}; }
hash<auto> sub mkl() { return <HL>{}; }
hash<auto> sub mkel() { return <EL>{}; }
hash<auto> sub mkek() { return <EK>{}; }
binary sub ser() { S s(); return Serializable::serialize(s); }
Program sub mkB(string src) {
    Program b(PO_NEW_STYLE);
    b.parse(src, 'b');
    return b;
}
";

# the code of the Program that inherits the declarations of the deleted Program and lives on
const SrcB = "
hashdecl BH { string x = sprintf('y'); }
class E inherits C { public { string e = sprintf('e'); } }
hash<auto> sub mk() { return <H>{}; }
hash<auto> sub mkc(hash<auto> h) { return cast<hash<H>>(h); }
hash<auto> sub mkbh() { return <BH>{}; }
object sub mkC() { return new C(); }
object sub mkD() { return new D(); }
object sub mkE() { return new E(); }
int sub mkL() { return (new L()).a; }
hash<auto> sub mkelem() { return <EL>{\"l\": ({},)}; }
hash<auto> sub mkhelem() { return <EM>{\"m\": {\"a\": {}}}; }
hash<auto> sub mkpar() { return <PC>{}; }
auto sub dsr(binary d) { return Serializable::deserialize(d).t; }
";

hash<auto> h;
hash<auto> hl;
hash<auto> el;
hash<auto> ek;
binary ser;
Program b;
{
    Program a(PO_NEW_STYLE);
    a.parse(SrcA, "a");
    h = a.callFunction("mk");
    hl = a.callFunction("mkl");
    el = a.callFunction("mkel");
    # keeps the hashdecl of the elements, which only the deleted Program held
    ek = a.callFunction("mkek");
    ser = a.callFunction("ser");
    b = a.callFunction("mkB", SrcB);
    # the declarations work while their Program exists
    printf("before: %y %y %y %y\n", b.callFunction("mk"), b.callFunction("mkE").e,
        Type::getType(h).getTypedHash().doCast({}), TypedHash::forName(b, "H").getMember("c").getDefaultValue());
    delete a;
}

TypedHash th = Type::getType(h).getTypedHash();
TypedHash thl = Type::getType(hl).getTypedHash();

#! the cases: each one runs code of the deleted Program, unless it says otherwise
hash<string, code> cases = {
    "value typed hash doCast": auto sub () { return th.doCast({}); },
    "value typed hash doCastWithOverrides": auto sub () { return th.doCastWithOverrides({}, {"a": 2}); },
    "value typed hash doCastValues": auto sub () { return th.doCastValues({"k": {}}); },
    "value typed hash doCastValuesWithOverrides": auto sub () {
        return th.doCastValuesWithOverrides({"k": {}}, {"k": {"a": 2}});
    },
    "value typed hash member default with a builtin call": auto sub () {
        return th.getMember("b").getDefaultValue();
    },
    "value typed hash member default with a user call": auto sub () {
        return th.getMember("c").getDefaultValue();
    },
    # no code runs: no exception
    "value typed hash literal member default": auto sub () { return th.getMember("a").getDefaultValue(); },
    "value typed hash doCast with every member given": auto sub () {
        return th.doCast({"a": 5, "b": "b", "c": "c"});
    },
    "value literal typed hash doCast": auto sub () { return thl.doCast({}); },
    # the declarations inherited by a Program that lives on
    "inherited hashdecl initialization": auto sub () { return b.callFunction("mk"); },
    "inherited hashdecl cast": auto sub () { return b.callFunction("mkc", {"a": 2}); },
    "inherited class without a constructor": auto sub () { return b.callFunction("mkC"); },
    "inherited class with a constructor": auto sub () { return b.callFunction("mkD"); },
    "subclass of an inherited class": auto sub () { return b.callFunction("mkE"); },
    "inherited reflection typed hash doCast": auto sub () { return TypedHash::forName(b, "H").doCast({}); },
    "inherited reflection typed hash member default": auto sub () {
        return TypedHash::forName(b, "H").getMember("b").getDefaultValue();
    },
    "inherited reflection class newObject": auto sub () { return Class::forName(b, "C").newObject(); },
    "inherited reflection class member default": auto sub () {
        return Class::forName(b, "C").getMember("m").getDefaultValue();
    },
    # no code of the deleted Program runs: no exception
    "inherited reflection class literal member default": auto sub () {
        return Class::forName(b, "C").getMember("n").getDefaultValue();
    },
    "inherited class with literal members": auto sub () { return b.callFunction("mkL"); },
    "own hashdecl of the inheriting Program": auto sub () { return b.callFunction("mkbh"); },
    # the element of a list of another Program's hashdecl is converted, which runs its initializers
    "value typed hash doCast with a list element": auto sub () {
        return Type::getType(el).getTypedHash().doCast({"l": ({},)});
    },
    "inherited hashdecl with a list element": auto sub () { return b.callFunction("mkelem"); },
    "inherited hashdecl with a hash element": auto sub () { return b.callFunction("mkhelem"); },
    # a non-public parent of a public hashdecl is not copied to the inheriting Program
    "inherited hashdecl with a non-public parent": auto sub () { return b.callFunction("mkpar"); },
    # deserialization initializes the transient members without running the constructor
    "transient member of a deserialized object": auto sub () { return b.callFunction("dsr", ser); },
};

foreach hash<auto> i in (cases.pairIterator()) {
    try {
        printf("%s: %y\n", i.key, i.value());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s%s\n", i.key, ex.err, ex.desc =~ /has already been deleted/ ? "" : " (" + ex.desc + ")");
    }
}
printf("done\n");
