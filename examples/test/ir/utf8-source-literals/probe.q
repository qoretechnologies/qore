# Copyright 2026 Qore Technologies, s.r.o.
%modern

sub report(string value, string expected) {
    printf("%s %d %y\n", value.encoding(), length(value), value == expected);
}

string latin = binary_to_string(<e9>, "ISO-8859-1");
string dash = binary_to_string(<e28094>, "UTF-8");
Program pgm(PO_NEW_STYLE);
# The bytes of the expression are Latin-1, even in a UTF-8 process.
Expression expr(pgm, binary_to_string(<22e922>, "ISO-8859-1"), "latin1-expression");
report(expr.eval(), latin);

# A UTF-8 source string can express a character outside the process charset.
string source = convert_encoding("string sub text() { return '", "UTF-8") + dash + "'; }";
pgm.parse(source, "utf8-program");
report(pgm.callFunction("text"), dash);
Program pending(PO_NEW_STYLE);
pending.parsePending(convert_encoding("string sub pending() { return '", "ISO-8859-1") + latin + "'; }",
    "latin1-pending");
pending.parseCommit();
report(pending.callFunction("pending"), latin);

# The scanner decodes a source string with non-ASCII syntax bytes before lexing it.
Expression wide(pgm, convert_encoding(convert_encoding("'", "UTF-8") + dash + "'", "UTF-16LE"), "wide-expression");
report(wide.eval(), dash);

# Big5 encodes this character with 0x5c as its second byte.
Expression big5(pgm, binary_to_string(<22b35c22>, "BIG5"), "big5-expression");
report(big5.eval(), binary_to_string(<e8a8b1>, "UTF-8"));
Expression escaped(pgm, binary_to_string(<22615c75323061636222>, "ISO-8859-1"), "escaped-expression");
report(escaped.eval(), binary_to_string(<61e282ac62>, "UTF-8"));
Expression match_latin(pgm, binary_to_string(<27e927203d7e202fe92f>, "ISO-8859-1"), "regex-expression");
printf("%y\n", match_latin.eval());

# A backslash before a multibyte source character escapes the whole character.
Program trans(PO_NEW_STYLE | PO_ALLOW_BARE_REFS);
string operation = sprintf("string sub trans() { string s = '%s'; s =~ tr/\\%s/x/; return s; }", latin, latin);
trans.parse(convert_encoding(operation, "UTF-8"), "transliteration");
printf("%y\n", trans.callFunction("trans") == "x");
