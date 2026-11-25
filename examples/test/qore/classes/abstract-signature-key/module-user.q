#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# declares the abstract declarations of the AbstractSignatureKey module's classes again in classes of its own and
# inherits both; the test runs it with the module's source and with a qcc -m .qmod and compares the output with
# module-user.expected

%modern
%requires AbstractSignatureKey

# the same signatures with other parameter names, and with other names of the same types
class JInt { abstract string m(int x); }
class JOptStr { abstract string m(*string x); }
class JListInt { abstract string m(ASK::IntList x); }
class JTypedef { abstract string m(list<int> x); }
class JHashSI { abstract string m(hash<string, int> x); }
class JObjFoo { abstract string m(object<ASK::Foo> x); }
class JFoo { abstract string m(ASK::Foo x); }
class JHashDecl { abstract string m(hash<ASK::H> x); }
class JLater { abstract string m(ASK::Later x); }
class JSoftList { abstract string m(softlist<string> x); }
class JCode { abstract string m(code x); }
class JTwo { abstract string m(int x, *string y); }
class JUnion { abstract string m(*union<ASK::Foo, int> x); }

# other signatures
class DStr { abstract string m(string s); }
class DReqStr { abstract string m(string s); }
class DListStr { abstract string m(list<string> l); }
class DHash { abstract string m(hash<auto> h); }
class DOne { abstract string m(int i); }

# each class inheriting both has one abstract variant
class VInt inherits ASK::IInt, JInt {}
class VOptStr inherits JOptStr, ASK::IOptStr {}
class VListInt inherits ASK::IListInt, JListInt {}
class VTypedef inherits ASK::ITypedef, JTypedef {}
class VHashSI inherits JHashSI, ASK::IHashSI {}
class VObjFoo inherits ASK::IObjFoo, JObjFoo {}
class VFoo inherits ASK::IFoo, JFoo {}
class VHashDecl inherits ASK::IHashDecl, JHashDecl {}
class VLater inherits JLater, ASK::ILater {}
class VSoftList inherits ASK::ISoftList, JSoftList {}
class VCode inherits ASK::ICode, JCode {}
class VTwo inherits ASK::ITwo, JTwo {}
class VUnion inherits JUnion, ASK::IUnion {}
# the list<int> declarations under both names
class VLists inherits ASK::IListInt, ASK::ITypedef, JListInt, JTypedef {}

# and two with different signatures
class XStr inherits ASK::IInt, DStr {}
class XReqStr inherits ASK::IOptStr, DReqStr {}
class XListStr inherits ASK::IListInt, DListStr {}
class XHash inherits ASK::IHashSI, DHash {}
class XTwo inherits ASK::ITwo, DOne {}

# a method of the class implements the declarations of both
class WInt inherits ASK::IInt, JInt { string m(int v) { return "WInt::m(int)"; } }
class WOptStr inherits ASK::IOptStr, JOptStr { string m(*string v) { return "WOptStr::m(*string)"; } }
class WListInt inherits ASK::IListInt, JListInt { string m(list<int> v) { return "WListInt::m(list<int>)"; } }
class WTypedef inherits ASK::ITypedef, JTypedef { string m(ASK::IntList v) { return "WTypedef::m(IntList)"; } }
class WHashSI inherits ASK::IHashSI, JHashSI { string m(hash<string, int> v) { return "WHashSI::m(hash)"; } }
class WObjFoo inherits ASK::IObjFoo, JObjFoo { string m(object<ASK::Foo> v) { return "WObjFoo::m(Foo)"; } }
class WFoo inherits ASK::IFoo, JFoo { string m(ASK::Foo v) { return "WFoo::m(Foo)"; } }
class WHashDecl inherits ASK::IHashDecl, JHashDecl { string m(hash<ASK::H> v) { return "WHashDecl::m(hash<H>)"; } }
class WLater inherits ASK::ILater, JLater { string m(ASK::Later v) { return "WLater::m(Later)"; } }
class WSoftList inherits ASK::ISoftList, JSoftList { string m(softlist<string> v) { return "WSoftList::m"; } }
class WCode inherits ASK::ICode, JCode { string m(code v) { return "WCode::m(code)"; } }
class WTwo inherits ASK::ITwo, JTwo { string m(int v, *string w) { return "WTwo::m(int, *string)"; } }
class WBox inherits ASK::IBox<int>, JInt { string m(int v) { return "WBox::m(int)"; } }
class WUnion inherits ASK::IUnion, JUnion { string m(*union<ASK::Foo, int> v) { return "WUnion::m"; } }

# the access of a privately inherited implementation is that of the declarations, whose classes are reached publicly
# or privately (qore#7288)
class K { string m(int i) { return "K::m(int)"; } }
class SStr { string m(string s) { return "SStr::m(string)"; } }
class U1 inherits private JInt, ASK::I1, private K, SStr {}
class U2 inherits ASK::I1, private JInt, private K, SStr {}
class U3 inherits private ASK::M, JInt, SStr {}
class U4 inherits JInt, private ASK::M, SStr {}
class U5 inherits private JInt, ASK::I1, private ASK::M, SStr {}
class U6 inherits private ASK::I1, JInt, private K, SStr {}
class U7 inherits private ASK::I1, private JInt, private K, SStr {}

hash<string, auto> args = {
    "Int": 1,
    "OptStr": "s",
    "ListInt": (1, 2),
    "Typedef": (1, 2),
    "HashSI": {"a": 1},
    "ObjFoo": new ASK::Foo(),
    "Foo": new ASK::Foo(),
    "HashDecl": hash<ASK::H>{},
    "Later": new ASK::Later(),
    "SoftList": "s",
    "Code": sub () {},
    "Two": 1,
    "Box": 1,
    "Union": 1,
};

foreach string c in (("VInt", "VOptStr", "VListInt", "VTypedef", "VHashSI", "VObjFoo", "VFoo", "VHashDecl", "VLater",
        "VSoftList", "VCode", "VTwo", "VUnion", "VLists", "XStr", "XReqStr", "XListStr", "XHash", "XTwo")) {
    try {
        create_object(c);
        printf("%s: instantiated\n", c);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", c, ex.err, ex.desc);
    }
}

foreach hash<auto> i in (args.pairIterator()) {
    string c = "W" + i.key;
    try {
        object o = create_object(c);
        printf("%s: %s\n", c, o.m(i.value));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", c, ex.err, ex.desc);
    }
}

foreach string c in (("U1", "U2", "U3", "U4", "U5", "U6", "U7")) {
    try {
        object o = create_object(c);
        printf("%s: m(1) from outside: %s\n", c, o.m(1));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: m(1) from outside: %s: %s\n", c, ex.err, ex.desc);
    }
}
