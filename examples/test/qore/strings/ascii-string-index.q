# Copyright (C) 2026 Qore Technologies, s.r.o.
# Checks character offsets and lengths of UTF-8 strings after the mutations of the language (issue #6138): the results
# are compared with the same text in UTF-16LE, which does not use the ASCII prefix cache of UTF-8 strings, or with the
# results of the code without the cache for text that is not valid UTF-8
%modern

int checks = 0;

sub fail(string label, string msg) {
    throw "ASCII-INDEX-ERROR", sprintf("%s: %s", label, msg);
}

*string sub to_utf8(*string v) {
    return exists v ? convert_encoding(v, "UTF-8") : NOTHING;
}

sub expect_same(auto actual, auto expected, string label, string what) {
    ++checks;
    if (exists actual != exists expected || (exists actual && actual != expected)) {
        fail(label, sprintf("%s: got %y, expected %y", what, actual, expected));
    }
}

#! the character offsets of a check: forward, backward or strided; all offsets of a short string, fewer of a long one
list<int> sub offsets(int n, int order, bool sparse) {
    list<int> rv = ();
    if (!sparse && n <= 300) {
        for (int i = -n - 3; i < n + 3; ++i) {
            push rv, i;
        }
    } else {
        # the offsets at both ends (also from the end of the string) and out of range, and every stride-th offset
        int ends = sparse ? 5 : 12;
        int stride = sparse ? 211 : 89;
        hash<string, bool> seen = {};
        foreach int start in ((-n - 3, -ends, n - ends)) {
            for (int i = start; i < start + ends + 3; ++i) {
                seen{i} = True;
            }
        }
        for (int i = 0; i < ends; ++i) {
            seen{-n + i} = True;
            seen{i} = True;
        }
        for (int i = -n - (n % stride); i < n + 3; i += stride) {
            seen{i} = True;
        }
        rv = sort(map $1.toInt(), keys seen);
    }
    if (order == 1) {
        rv = reverse(rv);
    } else if (order == 2) {
        list<int> strided = ();
        for (int start = 0; start < 5; ++start) {
            for (int i = start; i < rv.size(); i += 5) {
                push strided, rv[(i * 3 + 1) % rv.size()];
            }
        }
        rv = strided;
    }
    return rv;
}

#! compares the character offsets of a UTF-8 string with the same text in UTF-16LE
sub check(string s, string label, int order = 0, bool sparse = False) {
    string u = convert_encoding(s, "UTF-16LE");
    int n = u.length();
    expect_same(s.length(), n, label, "length()");
    expect_same(length(s), n, label, "length(s)");
    foreach int i in (offsets(n, order, sparse)) {
        expect_same(s[i], to_utf8(u[i]), label, sprintf("[%d]", i));
        expect_same(s.substr(i, 1), to_utf8(u.substr(i, 1)), label, sprintf("substr(%d, 1)", i));
        expect_same(substr(s, i, 2), to_utf8(substr(u, i, 2)), label, sprintf("substr(%d, 2)", i));
        # substr() without a length and index() take time linear in the length of the string
        if (!(i % (n <= 300 ? 7 : 623))) {
            expect_same(s.substr(i), to_utf8(u.substr(i)), label, sprintf("substr(%d)", i));
            expect_same(s.substr(i, -1), to_utf8(u.substr(i, -1)), label, sprintf("substr(%d, -1)", i));
            # rindex() is checked by the native test: in UTF-16 it misses a needle that starts at the position
            foreach string needle in (("a", "é", "€", "Z")) {
                string un = convert_encoding(needle, "UTF-16LE");
                expect_same(index(s, needle, i), index(u, un, i), label, sprintf("index(%y, %d)", needle, i));
            }
        }
    }
}

#! fills the cache: the whole string, half of it or nothing; returns the size of the characters read
int sub prime(string s, int how) {
    if (how == 2) {
        return s.length() + (s[-1] ?? "").size();
    }
    if (how == 1) {
        return (s[s.size() / 2] ?? "").size();
    }
    return 0;
}

string ascii = "";
for (int i = 0; i < 40; ++i) {
    ascii += chr(ord("a") + i % 26);
}

hash<string, string> contents = {
    "ascii": ascii + "Z\n",
    "empty": "",
    "2-byte at start": "é" + ascii,
    "3-byte in middle": ascii.substr(0, 20) + "€" + ascii.substr(20),
    "4-byte at end": ascii + "😀",
    "mixed": "aéb€c😀" + ascii + "é",
};

