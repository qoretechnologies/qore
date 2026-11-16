#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The character offsets of <string>::getUnicode(), rindex(), <string>::rfind(), index() and <string>::find() mean the
# same in every encoding: the results for a text in UTF-8, ISO-8859-1, IBM037 and Shift_JIS (characters decoded with
# iconv), UTF-16, UTF-32 and UCS-2 are the ones that a reference implementation over the list of the text's code
# points gives, at every offset from before the start to after the end (where getUnicode() gives 0 and reads no byte
# after the string).  Prints one line per text and encoding with the number of checks, followed by any mismatch,
# then the results of the functions for strings with invalid or truncated characters, which raise INVALID-ENCODING
# for the offsets that reach them.

%modern

#! the texts as lists of code points: one- and two-byte UTF-8 characters, a character outside the BMP (a surrogate
#! pair in UTF-16 and four bytes in UTF-8), and repeated characters so that a backwards search finds the last match
const Texts = {
    "abc": (0x61, 0x62, 0x63),
    "latin1": (0x61, 0xe9, 0x7a, 0x61, 0xe9, 0x62),
    "astral": (0x61, 0xe9, 0x1f600, 0x7a, 0x61, 0xe9, 0x1f600, 0x62),
    # double-byte characters in Shift_JIS
    "kanji": (0x65e5, 0x672c, 0x61, 0x65e5, 0x672c, 0x62),
    "empty": (),
};

const Encodings = ("UTF-8", "ISO-8859-1", "IBM037", "SHIFT_JIS", "UTF-16", "UTF-16LE", "UTF-16BE", "UTF-32",
    "UTF-32LE", "UTF-32BE", "UCS-2", "UCS-2LE", "UCS-2BE");

#! strings with an invalid or truncated last character: the data and its encoding
const Invalid = (
    # an odd number of bytes in UTF-16
    ("UTF-16LE", <610062>),
    ("UTF-16BE", <006162>),
    # a high surrogate without its low surrogate
    ("UTF-16LE", <61003dd8>),
    ("UTF-16BE", <0061d83d>),
    # a truncated surrogate pair
    ("UTF-16LE", <61003dd800>),
    # a truncated two-byte UTF-8 character and an invalid byte
    ("UTF-8", <61c3>),
    ("UTF-8", <61ff62>),
    # a truncated UTF-32 character
    ("UTF-32LE", <61000000620000>),
);

# returns the UTF-8 string for a list of code points
string sub text(list<int> cps) {
    string rv = "";
    map rv += $1.toUnicode(), cps;
    return rv;
}

# true if the encoding can hold every code point
bool sub fits(string enc, list<int> cps) {
    try {
        return convert_encoding(convert_encoding(text(cps), enc), "UTF-8") == text(cps);
    } catch (hash<ExceptionInfo> ex) {
        return False;
    }
}

int sub ref_get_unicode(list<int> cps, int offset) {
    int n = cps.size();
    if (offset < 0) {
        offset += n;
        if (offset < 0) {
            offset = 0;
        }
    }
    return offset < n ? cps[offset] : 0;
}

bool sub matches_at(list<int> cps, list<int> needle, int i) {
    for (int j = 0; j < needle.size(); ++j) {
        if (cps[i + j] != needle[j]) {
            return False;
        }
    }
    return True;
}

# the last match that lies in the characters 0 .. pos inclusive
int sub ref_rindex(list<int> cps, list<int> needle, int pos) {
    int n = cps.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            return -1;
        }
    }
    if (pos >= n) {
        pos = n - 1;
    }
    for (int i = pos - needle.size() + 1; i >= 0; --i) {
        if (matches_at(cps, needle, i)) {
            return i;
        }
    }
    return -1;
}

# the first match that starts at pos or after it
int sub ref_index(list<int> cps, list<int> needle, int pos) {
    int n = cps.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    for (int i = pos; i + needle.size() <= n; ++i) {
        if (matches_at(cps, needle, i)) {
            return i;
        }
    }
    return -1;
}

# the needles: every substring of one, two and three characters and one that the text does not contain
list<list<int>> sub needles(list<int> cps) {
    list<list<int>> rv = ();
    for (int l = 1; l <= 3; ++l) {
        for (int i = 0; i + l <= cps.size(); ++i) {
            rv += (cps[i..(i + l - 1)],);
        }
    }
    rv += ((0x71,),);
    return rv;
}

# the result of a call, or the code of the exception that it raises
string sub result(code c) {
    try {
        return sprintf("%d", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

int errors = 0;
foreach hash<auto> t in (Texts.pairIterator()) {
    list<int> cps = t.value;
    int n = cps.size();
    foreach string enc in (Encodings) {
        if (!fits(enc, cps)) {
            continue;
        }
        string str = convert_encoding(text(cps), enc);
        int checks = 0;
        list<string> mismatches = ();
        for (int offset = -n - 2; offset <= n + 2; ++offset) {
            string exp = sprintf("%d", ref_get_unicode(cps, offset));
            string got = result(int sub () { return str.getUnicode(offset); });
            ++checks;
            if (got != exp) {
                mismatches += sprintf("getUnicode(%d): %s expected %s", offset, got, exp);
            }
        }
        foreach list<int> needle in (needles(cps)) {
            string ns = convert_encoding(text(needle), enc);
            for (int pos = -n - 2; pos <= n + 2; ++pos) {
                int exp = ref_rindex(cps, needle, pos);
                int got = rindex(str, ns, pos);
                int got_rfind = str.rfind(ns, pos);
                checks += 2;
                if (got != exp || got_rfind != exp) {
                    mismatches += sprintf("rindex(%y, %d): %d rfind(): %d expected %d", text(needle), pos, got,
                        got_rfind, exp);
                }
                exp = ref_index(cps, needle, pos);
                got = index(str, ns, pos);
                int got_find = str.find(ns, pos);
                checks += 2;
                if (got != exp || got_find != exp) {
                    mismatches += sprintf("index(%y, %d): %d find(): %d expected %d", text(needle), pos, got,
                        got_find, exp);
                }
            }
        }
        printf("%s %s: %d checks, %d mismatches\n", t.key, enc, checks, mismatches.size());
        map printf("  %s\n", $1), mismatches;
        errors += mismatches.size();
    }
}


foreach list<auto> i in (Invalid) {
    string enc = i[0];
    string str = binary_to_string(i[1], enc);
    string a = convert_encoding("a", enc);
    string b = convert_encoding("b", enc);
    list<string> res = ();
    for (int offset = -3; offset <= 3; ++offset) {
        res += sprintf("%d: %s %s %s", offset, result(int sub () { return str.getUnicode(offset); }),
            result(int sub () { return rindex(str, a, offset); }), result(int sub () { return index(str, b, offset); }));
    }
    printf("invalid %s %s: %s\n", enc, i[1].toHex(), res.join(", "));
}

printf("%s\n", errors ? "FAIL" : "PASS");
