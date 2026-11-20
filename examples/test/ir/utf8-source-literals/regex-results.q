# Copyright 2026 Qore Technologies, s.r.o.
%modern

sub check(string label, string actual, string expected) {
    if (actual.encoding() != expected.encoding() || binary(actual) != binary(expected)) {
        throw "REGEX-ENCODING", sprintf("%s: %s %s, expected %s %s", label, actual.encoding(),
            binary(actual).toHex(), expected.encoding(), binary(expected).toHex());
    }
}

string latin = binary_to_string(<e9>, "ISO-8859-1");
string euro = binary_to_string(<e282ac>, "UTF-8");
foreach string s in (euro, latin) {
    string p = "^(?<letter>.)$";
    check("extract operator", (s =~ x/^(.)$/u)[0], s);
    check("named capture", regex_extract_named(s, p).letter, s);
    hash<RegexMatchInfo> m = regex_extract_detailed(s, p)[0];
    check("whole match", m.match, s);
    check("numbered group", m.groups[0], s);
    check("named group", m.named_groups.letter, s);
    Regex compiled(p);
    auto iter = compiled.matchIterator(s);
    if (!iter.next()) { throw "NO-MATCH", p; }
    check("iterator", iter.getNamedGroup("letter"), s);
    check("backreference", regex_subst(s, p, "$1"), s);
    string result = regex_subst(s, p, string sub (hash<RegexMatchInfo> match) {
        check("callback match", match.match, s);
        check("callback group", match.groups[0], s);
        check("callback named", match.named_groups.letter, s);
        return convert_encoding(latin, "UTF-8");
    });
    check("callback result", result, convert_encoding(latin, s.encoding()));
}
# PCRE2 encodes group names in UTF-8. Qore hash keys use the process encoding.
string named_pattern = "^(?<é>.)(?<à>z)?$";
Regex named_regex(named_pattern, RE_Unicode);
RegexExtract named_extract(named_pattern, RE_Unicode);
sub checkNames(hash<auto> groups, string expected) {
    string matched = binary_to_string(<e9>, "ISO-8859-1");
    string unmatched = binary_to_string(<e0>, "ISO-8859-1");
    list<binary> stored = map binary($1), keys groups;
    if (groups.size() != 2 || !inlist(binary(convert_encoding(matched, get_default_encoding())), stored)
            || !inlist(binary(convert_encoding(unmatched, get_default_encoding())), stored) || exists groups{unmatched}) {
        throw "REGEX-NAME", "lost the name of an unmatched group";
    }
    check("non-ASCII group name", groups{matched}, expected);
}
checkNames(regex_extract_named(latin, named_pattern, RE_Unicode), latin);
checkNames(named_regex.extractNamedGroups(latin), latin);
checkNames(named_extract.extractNamedGroups(latin), latin);
checkNames(regex_extract_detailed(latin, named_pattern, RE_Unicode)[0].named_groups, latin);
auto names = named_regex.matchIterator(latin);
string nul_name = binary_to_string(<c3a9007a>, "UTF-8");
try {
    @discard(names.getNamedGroup(nul_name));
    throw "MISSING-ITERATOR-ERROR";
} catch (hash<ExceptionInfo> ex) {
    if (ex.err != "INVALID-ITERATOR") { rethrow; }
}
if (!names.next()) { throw "NO-MATCH", named_pattern; }
if (exists names.getNamedGroup(nul_name)) { throw "REGEX-NAME", "truncated group name"; }
checkNames(names.getNamedGroups(), latin);
checkNames(names.getValue().named_groups, latin);
check("iterator name in Latin-1", names.getNamedGroup(latin), latin);
check("iterator name in UTF-16", names.getNamedGroup(convert_encoding(latin, "UTF-16BE")), latin);
if (get_default_encoding() == "ISO-8859-1") {
    try {
        @discard(regex_extract_named(latin, "(?<中>.)", RE_Unicode));
        throw "MISSING-HASH-KEY-ERROR";
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != "ENCODING-CONVERSION-ERROR") { rethrow; }
    }
}
check("Latin-1 callback replacement in UTF-8", regex_subst(euro, named_pattern,
        string sub (hash<RegexMatchInfo> m) {
            checkNames(m.named_groups, euro);
            return latin;
        }, RE_Unicode), convert_encoding(latin, "UTF-8"));
