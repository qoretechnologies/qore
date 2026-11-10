# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CallSiteFrames.qc the expected call stack frames of call-site-frames-script.q

/*  Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! the expected call stack frames of call-site-frames-script.q
/** Each frame of called code is located at its call expression, with its lines and 0-based byte columns, also in a
    statement of more than one line and after a call in an operand that is not evaluated
*/
class CallSiteFrames {
    public {
        #! the frames that AOT-compiled code gives with other lines (issue 5745; see getExpectedAot())
        const AotKnownDifferences = {
            "member initializer: mi 20-20 22-26": "member initializer: mi -1--1 22-26",
            "default argument: da 15-15 28-32": "default argument: da 44-44 28-32",
        };

        #! the name of the script
        const Script = "call-site-frames-script.q";

        #! the frames that the script records: "<what>: <function> <line>-<endline> <start column>-<end column>"
        const Expected = (
            # "private { int v = mi(); }"
            "member initializer: mi 20-20 22-26",
            # "constructor() : Base(ba()) {"
            "base constructor argument: ba 21-21 25-29",
            "base constructor: Base::constructor 21-21 20-30",
            # "C c();"
            "constructor: C::constructor 29-29 0-3",
            # g(2) of "int r = (x ? g(1) : 0) +\n    g(2);": g(1) is not called
            "g: g 34-34 4-8",
            # "r = c.m(3) +\n   cl(4);"
            "method: C::m 36-36 4-10",
            "closure: <anonymous closure> 37-37 3-8",
            # "r = C::s(5) + fref(\n    6);"
            "static method: C::s 38-38 4-11",
            "g: g 38-39 14-6",
            # "return m(1) +\n        m(2);" in C::selfCall()
            "method: C::m 24-24 28-32",
            "method: C::m 25-25 8-12",
            # "C c2 = new\n    C();"
            "member initializer: mi 20-20 22-26",
            "base constructor argument: ba 21-21 25-29",
            "base constructor: Base::constructor 21-21 20-30",
            "constructor: C::constructor 41-42 7-7",
            # "bool b = x && g(7) || g(8);": g(7) is not called
            "g: g 43-43 22-26",
            # the default argument of "int sub withDefault(int a = da())", called by "r = withDefault();"
            "default argument: da 15-15 28-32",
            # the frame of the exception's call stack: "r = 1 +\n        thr();"
            "exception: thr 47-47 8-13",
            # "r = takes(new D(), 1) +\n    g(9);": the destructor of the temporary object makes a call between the
            # calls, which does not take or clear the call site of the next call
            "takes: takes 56-56 4-21",
            "g: g 57-57 4-8",
            # "r = g(mk()) + g(takes(new D(), 2));": the destructor of a local variable of mk() makes a call
            "g: g 58-58 4-11",
            "takes: takes 58-58 16-33",
            "g: g 58-58 14-34",
        );
    }

    #! returns the frames expected from AOT-compiled code, whose locations have no columns
    /** Known differences (https://github.com/qoretechnologies/qore/issues/5745): the call nodes of the expressions
        that AOT-compiled code evaluates as AST trees restored from its image have no location, and its members
        none either, so the call of a member initializer has no line and the call of a default argument is located
        at the caller's line
    */
    static list<string> getExpectedAot() {
        return map regex_subst(AotKnownDifferences{$1} ?? $1, "[-0-9]+-[-0-9]+$", "-1--1"), Expected;
    }
}
