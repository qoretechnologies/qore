# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the classes whose signature strings class-signature-golden.qtest pins (class-signature-golden.expected); parsed in a
# Program of their own.  The methods of a class are in the signature in the order of their names, the variants of a
# method in the order of their declarations (qore#7458).  The declared types of members and static variables and the
# declared return types of methods are in the signature with namespace paths (qore#7457), not a return type implied
# by %require-types; the synchronized flag is not, as it is not part of the type of a class, like the code of a method

%new-style
%require-types
%strict-args

hashdecl SigHashDecl {
    int a = 1;
    *string b;
}

class SigRoot {
}

namespace SigA {
    enum SigColor {
        Red = 1,
        Green = 2,
    }

    #! only the namespace path
    class Empty {
    }

    #! members with every kind of type and access
    class Members {
        public {
            int i = 1;
            *string os;
            softint si;
            list<int> li;
            hash<string, softint> hss;
            hash<SigHashDecl> hd;
            *SigA::Empty oe;
            object<SigA::Empty> oo;
            union<int, string> u;
            code c;
            SigColor col = SigColor::Red;
            auto a;
            *list<hash<string, list<*float>>> deep;
        }

        private {
            float pf = 1.5;
        }

        private:internal {
            number pin = 1n;
        }
    }

    #! static members and constants
    class Statics {
        public {
            static int si = 2;
            static *list<string> sls;
            const C1 = "x";
            const C2 = 2;
        }

        private {
            static float spf = 1.0;
            const C3 = (1, 2);
        }

        private:internal {
            const C4 = {"a": 1};
        }
    }

    #! method variants with defaults, varargs, references and access modifiers
    class Methods {
        string m() {
            return "";
        }

        private string m(int a, *string b = "x") {
            return "";
        }

        private:internal int m(reference<list<int>> r) {
            return 0;
        }

        auto m(code c, ...) {
        }

        synchronized *SigA::Empty m(hash<SigHashDecl> h, object o) {
        }

        #! no declared return type: the nothing that %require-types implies is not in the signature (qore#7457)
        m(float f) {
        }

        static list<int> s(hash<string, int> h, *SigA::Empty e, softlist<string> l = ()) {
            return ();
        }

        private static nothing s(union<int, bool> v) {
        }
    }

    #! several method names, declared out of the order of their names, with the special methods (qore#7458)
    class ManyMethods {
        constructor() {
        }

        destructor() {
        }

        copy() {
        }

        int zeta() {
            return 0;
        }

        string alpha(int a) {
            return "";
        }

        string alpha() {
            return "";
        }

        *int mid(string s) {
        }

        private beta() {
        }

        static int szeta() {
            return 0;
        }

        static string salpha() {
            return "";
        }

        private static smid(int i) {
        }

        memberGate(string m) {
        }
    }

    #! a constructor
    class Ctor {
        constructor(int i, string s = "a") {
        }

        private constructor() {
        }
    }

    #! abstract variants
    class Abs {
        abstract string m(int a);
        abstract private *int m(string s, ...);
    }

    #! public, private and private:internal parent classes
    class Child inherits Members, private Statics, private:internal Empty {
        public {
            int child = 1;
        }
    }

    #! a generic class
    class Box<T> {
        public {
            *T v;
        }
    }

    #! a generic parent class
    class IntBox inherits Box<int> {
    }

    #! a generic parent class with a class type argument (named with its namespace path, qore#7457)
    class EmptyBox inherits Box<Empty> {
    }

    #! a class whose static variable initializer initializes its subclass while the class is initialized: the
    #! subclass's signature has the hash of the parent class, not its class ID (qore#7457)
    class ReBase {
        public {
            static *ReBase inst = new ReSub();
        }
    }

    class ReSub inherits ReBase {
    }

    namespace SigB {
        #! a nested namespace
        class Nested {
        }
    }
}

#! a scoped declaration
class SigA::Scoped {
    public {
        int x;
    }
}

#! a scoped declaration in a nested namespace
class SigA::SigB::ScopedNested {
}

#! the classes pinned, in the order of the golden file
const ClassPaths = (
    "SigRoot",
    "SigA::Empty",
    "SigA::Members",
    "SigA::Statics",
    "SigA::Methods",
    "SigA::ManyMethods",
    "SigA::Ctor",
    "SigA::Abs",
    "SigA::Child",
    "SigA::Box",
    "SigA::IntBox",
    "SigA::EmptyBox",
    "SigA::ReBase",
    "SigA::ReSub",
    "SigA::SigB::Nested",
    "SigA::Scoped",
    "SigA::SigB::ScopedNested"
);

#! returns the signature parts and hash of every class in ClassPaths
list<hash<auto>> sub get_signatures() {
    list<hash<auto>> rv = ();
    foreach string path in (ClassPaths) {
        hash<auto> sig = call_function("dbg_class_signature", path);
        rv += {"path": path} + sig;
    }
    return rv;
}
