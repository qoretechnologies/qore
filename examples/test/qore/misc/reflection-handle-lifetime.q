#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# driver of reflection-handle-lifetime.qtest: prints the result of each case on a line of its own

%modern
%requires reflection

# the declarations of a Program that is deleted; only the reflection handles keep them afterwards
const Src = "
public hashdecl H { int a = 1; string s = 'x'; }
public class C { public { int a = 1; } }
public enum E { A = 1, B = 2 }
auto sub mke() { return E::A; }
hash<auto> sub mk() { return <H>{}; }
list<auto> sub mkl() { list<hash<H>> l = (<H>{},); return l; }
object sub mkc() { return new C(); }
";

hash<auto> h;
list<auto> l;
object o;
auto e;
{
    Program p(PO_NEW_STYLE);
    p.parse(Src, "child");
    h = p.callFunction("mk");
    l = p.callFunction("mkl");
    o = p.callFunction("mkc");
    e = p.callFunction("mke");
    delete p;
}

# the handles are got from the values, which are then removed: the handles are the only holders of the declarations
TypedHash th = Type::getType(h).getTypedHash();
TypedHashMember thm = th.getMember("a");
Type t = Type::getType(h);
Type tl = Type::getType(l);
Type tc = Type::getType(o);
Type te = Type::getType(e);
remove h;
remove l;
remove o;
remove e;

hash<string, code> cases = {
    "typed hash name": auto sub () { return th.getName(); },
    "typed hash path": auto sub () { return th.getPathName(); },
    "typed hash members": auto sub () { return map $1.getName(), th.getMembers(); },
    "typed hash doCast": auto sub () { return th.doCast({}); },
    "typed hash member name": auto sub () { return thm.getName(); },
    "typed hash member type": auto sub () { return thm.getType().getName(); },
    "typed hash member default": auto sub () { return thm.getDefaultValue(); },
    "hashdecl type name": auto sub () { return t.getName(); },
    "hashdecl type typed hash": auto sub () { return t.getTypedHash().getName(); },
    "list of hashdecl type name": auto sub () { return tl.getName(); },
    "class type name": auto sub () { return tc.getName(); },
    "enum type name": auto sub () { return te.getName(); },
};

foreach hash<auto> i in (cases.pairIterator()) {
    try {
        printf("%s: %y\n", i.key, i.value());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", i.key, ex.err, ex.desc);
    }
}
printf("done\n");