#! the mutations of the language; each one adds text that is not ASCII where it can
hash<string, code> mutations = {
    "none": sub (reference<string> s) {},
    "append": sub (reference<string> s) { s += "é"; },
    "append-ascii": sub (reference<string> s) { s += "xyz"; },
    "append-list-element": sub (reference<string> s) { list<string> l = (s,); s = ""; l[0] += "€"; s = l[0]; },
    "prepend": sub (reference<string> s) { s = "é" + s; },
    "splice-tail": sub (reference<string> s) { splice s, 3; s += "é"; },
    "splice-range": sub (reference<string> s) { splice s, 1, 2; },
    "splice-negative": sub (reference<string> s) { splice s, -5, 2; },
    "splice-replace": sub (reference<string> s) { splice s, 1, 1, "éé"; },
    "splice-replace-shrink": sub (reference<string> s) { splice s, 0, 9, "é"; },
    "extract-tail": sub (reference<string> s) { extract s, 4; s += "é"; },
    "extract-range": sub (reference<string> s) { extract s, 2, 3; },
    "extract-replace": sub (reference<string> s) { extract s, 0, 2, "€"; },
    "regex-subst": sub (reference<string> s) { s =~ s/a/é/; },
    "regex-subst-global": sub (reference<string> s) { s =~ s/[ace]/é/g; },
    "transliteration": sub (reference<string> s) { s =~ tr/a-c/A-C/; s += "é"; },
    "chomp": sub (reference<string> s) { s += "\r\n"; chomp s; s += "é"; },
    "trim": sub (reference<string> s) { s = " \t" + s + " \n"; trim s; s = "é" + s; },
    "trim-unicode": sub (reference<string> s) { s = "  " + s + "  "; s = trim(s, "  "); },
    "uppercase": sub (reference<string> s) { s = ("é" + s).upr(); },
    "lowercase": sub (reference<string> s) { s = ("É" + s).lwr(); },
    "reverse": sub (reference<string> s) { s = reverse(s + "é"); },
    "replace": sub (reference<string> s) { s = replace(s, "b", "é"); },
    "sprintf": sub (reference<string> s) { s = sprintf("%s-%s", s, "é"); },
    "to-latin1-and-back": sub (reference<string> s) {
        # text with characters that ISO-8859-1 does not have is converted from the part that it has
        string latin1;
        try {
            latin1 = convert_encoding(s + "é", "ISO-8859-1");
        } catch (hash<ExceptionInfo> ex) {
            latin1 = convert_encoding("é" + ascii, "ISO-8859-1");
        }
        s = convert_encoding(latin1, "UTF-8");
    },
    "force-encoding": sub (reference<string> s) {
        s = force_encoding(force_encoding(s + "é", "ISO-8859-1"), "UTF-8");
    },
    "to-utf16-and-back": sub (reference<string> s) {
        s = convert_encoding(convert_encoding("€" + s, "UTF-16"), "UTF-8");
    },
    "shared-copy": sub (reference<string> s) { string t = s; t += "é"; s = t; },
    "assign": sub (reference<string> s) { s = "é" + "new"; },
};

foreach hash<auto> c in (contents.pairIterator()) {
    foreach hash<auto> m in (mutations.pairIterator()) {
        for (int how = 0; how < 3; ++how) {
            string label = sprintf("%s, %s, prime %d", m.key, c.key, how);
            string s = c.value;
            prime(s, how);
            m.value(\s);
            check(s, label);
            m.value(\s);
            check(s, label + ", twice", 2);
            s += "!";
            check(s, label + ", appended", 1, True);
        }
    }
}

# views: a substring of more than 4096 bytes refers to the bytes of its parent; the reference results in UTF-16 take
# time linear in the length of the string, so the views are checked with the mutations that change them in place
list<string> view_mutations = ("none", "append", "append-ascii", "prepend", "splice-tail", "splice-range",
    "splice-replace", "splice-replace-shrink", "extract-replace", "regex-subst", "chomp", "trim", "shared-copy",
    "assign");
foreach string key in (("ascii", "2-byte at start", "3-byte in middle")) {
    string body = contents{key};
    while (body.size() < 4200) {
        body += "abcdefghij";
    }
    foreach bool middle in ((False, True)) {
        foreach bool prime_parent in ((False, True)) {
            foreach string mutation in (view_mutations) {
                string label = sprintf("%s, %s view, %s%s", mutation, middle ? "middle" : "end", key,
                    prime_parent ? ", parent primed" : "");
                # the expected text and the parent are separate strings
                string parent_text = sprintf("prefix%s%s", body, middle ? "suffix" : "");
                string parent = sprintf("prefix%s%s", body, middle ? "suffix" : "");
                if (prime_parent) {
                    prime(parent, 2);
                }
                string view = middle ? parent.substr(6, body.length()) : parent.substr(6);
                if (!middle) {
                    check(view, label + ", unchanged", 2, True);
                }
                mutations{mutation}(\view);
                check(view, label);
                view += "!";
                check(view, label + ", appended", 1, True);
                expect_same(parent, parent_text, label, "parent");
                check(parent, label + ", parent", 2, True);
            }
        }
    }
}

