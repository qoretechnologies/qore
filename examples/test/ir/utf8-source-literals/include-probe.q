# Copyright 2026 Qore Technologies, s.r.o.
%modern
Program pgm(PO_NEW_STYLE);
string source = convert_encoding(sprintf("%%include %s\nstring sub outside() { return '", ARGV[0]), "UTF-8")
    + binary_to_string(<e28094>, "UTF-8") + "'; }";
source += sprintf("\nstring sub joined() { return 'x'\n%%include %s\n'z'; }", ARGV[1]);
pgm.parse(source, "caller");
foreach string name in ("inside", "outside", "joined") {
    string s = pgm.callFunction(name);
    printf("%s %d\n", s.encoding(), length(s));
    if (name == "joined") {
        printf("%y\n", s == binary_to_string(<78c3a97a>, "UTF-8"));
    }
}
