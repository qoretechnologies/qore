#!/usr/bin/env qore

%requires ../../../../qlib/Util.qm
%requires ../../../../qlib/QUnit.qm
%requires ../../../../qlib/TextWrap.qm

%allow-bare-refs


#! Parent class with utility methods for TextWrap tests.
class BaseTest inherits QUnit::Test {
    constructor (string name, string version) : Test (name, version) {
    }

    constructor () : Test ("BaseTest", "1.0") {
        set_return_value (main());
    }

    private {
        TextWrapper wrapper;
    }

    string show (list text) {
        my list result = ();
        for (my int i = 0; i < text.size(); i++)
            result += sprintf ("  %d: %n", i, text[i]);
        return result ? result.join("\n") : "  no lines";
    }

    string show (string text) {
        return sprintf ("  %n\n", text);
    }

    check (any result, any expect) {
        assertEq (expect, result,
                # sprintf ("expected:\n%s\nbut got:\n%s", self.show(expect), self.show(result))
                );
    }

    check_wrap (string text, int width, list expect, *hash kwargs) {
        my list result = TextWrap::wrap (text, width, kwargs);
        self.check (result, expect);
    }

    check_split (string text, list expect) {
        my list result = self.wrapper._split(text);
        assertEq (expect, result,
                # sprintf ("expected: %n\nbut got:  %n", expect, result)
                );
    }

    list wssplit (string text) {
        return regex_subst(text, '\s+', ' ', Qore::RE_Global).split(' ');
    }
}
