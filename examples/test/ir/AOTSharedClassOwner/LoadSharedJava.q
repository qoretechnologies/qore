%modern
# Copyright 2026 Qore Technologies, s.r.o.
# loads the modules given, in their order, and calls the methods of SharedJavaA and SharedJavaB, which call a method
# of the same Java class (see AOTSharedClassOwner.qtest)
string code;
foreach string name in (ARGV) {
    code += sprintf("%%requires %s\n", name);
}
code += "printf(\"%d %d\\n\", SharedJavaA::Lists::add((\"a\",)), SharedJavaB::Lists::add((\"b\", \"c\")));\n";
Program p(PO_NEW_STYLE);
p.parse(code, "calls");
p.run();
