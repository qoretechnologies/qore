#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The byte positions of bindex() and brindex() and the character positions of index() and rindex() mean the same as
# each other and in every encoding (issue #6271): for texts in ISO-8859-1, UTF-8 and UTF-16LE, the results are the
# ones that a reference implementation over the text's bytes (or characters) gives, at every position from before
# the start to after the end.  brindex() finds a match that lies within the bytes up to and including the position,
# as rindex() does with characters, and a negative position before the start is the start for bindex() and index()
# and gives no match for brindex() and rindex().  Prints one line per text and encoding with the number of checks,
# followed by any mismatch.

%modern

#! the texts: ASCII, two-byte characters in UTF-8, a character outside the BMP, repeated so that a backwards search
#! finds the last match
const Texts = {
    "ascii": "abcabd",
    "latin1": "aézaéb",
    "astral": "a😀bé😀b",
};

const Encodings = ("ISO-8859-1", "UTF-8", "UTF-16LE");

# true if the encoding can hold the text
bool sub fits(string enc, string text) {
    try {
        return convert_encoding(convert_encoding(text, enc), "UTF-8") == text;
    } catch (hash<ExceptionInfo> ex) {
        return False;
    }
}

# the characters of a UTF-8 text
list<string> sub chars(string text) {
    list<string> rv = ();
    for (int i = 0; i < text.length(); ++i) {
        rv += text.substr(i, 1);
    }
    return rv;
}

# the needles: every substring of one and two characters and one that the text does not contain
list<string> sub needles(list<string> cs) {
    list<string> rv = ();
    for (int l = 1; l <= 2; ++l) {
        for (int i = 0; i + l <= cs.size(); ++i) {
            string n = cs[i..(i + l - 1)].join("");
            if (!rv.contains(n)) {
                rv += n;
            }
        }
    }
    rv += "q";
    return rv;
}

# the byte offsets of the characters of a text in an encoding (the boundaries where a match can start)
list<int> sub boundaries(list<string> cs, string enc) {
    list<int> rv = ();
    int off = 0;
    foreach string c in (cs) {
        rv += off;
        off += convert_encoding(c, enc).size();
    }
    return rv;
}

int sub ref_bindex(binary hay, list<int> starts, binary needle, int pos) {
    int n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    foreach int i in (starts) {
        if (i >= pos && i + needle.size() <= n && hay.substr(i, needle.size()) == needle) {
            return i;
        }
    }
    return -1;
}

int sub ref_brindex(binary hay, list<int> starts, binary needle, int pos) {
    int n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            return -1;
        }
    }
    if (pos >= n) {
        pos = n - 1;
    }
    for (int j = starts.size() - 1; j >= 0; --j) {
        int i = starts[j];
        if (i + needle.size() - 1 <= pos && hay.substr(i, needle.size()) == needle) {
            return i;
        }
    }
    return -1;
}

int sub ref_index(list<string> cs, list<string> needle, int pos) {
    int n = cs.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    for (int i = pos; i + needle.size() <= n; ++i) {
        if (cs[i..(i + needle.size() - 1)] == needle) {
            return i;
        }
    }
    return -1;
}

int sub ref_rindex(list<string> cs, list<string> needle, int pos) {
    int n = cs.size();
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
        if (cs[i..(i + needle.size() - 1)] == needle) {
            return i;
        }
    }
    return -1;
}

int errors = 0;
foreach hash<auto> t in (Texts.pairIterator()) {
    list<string> cs = chars(t.value);
    foreach string enc in (Encodings) {
        if (!fits(enc, t.value)) {
            continue;
        }
        string str = convert_encoding(t.value, enc);
        binary hay = binary(str);
        list<int> starts = boundaries(cs, enc);
        int bytes = hay.size();
        int n = cs.size();
        int checks = 0;
        list<string> mismatches = ();
        foreach string needle in (needles(cs)) {
            string ns = convert_encoding(needle, enc);
            binary nb = binary(ns);
            list<string> ncs = chars(needle);
            for (int pos = -bytes - 2; pos <= bytes + 2; ++pos) {
                int exp = ref_bindex(hay, starts, nb, pos);
                int got = bindex(str, ns, pos);
                ++checks;
                if (got != exp) {
                    mismatches += sprintf("bindex(%y, %d): %d expected %d", needle, pos, got, exp);
                }
                exp = ref_brindex(hay, starts, nb, pos);
                got = brindex(str, ns, pos);
                ++checks;
                if (got != exp) {
                    mismatches += sprintf("brindex(%y, %d): %d expected %d", needle, pos, got, exp);
                }
            }
            for (int pos = -n - 2; pos <= n + 2; ++pos) {
                int exp = ref_index(cs, ncs, pos);
                int got = index(str, ns, pos);
                ++checks;
                if (got != exp) {
                    mismatches += sprintf("index(%y, %d): %d expected %d", needle, pos, got, exp);
                }
                exp = ref_rindex(cs, ncs, pos);
                got = rindex(str, ns, pos);
                ++checks;
                if (got != exp) {
                    mismatches += sprintf("rindex(%y, %d): %d expected %d", needle, pos, got, exp);
                }
            }
        }
        printf("%s %s: %d checks, %d mismatches\n", t.key, enc, checks, mismatches.size());
        map printf("  %s\n", $1), mismatches;
        errors += mismatches.size();
    }
}
printf("%s\n", errors ? "FAIL" : "PASS");
