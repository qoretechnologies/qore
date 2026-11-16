/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <string>
#include <vector>

struct Mutation {
    const char* name;
    std::function<void(QoreString&, ExceptionSink*)> apply;
};

static const std::vector<Mutation> mutations = {
    {"replace-char", [](auto& s, auto*) { s.replaceChar(0, 'b'); }},
    {"terminate", [](auto& s, auto*) { s.terminate(17); }},
    {"replace-remove", [](auto& s, auto*) { s.replace(0, 2, ""); }},
    {"replace", [](auto& s, auto*) { s.replace(0, 2, "bbbbb"); }},
    {"replace-equal", [](auto& s, auto*) { s.replace(0, 2, "bb"); }},
    {"replace-shrink", [](auto& s, auto*) { s.replace(0, 2, "b"); }},
    {"replace-all", [](auto& s, auto*) { s.replaceAll("aaa", "bb"); }},
    {"splice-tail", [](auto& s, auto* x) { s.splice(3, x); }},
    {"splice-range", [](auto& s, auto* x) { s.splice(0, 2, x); }},
    {"splice-replace", [](auto& s, auto* x) { QoreString r("bbb"); s.splice(0, 2, r, x); }},
    {"extract-tail", [](auto& s, auto* x) { TempString r(s.extract(3, x)); if (!*x) s.concat(*r, x); }},
    {"extract-range", [](auto& s, auto* x) { TempString r(s.extract(0, 2, x)); if (!*x) s.concat(*r, x); }},
    {"extract-replace", [](auto& s, auto* x) {
        QoreStringNodeHolder replacement(new QoreStringNode("bbbbb"));
        TempString r(s.extract(0, 2, QoreValue(*replacement), x));
        if (!*x) s.concat(*r, x);
    }},
    {"sprintf", [](auto& s, auto*) { s.sprintf("-%s:%d", "tail", 17); }},
    {"regex", [](auto& s, auto* x) { QoreString a("a"), b("b"); s.regexSubstInPlace(a, b, 0, x); }},
    {"regex-error", [](auto& s, auto* x) {
        QoreString a("["), b("b");
        s.regexSubstInPlace(a, b, 0, x);
        if (!*x) { fprintf(stderr, "FAIL: invalid regex accepted\n"); abort(); }
        x->clear();
    }},
    {"append-char", [](auto& s, auto*) { s.concat('b'); }},
    {"append-text", [](auto& s, auto*) { s.concat("xyz"); }},
    {"append-bytes", [](auto& s, auto*) { s.concat("xyz", size_t(3)); }},
    {"append-unicode", [](auto& s, auto* x) { s.concatUnicode(0xe9, x); }},
    {"append-padding", [](auto& s, auto*) { s.addch('x', 17); }},
    {"prepend", [](auto& s, auto*) { s.prepend("xx"); }},
    {"insert", [](auto& s, auto*) { s.insert("bb", 3); }},
    {"insert-char", [](auto& s, auto*) { s.insertch('b', 3, 2); }},
    {"trim-leading", [](auto& s, auto*) { s.trim_leading(" a"); }},
    {"trim-trailing", [](auto& s, auto*) { s.trim_trailing(" a\n"); }},
    {"trim-unicode", [](auto& s, auto* x) { s.trim(x); }},
    {"trim-single-leading", [](auto& s, auto*) { s.trim_single_leading(' '); }},
    {"trim-single-trailing", [](auto& s, auto*) { s.trim_single_trailing('\n'); }},
    {"chomp", [](auto& s, auto*) { s.chomp(); }},
    {"uppercase", [](auto& s, auto*) { s.toupr(); }},
    {"lowercase", [](auto& s, auto*) { s.tolwr(); }},
    {"remove-bytes", [](auto& s, auto*) { s.removeBytes(3); }},
    {"clear", [](auto& s, auto*) { s.clear(); s.concat("reused"); }},
    {"clear-terminate-empty", [](auto& s, auto*) { s.clear(); s.terminate(0); }},
    {"clear-replace-empty", [](auto& s, auto*) { s.clear(); s.replace(0, 0, ""); }},
    {"clear-splice-empty", [](auto& s, auto* x) { QoreString empty; s.clear(); s.splice(0, 0, empty, x); }},
    {"clear-format-error", [](auto& s, auto*) { s.clear(); s.sprintf("%lc", wint_t(0xd800)); }},
    {"reset", [](auto& s, auto*) { s.reset(); s.concat("reused"); }},
    {"set", [](auto& s, auto*) { s.set("new"); }},
    {"set-string", [](auto& s, auto*) { QoreString r("new"); s.set(r); }},
    {"take", [](auto& s, auto*) { s.take(strdup("new")); }},
    {"take-and-terminate", [](auto& s, auto*) { s.takeAndTerminate(strdup("new"), 3); }},
    {"give-buffer", [](auto& s, auto*) { free(s.giveBuffer()); s.concat("reused"); }},
    {"give-buffer-terminate-empty", [](auto& s, auto*) { free(s.giveBuffer()); s.terminate(0); }},
    {"reserve", [](auto& s, auto*) { s.reserve(16384); s.concat('b'); }},
};

static bool check(const Mutation& mutation, const QoreEncoding* encoding, bool middle, bool nested, bool unicode) {
    ExceptionSink xsink;
    const std::string content = std::string(" ") + std::string(8192, 'a') + (unicode ? "éZ\n" : "Z\n");
    const std::string parent_text = "prefix" + content + (middle ? "suffix" : "");
    QoreString expected(content, encoding);
    QoreStringNodeHolder parent(new QoreStringNode(parent_text, encoding));
    QoreStringNodeHolder view(middle ? parent->substr(6, expected.length(), &xsink) : parent->substr(6, &xsink));
    if (xsink || !view) {
        xsink.handleExceptions();
        return false;
    }
    if (nested) {
        QoreStringNodeHolder inner(view->substr(0, &xsink));
        if (xsink || !inner) {
            xsink.handleExceptions();
            return false;
        }
        view = inner.release();
    }
    // Do not read view's bytes here: c_str() would materialize a middle view and conceal the missing check.
    mutation.apply(**view, &xsink);
    if (xsink) {
        xsink.handleExceptions();
        return false;
    }
    mutation.apply(expected, &xsink);
    bool ok = !xsink && view->size() == expected.size()
        && (!expected.size() || !memcmp(view->c_str(), expected.c_str(), expected.size()))
        && parent->equal(parent_text.c_str());
    // A second mutation checks that no retained view state overrides the replacement text.
    view->concat("!");
    expected.concat("!");
    ok = view->equal(expected) && ok;
    if (!ok) {
        fprintf(stderr, "FAIL: %s, %s view%s, %s\n", mutation.name, middle ? "middle" : "end",
            nested ? " of view" : "", encoding->getCode());
    }
    if (xsink) xsink.handleExceptions();
    return ok;
}

int main(int argc, char** argv) {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    bool ok = true;
    unsigned checks = 0;
    for (const auto& mutation : mutations) {
        if (argc > 1 && strcmp(argv[1], mutation.name)) continue;
        for (auto* encoding : {QCS_UTF8, QCS_ISO_8859_1}) {
            for (bool middle : {false, true}) {
                for (bool nested : {false, true}) {
                    for (bool unicode : {false, true}) {
                        ok = check(mutation, encoding, middle, nested, unicode) && ok;
                        ++checks;
                    }
                }
            }
        }
    }
    qore_cleanup();
    if (ok && checks) printf("PASS: %u substring-view mutation checks\n", checks);
    return ok && checks ? 0 : 1;
}
