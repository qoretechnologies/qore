# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Probes for declarations in the code of an expression (issue #6524); see expression-declarations.qtest and
# expression-declarations-modes.qtest.  probes() returns the result of each probe, one per line.

%modern
%no-child-restrictions

#! the declarations, after code that leaves the expression, that an expression refuses
const ExpressionDeclarations = (
    ("function", "1); int sub f() { return 7; }\n#"),
    ("function in a namespace", "1); int sub N0::f() { return 7; }\n#"),
    ("class", "1); class C { }\n#"),
    ("hashdecl", "1); hashdecl H { int a; }\n#"),
    ("enum", "1); enum E { A = 1 }\n#"),
    ("typedef", "1); typedef T = int;\n#"),
    ("typedef in a namespace", "1); typedef N0::T = int;\n#"),
    ("constant", "1); const X = 1;\n#"),
    ("constant in a namespace", "1); const N0::X = 1;\n#"),
    ("method", "1); int C0::m() { return 1; }\n#"),
    ("namespace", "1); namespace N { const Y = 1; }\n#"),
    ("global variable", "1); our int g = 1;\n#"),
    ("thread-local variable", "1); thread_local int t;\n#"),
    ("global variable list", "1); our (int ga, int gb);\n#"),
    ("thread-local variable list", "1); thread_local (int ta, int tb);\n#"),
    ("global variable list in the expression", "our (int gc, int gd) = (1, 2)"),
    ("function variant", "1); int sub f0(int i) { return i; }\n#"),
    ("namespace member", "1); namespace N0 { const Z = 1; }\n#"),
    ("method variant", "1); int C0::n(int i) { return i; }\n#"),
    ("function and constant", "1); int sub f() { return 7; } const X = (1"),
    ("function after a comment", "1 /* ) */); int sub f() { return 7; }\n#"),
    ("function after a string", "\")\"); int sub f() { return 7; }\n#"),
    ("function on lines of their own", "1\n)\n;\nint sub f() {\n    return 7;\n}\nconst X = (\n1"),
    ("function after a valid declaration", "1); const X = 1; int sub f() { return 7; }\n#"),
);

#! the code of expressions that is valid: parentheses and declarations in comments and strings, and closures
const ValidExpressions = (
    ("comment", "1 /* ); int sub f() { return 7; } const X = (1 */ + 1"),
    ("line comment", "1 # ); int sub f() { return 7; }\n+ 2"),
    ("string", "\"); int sub f() { return 7; } const X = (1\""),
    ("closure", "(int sub () { return 7; })()"),
    ("parentheses", "((1) + (2))"),
);

#! returns the Program that the expressions are created in
Program sub expression_program() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_REPARSE);
    p.parse("namespace N0 {} class C0 { int n() { return 1; } } int sub f0() { return 1; }", "base");
    return p;
}

#! returns the result of creating an expression: "refused", "created", or the error
string sub expression_result(Program p, string code) {
    try {
        Expression e(p, code, "probe");
        return sprintf("created: %y", e.eval());
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "PARSE-EXCEPTION" && ex.desc =~ /cannot be declared in the code of an expression/) {
            return "refused";
        }
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

#! returns the result of each probe, one per line
string sub probes() {
    list<string> rv = ();
    Program p = expression_program();
    foreach list<string> probe in (ExpressionDeclarations) {
        push rv, sprintf("%s: %s", probe[0], expression_result(p, probe[1]));
    }
    # nothing that the refused expressions declared is found by a later expression
    foreach string code in ("f()", "X", "N0::f()", "N0::X", "new C0().m()", "N::Y", "g", "t", "new C()",
            "<H>{}", "E::A", "cast<T>(1)", "ga", "ta", "gc", "f0(2)", "N0::Z", "new C0().n(2)") {
        push rv, sprintf("later %s: %s", code, expression_result(p, code));
    }
    # nothing is pending or committed: the same names can be declared in a later parse
    try {
        p.parse("int sub f() { return 1; } const X = 2; class C {} hashdecl H { int a; } enum E { A = 1 } "
            "typedef T = int; namespace N { const Y = 3; } our int g = 4; thread_local int t; "
            "namespace N0 { const X = 5; typedef T = int; int sub f() { return 6; } }", "after");
        push rv, sprintf("later declarations: %y", (p.callFunction("f"), p.callFunction("N0::f")));
    } catch (hash<ExceptionInfo> ex) {
        push rv, sprintf("later declarations: %s: %s", ex.err, ex.desc);
    }
    foreach list<string> probe in (ValidExpressions) {
        push rv, sprintf("valid %s: %s", probe[0], expression_result(p, probe[1]));
    }

    # an expression created before a refused one still runs
    Program p2 = expression_program();
    Expression e(p2, "f0() + 1", "before");
    push rv, sprintf("refused after a valid expression: %s", expression_result(p2, "1); our (int a, int b);\n#"));
    push rv, sprintf("valid expression after a refused one: %y", e.eval());

    # an implicit global variable in a Program without PO_REQUIRE_OUR
    Program p3(PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_REPARSE);
    push rv, sprintf("implicit global variable: %s", expression_result(p3, "$zz = 5"));
    push rv, sprintf("later implicit global variable: %s", expression_result(p3, "$zz"));
    return rv.join("\n");
}
