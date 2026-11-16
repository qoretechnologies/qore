# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

string sub mutate(int operation, string encoding, bool middle, bool nested) {
    string content = " " + strmul("a", 8192) + "Z\n";
    string source = convert_encoding("prefix" + content + (middle ? "suffix" : ""), encoding);
    string value = middle ? source.substr(6, content.length()) : source.substr(6);
    if (nested) value = value.substr(0);
    string result = "";
    switch (operation) {
        case 0: splice value, 3; break;
        case 1: splice value, 0, 2; break;
        case 2: splice value, 0, 2, "bbb"; break;
        case 3: result = extract value, 3; break;
        case 4: result = extract value, 0, 2; break;
        case 5: result = extract value, 0, 2, "bbb"; break;
        case 6: value += "tail"; break;
        case 7: value =~ s/a/b/; break;
        case 8: chomp value; break;
        case 9: trim value; break;
        case 10: value =~ tr/a-z/A-Z/; break;
    }
    if (convert_encoding(source, "UTF-8") != "prefix" + content + (middle ? "suffix" : ""))
        throw "PARENT-CHANGED", operation;
    return convert_encoding(value, "UTF-8") + ":" + convert_encoding(result, "UTF-8");
}

int checks = 0;
foreach string encoding in (("UTF-8", "ISO-8859-1")) {
    foreach bool middle in ((False, True)) {
        foreach bool nested in ((False, True)) {
            for (int operation = 0; operation < 11; ++operation) {
                string actual = mutate(operation, encoding, middle, nested);
                string value = " " + strmul("a", 8192) + "Z\n";
                string result = "";
                switch (operation) {
                    case 0: splice value, 3; break;
                    case 1: splice value, 0, 2; break;
                    case 2: splice value, 0, 2, "bbb"; break;
                    case 3: result = extract value, 3; break;
                    case 4: result = extract value, 0, 2; break;
                    case 5: result = extract value, 0, 2, "bbb"; break;
                    case 6: value += "tail"; break;
                    case 7: value =~ s/a/b/; break;
                    case 8: chomp value; break;
                    case 9: trim value; break;
                    case 10: value =~ tr/a-z/A-Z/; break;
                }
                if (actual != value + ":" + result) throw "VIEW-MUTATION", operation;
                ++checks;
            }
        }
    }
}
printf("PASS: %d language substring-view mutation checks\n", checks);
