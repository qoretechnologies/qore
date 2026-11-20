# Copyright 2026 Qore Technologies, s.r.o.
%modern
%requires SourceText

string dash = binary_to_string(<e28094>, "UTF-8");
sub report(string value, string expected) {
    printf("%s %d %y\n", value.encoding(), length(value), value == expected);
}
report(SourceText::LongDash, dash + dash + dash);
report(SourceText::dash(), dash);
report(SourceText::Latin, binary_to_string(<e9>, "ISO-8859-1"));
