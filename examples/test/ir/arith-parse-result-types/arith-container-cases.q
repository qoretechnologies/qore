#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the container and typed cases of part 5 of ArithParseResultTypes.qc: each case builds a list with a map expression,
# whose element type is the parse result type of the operator in every execution mode (the AST interpreter too), over a
# typed list, and prints the list with its full type and the full type of its first element: a parse result type that
# does not allow a value raises RUNTIME-TYPE-ERROR, and a container type that the rules of an operator keep (ex:
# list<int> for list<int> + int) shows in the full types; the output of every execution mode must be that of the AST
# interpreter

%modern
# binary + string is an invalid operation, which the parser warns about
%disable-warning invalid-operation

hashdecl D {
    int x = 1;
    string s = "a";
}

enum Pr : int {
    Low = 0,
    High = 10,
}

enum St : string {
    A = "a",
}

#! members initialized with enum values, which keep the enum type
class EnumMembers {
    public {
        int m = Pr::High;
        string s = St::A;
    }
}

hashdecl EnumHash {
    int x = Pr::High;
}

sub show(string label, code<list<auto>()> c) {
    try {
        list<auto> l = c();
        printf("%s => %y %s %s\n", label, l, l.fullType(), l ? l[0].fullType() : "-");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

sub showArgs(string label, code c, softlist<auto> args) {
    try {
        list<auto> l = call_function_args(c, args);
        printf("%s => %y %s %s\n", label, l, l.fullType(), l ? l[0].fullType() : "-");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

list<auto> sub litListList(softlist<int> a, softlist<int> b) { return (a + b,); }
list<auto> sub litListListSet(softlist<int> a, softlist<int> b) { auto q = (a + b,); q[0] = 5; return q; }
list<auto> sub litSoftListInt(softlist<int> a, int b) { return (a + b,); }
list<auto> sub litSoftListIntSet(softlist<int> a, int b) { auto q = (a + b,); q[0] = 5; return q; }
list<auto> sub litSoftPlainList(softlist a, list b) { return (a + b,); }
list<auto> sub litIntStringLists(list<int> a, list<string> b) { return (a + b,); }
list<auto> sub litIntLists(list<int> a, list<int> b) { return (a + b,); }
list<auto> sub litOrNothingIntListInt(list<*int> a, int b) { return (a + b,); }
list<auto> sub litIntIntList(int a, list<int> b) { return (a + b,); }
list<auto> sub litPlainListInt(list a, int b) { return (a + b,); }
list<auto> sub litEnumMemberList(EnumMembers o, list<Pr> b) { return (o.m + b,); }
list<auto> sub litListEnumMember(EnumMembers o, list<Pr> b) { return (b + o.m,); }
list<auto> sub litStringEnumMemberList(EnumMembers o, list<St> b) { return (o.s + b,); }
list<auto> sub litEnumHashMemberList(hash<EnumHash> h, list<Pr> b) { return (h.x + b,); }
list<auto> sub litEnumMemberIntList(EnumMembers o, list<int> b) { return (o.m + b,); }
list<auto> sub litEnumMemberOrNothingIntList(EnumMembers o, list<*int> b) { return (o.m + b,); }
list<auto> sub litOrNothingIntListEnumMember(EnumMembers o, list<*int> b) { return (b + o.m,); }
list<auto> sub litOrNothingStringListEnumMember(EnumMembers o, list<*string> b) { return (b + o.s,); }
# the result of list<softint> + int has no parse-time type (it depends on the values), so its assignment to an int
# parses and raises an error at runtime
list<auto> sub litSoftIntListIntToInt(list<softint> l) { int x = l + 1; return (x,); }

# containers: the type that the rules of the operator keep
sub containers() {
    list<list<int>> li = ((1, 2), (3,));
    show("list<int> + int", list<auto> sub () { return map $1 + 4, li; });
    show("int + list<int>", list<auto> sub () { return map 4 + $1, li; });
    show("list<int> + list<int>", list<auto> sub () { return map $1 + (5, 6), li; });
    list<int> lx = (7,);
    show("list<int> + list<int> (variable)", list<auto> sub () { return map $1 + lx, li; });
    list<string> ls = ("a",);
    show("list<int> + list<string>", list<auto> sub () { return map $1 + ls, li; });
    show("list<int> + string", list<auto> sub () { return map $1 + "a", li; });

    list<hash<D>> lh = (<D>{}, <D>{"x": 2});
    hash<D> d = <D>{"x": 3};
    show("hash<D> + hash<D>", list<auto> sub () { return map $1 + d, lh; });
    show("hash<D> - string", list<auto> sub () { return map $1 - "x", lh; });
    list<string> ks = ("s",);
    show("hash<D> - list<string>", list<auto> sub () { return map $1 - ks, lh; });

    list<hash<string, int>> lhi = ({"a": 1}, {"b": 2});
    hash<string, int> hi = {"c": 3};
    show("hash<string, int> + hash<string, int>", list<auto> sub () { return map $1 + hi, lhi; });
    show("hash<string, int> - string", list<auto> sub () { return map $1 - "a", lhi; });
    hash<auto> ha = {"d": "x"};
    show("hash<string, int> + hash<auto>", list<auto> sub () { return map $1 + ha, lhi; });

    list<*hash<auto>> loh = ({"a": 1}, NOTHING);
    show("*hash<auto> + hash<D>", list<auto> sub () { return map $1 + d, loh; });
    show("hash<D> + *hash<auto>", list<auto> sub () { return map d + $1, loh; });
    list<*list<int>> lol = ((1,), NOTHING);
    show("*list<int> + int", list<auto> sub () { return map $1 + 2, lol; });

    # list literals of the result in named functions, whose parameters have the declared types (a list literal in a
    # closure takes its element type from the values in the IR mode): the element type of the literal is the parse
    # result type in the compiled modes, and an element of another type is refused by it
    softlist<int> sl = 1;
    showArgs("softlist<int> + softlist<int>", \litListList(), (sl, sl));
    showArgs("softlist<int> + softlist<int> (element assignment)", \litListListSet(), (sl, sl));
    showArgs("softlist<int> + int", \litSoftListInt(), (sl, 2));
    showArgs("softlist<int> + int (element assignment)", \litSoftListIntSet(), (sl, 2));
    softlist sl2 = 1;
    showArgs("softlist + list", \litSoftPlainList(), (sl2, (2,)));
    showArgs("list<int> + list<string> (literal)", \litIntStringLists(), ((1,), ("a",)));
    showArgs("list<int> + list<int> (literal)", \litIntLists(), ((1,), (2,)));
    showArgs("list<*int> + int (literal)", \litOrNothingIntListInt(), ((1,), 2));
    showArgs("int + list<int> (literal)", \litIntIntList(), (2, (1,)));
    showArgs("list + int (literal)", \litPlainListInt(), ((1,), 2));
    # an int or string member initialized with an enum value keeps the enum type, which the runtime takes
    EnumMembers em();
    list<Pr> lpr = (Pr::Low,);
    list<St> lst = (St::A,);
    showArgs("int member (enum value) + list<Pr>", \litEnumMemberList(), (em, lpr));
    showArgs("list<Pr> + int member (enum value)", \litListEnumMember(), (em, lpr));
    showArgs("string member (enum value) + list<St>", \litStringEnumMemberList(), (em, lst));
    showArgs("int hashdecl member (enum value) + list<Pr>", \litEnumHashMemberList(), (<EnumHash>{}, lpr));
    showArgs("int member (enum value) + list<int>", \litEnumMemberIntList(), (em, (1,)));
    showArgs("int member (enum value) + list<*int>", \litEnumMemberOrNothingIntList(), (em, (1, NOTHING)));
    showArgs("list<*int> + int member (enum value)", \litOrNothingIntListEnumMember(), (em, (1, NOTHING)));
    showArgs("list<*string> + string member (enum value)", \litOrNothingStringListEnumMember(), (em, ("x",)));
    list<softint> lsi1 = (1,);
    showArgs("list<softint> + int assigned to int", \litSoftIntListIntToInt(), (lsi1,));
}

# the AST interpreter applies the parse result type of a map expression (qore#5876)
sub unions() {
    list<union<int, float>> uf = (1, 2.5);
    show("union<int, float> + int", list<auto> sub () { return map $1 + 1, uf; });
    show("union<int, float> * float", list<auto> sub () { return map $1 * 2.0, uf; });
    show("union<int, float> / int", list<auto> sub () { return map $1 / 2, uf; });
    list<union<int, number>> un = (2, 2.5n);
    show("union<int, number> * float", list<auto> sub () { return map $1 * 2.5, un; });
    show("float - union<int, number>", list<auto> sub () { return map 2.5 - $1, un; });
    list<*float> onf = (2.5, NOTHING);
    show("*float + int", list<auto> sub () { return map $1 + 2, onf; });
    show("*float / int", list<auto> sub () { return map $1 / 2, onf; });
    list<*int> oni = (3, NOTHING);
    show("*int - float", list<auto> sub () { return map $1 - 0.5, oni; });
    show("*int / int", list<auto> sub () { return map $1 / 2, oni; });
    list<*number> onn = (2.5n, NOTHING);
    show("float * *number", list<auto> sub () { return map 2.0 * $1, onn; });
}

# soft, untyped and other operand types
sub others() {
    list<softint> lsi = (1, 2);
    show("softint + float", list<auto> sub () { return map $1 + 0.5, lsi; });
    list<softfloat> lsf = (1.5, 2.0);
    show("softfloat * int", list<auto> sub () { return map $1 * 2, lsf; });
    list<softnumber> lsn = (1.5n, 2n);
    show("softnumber - float", list<auto> sub () { return map $1 - 0.5, lsn; });
    list<*softint> losi = (1, NOTHING);
    show("*softint + int", list<auto> sub () { return map $1 + 1, losi; });

    list<auto> la = (1, 2.5, 3n, "4", 2s, True, NOTHING);
    show("auto + int", list<auto> sub () { return map $1 + 1, la; });
    show("auto * float", list<auto> sub () { return map $1 * 2.0, la; });
    show("auto / int", list<auto> sub () { return map $1 / 2, la; });
    list<any> ly = (1, 2.5, "x");
    show("any + string", list<auto> sub () { return map $1 + "z", ly; });

    list<timeout> lt = (1000, 2500);
    date dt = 2026-01-01T00:00:00Z;
    show("timeout + date", list<auto> sub () { return map $1 + dt, lt; });
    show("date + timeout", list<auto> sub () { return map dt + $1, lt; });
    show("date - timeout", list<auto> sub () { return map dt - $1, lt; });
    list<*timeout> lot = (1000, NOTHING);
    show("*timeout + date", list<auto> sub () { return map $1 + dt, lot; });

    list<date> ld = (1h, 2026-01-01T00:00:00Z);
    show("date * float", list<auto> sub () { return map $1 * 2.5, ld; });
    show("date + int", list<auto> sub () { return map $1 + 60, ld; });

    list<bool> lb = (True, False);
    show("bool + int", list<auto> sub () { return map $1 + 1, lb; });
    show("bool * float", list<auto> sub () { return map $1 * 1.5, lb; });

    list<binary> lbin = (<01>, <0203>);
    show("binary + binary", list<auto> sub () { return map $1 + <ff>, lbin; });
    show("binary + string", list<auto> sub () { return map $1 + "a", lbin; });

    # a string index gives a char value, which is an int in arithmetic
    string s = "ab";
    show("char + int", list<auto> sub () { return map s[$1] + 1, (0, 1); });
    show("char * float", list<auto> sub () { return map s[$1] * 0.5, (0, 1); });
    show("char + string", list<auto> sub () { return map s[$1] + "z", (0, 1); });
    show("char / int", list<auto> sub () { return map s[$1] / 2, (0, 1); });
}

#! a hashdecl that inherits D: a value of type hash<D> can be a hash<D2>
hashdecl D2 inherits D {
    int y = 2;
}

#! a class for the cases of object operands that can be NOTHING
class OptObj {
    public {
        int m = 1;
    }
}

#! a value of an optional hashdecl type minus keys keeps the hashdecl: the parse type of the result allows it
*hash<D> sub optionalHashdeclMinus(*hash<D> h) {
    return h - ("s",);
}

#! returns its argument with its optional type, which a local variable initialized with the value would not keep
*string sub optString(*string s) {
    return s;
}

#! returns its argument with its optional type
*hash<auto> sub optHash(*hash<auto> h) {
    return h;
}

#! returns its argument with its optional type
*hash<D> sub optHashD(*hash<D> h) {
    return h;
}

#! an optional hashdecl, hash, list or object operand of + or - gives the container type of the operand, as its value
#! does at runtime, or NOTHING: the parse type of the result is the container type or NOTHING, not the base type of
#! the container
sub optionalContainers() {
    list<*hash<D>> lod = (<D>{"x": 2}, NOTHING);
    hash<D> d = <D>{"x": 3};
    show("*hash<D> - list<string>", list<auto> sub () { return map $1 - ("s",), lod; });
    show("*hash<D> - string", list<auto> sub () { return map $1 - "s", lod; });
    show("*hash<D> - *string", list<auto> sub () { return map $1 - optString("s"), lod; });
    show("*hash<D> - list<string> (function)", list<auto> sub () { return map optionalHashdeclMinus($1), lod; });
    show("*hash<D> + hash<D>", list<auto> sub () { return map $1 + d, lod; });
    show("hash<D> + *hash<D>", list<auto> sub () { return map d + $1, lod; });
    show("*hash<D> + *hash<D>", list<auto> sub () { return map $1 + optHashD(d), lod; });
    show("*hash<D> + hash<auto>", list<auto> sub () { return map $1 + {"y": 1}, lod; });

    # a hash<D> value can be a hash<D2>, which a hash<D> minus a key also is
    list<*hash<D>> lod2 = (optHashD(<D2>{}), NOTHING);
    show("*hash<D> (hash<D2>) - string", list<auto> sub () { return map $1 - "s", lod2; });

    list<*hash<string, int>> loi = ({"a": 1, "b": 2}, NOTHING);
    hash<string, int> hi = {"c": 3};
    show("*hash<string, int> - string", list<auto> sub () { return map $1 - "a", loi; });
    show("*hash<string, int> + hash<string, int>", list<auto> sub () { return map $1 + hi, loi; });

    list<*list<int>> lol = ((1, 2), NOTHING);
    show("*list<int> + int", list<auto> sub () { return map $1 + 3, lol; });
    show("int + *list<int>", list<auto> sub () { return map 3 + $1, lol; });

    list<*OptObj> loo = (new OptObj(), NOTHING);
    show("*OptObj + *hash<auto> (NOTHING)", list<auto> sub () { return map $1 + optHash(), loo; });
    show("*OptObj + *hash<auto>", list<auto> sub () { return map $1 + optHash({"a": 1}), loo; });
    show("*OptObj - *string", list<auto> sub () { return map $1 - optString("s"), loo; });
    show("*OptObj - *string (NOTHING)", list<auto> sub () { return map $1 - optString(), loo; });
}

containers();
unions();
others();
optionalContainers();