check("Latin-1 replacement", regex_subst(euro, ".", latin), convert_encoding(latin, "UTF-8"));
string subject = convert_encoding("a", "ISO-8859-1");
subject =~ s/a/é/;
check("source substitution", subject, latin);
foreach string encoding in ("UTF-8", "ISO-8859-1") {
    Program pgm(PO_NEW_STYLE);
    string code = convert_encoding("string sub replaceAccent(string s) { s =~ s/a/é/; return s; }", encoding);
    pgm.parse(code, "tagged-substitution");
    check("tagged Program substitution", pgm.callFunction("replaceAccent", convert_encoding("a", latin.encoding())), latin);
    check("tagged replacement on UTF-8 subject", pgm.callFunction("replaceAccent", convert_encoding("a", "UTF-8")),
        convert_encoding(latin, "UTF-8"));
}
check("Unicode case conversion", regex_subst(latin, ".", "\\u" + latin), binary_to_string(<c9>, "ISO-8859-1"));
check("Latin-1 octal byte", regex_subst(latin, ".", "\\351"), latin);
check("UTF-8 octal bytes", regex_subst(euro, ".", "\\342\\202\\254"), euro);
check("raw octal byte", regex_subst(euro, ".", "\\351"), binary_to_string(<e9>, "UTF-8"));
string z = convert_encoding("Z", latin.encoding());
string text = latin + z;
hash<RegexMatchInfo> detailed = regex_extract_detailed(text, "(Z)")[0];
auto zi = new Regex("(Z)").matchIterator(text);
if (!zi.next() || detailed.offset != 2 || zi.getOffset() != 2 || zi.getValue().offset != 2
        || zi.getGroupOffset(0) != 2) {
    throw "REGEX-OFFSET", "expected the UTF-8 byte offset 2 for Z";
}
check("callback offsets", regex_subst(text, "Z", string sub (hash<RegexMatchInfo> m) {
    if (m.offset != 2 || m.length != 1) { throw "REGEX-OFFSET", "callback offset"; }
    return m.match;
}), text);
auto lazy = text.splitLinesRegex("Z");
if (!lazy.next()) { throw "NO-SPLIT", "lazy split"; }
check("lazy split", lazy.getValue(), latin);
if (!lazy.next()) { throw "NO-SPLIT", "trailing empty piece"; }
check("lazy empty piece", lazy.getValue(), convert_encoding("", latin.encoding()));
check("eager split", text.splitRegex("Z")[0], latin);
string with_nul = binary_to_string(<e900e9>, latin.encoding());
string two = text + with_nul;
check("eager no match", with_nul.splitRegex("Z")[0], with_nul);
check("eager included separator", two.splitRegex("Z", 0, True)[0], text);
check("eager tail", two.splitRegex("Z")[1], with_nul);
check("limited split", two.splitRegex("Z", 0, False, 1)[0], two);
auto kept = two.splitLinesRegex("Z", 0, True);
if (!kept.next()) { throw "NO-SPLIT", "included separator"; }
check("lazy included separator", kept.getValue(), text);
if (!kept.next()) { throw "NO-SPLIT", "tail"; }
check("lazy tail", kept.getValue(), with_nul);
string delimiter = latin + convert_encoding("XYZ", latin.encoding()) + latin;
list<string> captured = delimiter.splitRegex("X(Y)Z");
if (captured.size() != 2) { throw "SPLIT-SIZE", captured.size(); }
check("capturing separator prefix", captured[0], latin);
check("capturing separator tail", captured[1], latin);
list<string> boundary = latin.splitRegex("(?=.)", 0, False, 2);
check("empty match tail", boundary[1], latin);
auto boundaries = latin.splitLinesRegex("(?=.)");
if (!boundaries.next() || !boundaries.next()) { throw "NO-SPLIT", "empty match tail"; }
check("lazy empty match tail", boundaries.getValue(), latin);

try {
    string unrepresentable = regex_subst(latin, ".", euro);
    throw "NO-CONVERSION-ERROR", unrepresentable;
} catch (hash<ExceptionInfo> ex) {
    if (ex.err != "ENCODING-CONVERSION-ERROR") { rethrow; }
}

# Some platforms do not provide this stateful converter.
bool has_jis = False;
try { has_jis = !!convert_encoding("A", "ISO-2022-JP"); }
catch (hash<ExceptionInfo> ex) {
    if (ex.err != "ENCODING-CONVERSION-ERROR") { rethrow; }
}
if (has_jis) {
    string shifted = binary_to_string(<1b284241>, "ISO-2022-JP");
    check("whole eager piece", shifted.splitRegex("z")[0], shifted);
    check("whole eager piece with separator", shifted.splitRegex("A", 0, True)[0], shifted);
    auto included = shifted.splitLinesRegex("A", 0, True);
    if (!included.next()) { throw "NO-SPLIT", "whole included separator"; }
    check("whole lazy piece with separator", included.getValue(), shifted);
    auto whole = shifted.splitLinesRegex("z");
    if (!whole.next()) { throw "NO-SPLIT", "whole stateful subject"; }
    check("whole lazy piece", whole.getValue(), shifted);
    check("no-match bytes", regex_subst(shifted, "z", "x"), shifted);
    check("no-match callback bytes", regex_subst(shifted, "z", string sub (hash<RegexMatchInfo> m) {
        throw "UNEXPECTED-MATCH", m;
    }), shifted);
}
printf("regex encodings passed\n");
