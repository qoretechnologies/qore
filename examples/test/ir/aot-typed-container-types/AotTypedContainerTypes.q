# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Typed container values of every type form keep their types in AOT code; see aot-typed-container-types.qtest

%modern
%requires ./TypeIdMod.qm
# a hashdecl and a class of the names of those of the module below, which are other types
hashdecl TypeIdSameHash {
    string z;
}
class TypeIdSameClass {
}
# a module whose init raises an exception with containers of its own types: those that the Program does not have, and
# those whose names are those of the Program's types, are written without the type, as the variable of the %try-module
# gives them
%try-module (fail_err) ./TypeIdFailMod.qm
%endtry
class Foo {
}
hashdecl HD {
    int a = 1;
    string b = "x";
}
typedef MyInt = int;
typedef FooList = list<object<Foo>>;
namespace Ns {
    class Bar {
    }
    hashdecl NsHash {
        int n = 0;
    }
}
const Values = {
    "hashdecl": <HD>{"a": 5},
    "ns hashdecl": <Ns::NsHash>{"n": 1},
    "list<object<Foo>>": cast<list<object<Foo>>>(()),
    "list<*object<Foo>>": cast<list<*object<Foo>>>((NOTHING,)),
    "hash<string, object<Foo>>": cast<hash<string, object<Foo>>>({}),
    "list<object<Ns::Bar>>": cast<list<object<Ns::Bar>>>(()),
    "list<hash<HD>>": cast<list<hash<HD>>>((<HD>{},)),
    "hash<string, hash<HD>>": cast<hash<string, hash<HD>>>({"x": <HD>{}}),
    "list<int>": cast<list<int>>((1, 2)),
    "list<*int>": cast<list<*int>>((1, NOTHING)),
    "hash<string, int>": cast<hash<string, int>>({"a": 1}),
    "list<MyInt>": cast<list<MyInt>>((1,)),
    "list<*MyInt>": cast<list<*MyInt>>((NOTHING,)),
    "FooList": cast<FooList>(()),
    "list<union<int, string>>": cast<list<union<int, string>>>((1, "a")),
    "list<*union<int, string>>": cast<list<*union<int, string>>>((1, NOTHING)),
    "hash<string, union<int, string>>": cast<hash<string, union<int, string>>>({"a": 1}),
    "list<list<int>>": cast<list<list<int>>>((cast<list<int>>((1,)),)),
    "list<hash<string, int>>": cast<list<hash<string, int>>>((cast<hash<string, int>>({"a": 1}),)),
    "list<object<TypeIdModClass>>": cast<list<object<TypeIdModClass>>>(()),
    "list<hash<TypeIdModHash>>": cast<list<hash<TypeIdModHash>>>((<TypeIdModHash>{},)),
    "list<TypeIdModInt>": cast<list<TypeIdModInt>>((3,)),
    "module hashdecl": <TypeIdModHash>{"a": 2},
};
# prints each value's type and that of its first element
sub show(hash<auto> values) {
    foreach hash<auto> i in (values.pairIterator()) {
        printf("%s: %s", i.key, i.value.fullType());
        if (i.value.typeCode() == NT_LIST && i.value) {
            printf(" [0]=%s", i.value[0].fullType());
        } else if (i.value.typeCode() == NT_HASH && i.value && i.key !~ /hashdecl/) {
            printf(" first=%s", i.value.firstValue().fullType());
        }
        printf("\n");
    }
}
show(Values);
foreach string k in (("lsame", "osame", "lonly", "honly", "oonly", "nested", "union")) {
    auto v = fail_err.arg{k};
    printf("%s: %s", k, v.fullType());
    if (v.typeCode() == NT_LIST && v) {
        printf(" [0]=%s", v[0].fullType());
    }
    printf(" %y\n", v);
}