# text that is not valid UTF-8: the offsets before the invalid bytes are characters, the others raise exceptions as
# without the cache; the length counts each byte that does not start a complete character as one character
# (qore#6597)
sub check_invalid(string s, int valid_chars, int length, string label) {
    for (int how = 0; how < 3; ++how) {
        # reading invalid text fills the cache up to the invalid bytes before it raises the exception
        try {
            prime(s, how);
        } catch (hash<ExceptionInfo> ex) {
            expect_same(ex.err, "INVALID-ENCODING", label, "prime");
        }
        for (int pass = 0; pass < 2; ++pass) {
            expect_same(s.length(), length, label, "length()");
            for (int i = -s.size() - 2; i < s.size() + 2; ++i) {
                ++checks;
                try {
                    *string c = s[i];
                    if (i < 0 || i >= valid_chars) {
                        fail(label, sprintf("[%d]: got %y, expected an exception", i, c));
                    }
                    expect_same(c, chr(ord("a") + i % 26), label, sprintf("[%d]", i));
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "INVALID-ENCODING") {
                        rethrow;
                    }
                    if (i >= 0 && i < valid_chars) {
                        fail(label, sprintf("[%d]: unexpected exception: %s: %s", i, ex.err, ex.desc));
                    }
                    expect_same(ex.desc, "invalid UTF-8 encoding encountered in string", label, sprintf("[%d]", i));
                }
            }
        }
    }
}

{
    string a20 = ascii.substr(0, 20);
    check_invalid(binary_to_string(binary(a20) + <c3>, "UTF-8"), 20, 21, "truncated 2-byte at end");
    check_invalid(binary_to_string(binary(a20) + <e9> + binary("cd"), "UTF-8"), 20, 23, "invalid 3-byte in middle");
    check_invalid(binary_to_string(binary(a20) + <f09f98>, "UTF-8"), 20, 23, "truncated 4-byte at end");
    check_invalid(binary_to_string(<c3> + binary(a20), "UTF-8"), 0, 21, "invalid at start");
    # the bytes that RFC 3629 does not allow are invalid as the other invalid sequences are (qore#6604), each invalid
    # byte counting as one character in the length: a continuation byte on its own, an overlong form, a surrogate, a
    # code point above U+10FFFF and a byte that UTF-8 does not use
    check_invalid(binary_to_string(binary(a20) + <80> + binary("xy"), "UTF-8"), 20, 23, "continuation byte");
    check_invalid(binary_to_string(binary(a20) + <c0af> + binary("xy"), "UTF-8"), 20, 24, "overlong 2-byte");
    check_invalid(binary_to_string(binary(a20) + <e080af> + binary("xy"), "UTF-8"), 20, 25, "overlong 3-byte");
    check_invalid(binary_to_string(binary(a20) + <eda080> + binary("xy"), "UTF-8"), 20, 25, "surrogate");
    check_invalid(binary_to_string(binary(a20) + <f4908080> + binary("xy"), "UTF-8"), 20, 26, "above U+10FFFF");
    check_invalid(binary_to_string(binary(a20) + <f5808080> + binary("xy"), "UTF-8"), 20, 26, "byte F5");
}

# one string read from several threads at once, each thread in another order
{
    string text = "";
    for (int i = 0; i < 5000; ++i) {
        text += chr(ord("a") + i % 26);
    }
    text += "éend";
    int chars = 5004;
    for (int round = 0; round < 4; ++round) {
        # a new string in every round, so that the threads fill its cache at the same time
        string shared = text + "";
        Counter start(1);
        Counter done();
        list<int> errors = ();
        Mutex m();
        for (int t = 0; t < 8; ++t) {
            # the variables of the closure are new in every iteration
            int tid = t;
            int r = round;
            done.inc();
            background sub () {
                on_exit done.dec();
                start.waitForZero();
                int bad = 0;
                for (int k = 0; k < chars; ++k) {
                    int i = (k * (2 * tid + 1) + tid * 1259 + r * 97) % chars;
                    *string c = shared[i];
                    string expected = i < 5000 ? chr(ord("a") + i % 26) : (i == 5000 ? "é" : "end"[i - 5001]);
                    if (c != expected) {
                        ++bad;
                    }
                    if (!(k % 500) && shared.length() != chars) {
                        ++bad;
                    }
                }
                m.lock();
                on_exit m.unlock();
                push errors, bad;
            }();
        }
        start.dec();
        done.waitForZero();
        expect_same(errors.size(), 8, "threads", "thread count");
        foreach int bad in (errors) {
            expect_same(bad, 0, sprintf("threads, round %d", round), "wrong characters");
        }
    }
}

printf("PASS: %d language ASCII prefix cache checks\n", checks);
