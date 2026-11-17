# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the program run by in-place-append-after-try.qtest in every execution mode: each case gets a string or a list that
# the caller still holds, and prints what it returns and what the caller's value is afterwards, which an append
# must never change

%modern
%disable-all-warnings

#! returns a string built at runtime, which the caller and a case then share
string sub src() {
    string t = "T";
    t += "U";
    return t;
}

#! returns a list built at runtime, which the caller and a case then share
list<int> sub lsrc() {
    list<int> l = (1,);
    push l, 2;
    return l;
}

string sub thrower(bool thr) {
    if (thr) {
        throw "X";
    }
    return "-";
}

# the catch block assigns the shared value; the append follows the try statement
string sub catchAssign(string t, bool thr) {
    string s = "abc";
    try {
        if (thr) {
            throw "X";
        }
        s = "def";
    } catch (hash<ExceptionInfo> ex) {
        s = t;
    }
    s += "x";
    return s;
}

# appends in the try block, to the shared value in the catch block and after the try statement
string sub appendInCatch(string t, bool thr) {
    string s = "abc";
    try {
        s += "y";
        s = t;
        s += "w";
        if (thr) {
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
        s += "z";
    }
    s += "q";
    return s;
}

# a fresh value in the catch block, then the shared one
string sub catchFreshThenShared(string t, bool thr) {
    string s = "";
    try {
        if (thr) {
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
        s = "fresh";
        s += "1";
        s = t;
        s += "2";
    }
    return s;
}

# the append follows the inner try statement in the outer try block
string sub nestedTry(string t, bool thr) {
    string s = "abc";
    try {
        try {
            if (thr) {
                throw "X";
            }
            s = "def";
        } catch (hash<ExceptionInfo> ex) {
            s = t;
        }
        s += "i";
    } catch (hash<ExceptionInfo> ex) {
        s = "outer";
    }
    s += "o";
    return s;
}

# the inner catch block assigns the shared value and rethrows to the outer one
string sub rethrowFromCatch(string t, bool thr) {
    string s = "abc";
    try {
        try {
            if (thr) {
                throw "X";
            }
            s = "def";
        } catch (hash<ExceptionInfo> ex) {
            s = t;
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) {
    }
    s += "r";
    return s;
}

# an on_error block assigns the shared value when the exception leaves the try block
string sub onErrorAssign(string t, bool thr) {
    string s = "abc";
    try {
        on_error {
            s = t;
        }
        if (thr) {
            throw "X";
        }
        s = "def";
    } catch (hash<ExceptionInfo> ex) {
    }
    s += "e";
    return s;
}

# an on_exit block assigns the shared value on both paths
string sub onExitAssign(string t, bool thr) {
    string s = "abc";
    try {
        on_exit {
            s = t;
        }
        if (thr) {
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
    }
    s += "x";
    return s;
}

# a call throws after a fresh value was assigned
string sub callThrows(string t, bool thr) {
    string s = "abc";
    try {
        s = "def";
        s += thrower(thr);
    } catch (hash<ExceptionInfo> ex) {
        s = t;
    }
    s += "c";
    return s;
}

# the try statement is in a loop, so the value from the catch block reaches the next iteration's appends too
string sub tryInLoop(string t, bool thr) {
    string s = "";
    for (int i = 0; i < 3; ++i) {
        try {
            if (thr && i == 1) {
                throw "X";
            }
            s += "a";
        } catch (hash<ExceptionInfo> ex) {
            s = t;
        }
        s += "b";
    }
    return s;
}

# the exception leaves the try block between the assignment of the shared value and a fresh one
string sub throwBetweenAssignments(string t, bool thr) {
    string s = "abc";
    try {
        s = t;
        thrower(thr);
        s = "fresh";
    } catch (hash<ExceptionInfo> ex) {
        s += "m";
    }
    s += "n";
    return s;
}

# the same for a push to a list
list<int> sub listCatchAssign(list<int> o, bool thr) {
    list<int> l = ();
    try {
        if (thr) {
            throw "X";
        }
        l = ();
    } catch (hash<ExceptionInfo> ex) {
        l = o;
    }
    push l, 9;
    return l;
}

list<int> sub listPushInCatch(list<int> o, bool thr) {
    list<int> l = ();
    try {
        l = o;
        thrower(thr);
        l = ();
    } catch (hash<ExceptionInfo> ex) {
        push l, 8;
    }
    push l, 9;
    return l;
}

#! an object whose destructor throws, so that the store that releases it raises in the middle of a block
class Thrower {
    destructor() {
        throw "DTOR";
    }
}

# the store that releases the object throws in the middle of the block, after the shared value was assigned and
# before the fresh one: the state at the end of the block (fresh) is not the state that the handler sees
list<int> sub dtorList(list<int> o) {
    list<int> l = ();
    auto x = new Thrower();
    try {
        l = o;
        x = 1;
        l = ();
    } catch (hash<ExceptionInfo> ex) {
    }
    push l, 9;
    return l;
}

string sub dtorString(string t) {
    string s = "abc";
    auto x = new Thrower();
    try {
        s = t;
        x = 1;
        s = "fresh";
    } catch (hash<ExceptionInfo> ex) {
    }
    s += "p";
    return s;
}

# the type of a value that the catch block changes
string sub catchChangesType(bool thr) {
    auto x = "str";
    try {
        if (thr) {
            throw "X";
        }
        x = "s2";
    } catch (hash<ExceptionInfo> ex) {
        x = 15;
    }
    return x.typeCode() + ":" + x.type();
}

# the catch block assigns the shared value on one path only
string sub catchAssignsConditionally(string t, bool thr, bool assign) {
    string s = "abc";
    try {
        if (thr) {
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
        if (assign) {
            s = t;
        }
    }
    s += "k";
    return s;
}

# two throwing calls: the first with the shared value, the second with a fresh one
string sub twoThrowPoints(string t, bool first, bool second) {
    string s = "abc";
    try {
        s = t;
        thrower(first);
        s = "fresh";
        thrower(second);
    } catch (hash<ExceptionInfo> ex) {
    }
    s += "p";
    return s;
}

# the appends that stay in place: no try statement, and a fresh value after one
string sub plainBuilder(int n) {
    string s = "";
    for (int i = 0; i < n; ++i) {
        s += "ab";
    }
    return s;
}

string sub freshAfterTry(string t, bool thr, int n) {
    string s = "abc";
    try {
        if (thr) {
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
        s = t;
    }
    string r = s;
    s = "";
    for (int i = 0; i < n; ++i) {
        s += "ab";
    }
    return r + "/" + s.size();
}

# appends before and after a try statement whose catch block leaves the local alone stay in place
string sub appendAroundTry(int n) {
    string s = "";
    int x = 0;
    for (int i = 0; i < n; ++i) {
        s += "a";
        try {
            x += thrower(i == 3);
        } catch (hash<ExceptionInfo> ex) {
            x = -1;
        }
        s += "b";
    }
    return s + "/" + x;
}

# appends in a catch block and before the try statement to a local that is fresh wherever the try block can throw
# are in place
string sub appendInCatchFresh(int n) {
    string s = "";
    for (int i = 0; i < n; ++i) {
        s += "a";
        try {
            if (i == 2) {
                throw "X";
            }
        } catch (hash<ExceptionInfo> ex) {
            s += "c";
        }
    }
    return s;
}

list<int> sub pushAroundTry(int n) {
    list<int> l = ();
    int x = 0;
    for (int i = 0; i < n; ++i) {
        push l, i;
        try {
            x += thrower(i == 3);
        } catch (hash<ExceptionInfo> ex) {
            x = -1;
        }
        push l, x;
    }
    return l;
}

list<int> sub plainListBuilder(int n) {
    list<int> l = ();
    for (int i = 0; i < n; ++i) {
        push l, i;
    }
    return l;
}

foreach bool thr in ((True, False)) {
    string t;
    t = src(); printf("catchAssign|%y => %y | %y\n", thr, catchAssign(t, thr), t);
    t = src(); printf("appendInCatch|%y => %y | %y\n", thr, appendInCatch(t, thr), t);
    t = src(); printf("catchFreshThenShared|%y => %y | %y\n", thr, catchFreshThenShared(t, thr), t);
    t = src(); printf("nestedTry|%y => %y | %y\n", thr, nestedTry(t, thr), t);
    t = src(); printf("rethrowFromCatch|%y => %y | %y\n", thr, rethrowFromCatch(t, thr), t);
    t = src(); printf("onErrorAssign|%y => %y | %y\n", thr, onErrorAssign(t, thr), t);
    t = src(); printf("onExitAssign|%y => %y | %y\n", thr, onExitAssign(t, thr), t);
    t = src(); printf("callThrows|%y => %y | %y\n", thr, callThrows(t, thr), t);
    t = src(); printf("tryInLoop|%y => %y | %y\n", thr, tryInLoop(t, thr), t);
    t = src(); printf("throwBetweenAssignments|%y => %y | %y\n", thr, throwBetweenAssignments(t, thr), t);
    t = src(); printf("freshAfterTry|%y => %y | %y\n", thr, freshAfterTry(t, thr, 100), t);
    list<int> o;
    o = lsrc(); printf("listCatchAssign|%y => %y | %y\n", thr, listCatchAssign(o, thr), o);
    o = lsrc(); printf("listPushInCatch|%y => %y | %y\n", thr, listPushInCatch(o, thr), o);
    printf("catchChangesType|%y => %y\n", thr, catchChangesType(thr));
    t = src(); printf("catchAssignsConditionally|%y|True => %y | %y\n", thr, catchAssignsConditionally(t, thr, True), t);
    t = src(); printf("catchAssignsConditionally|%y|False => %y | %y\n", thr, catchAssignsConditionally(t, thr, False),
        t);
    t = src(); printf("twoThrowPoints|%y|first => %y | %y\n", thr, twoThrowPoints(t, thr, False), t);
    t = src(); printf("twoThrowPoints|%y|second => %y | %y\n", thr, twoThrowPoints(t, False, thr), t);
}
list<int> dtor_o = lsrc();
printf("dtorList => %y | %y\n", dtorList(dtor_o), dtor_o);
string dtor_t = src();
printf("dtorString => %y | %y\n", dtorString(dtor_t), dtor_t);
printf("appendInCatchFresh => %y\n", appendInCatchFresh(5));
printf("appendAroundTry => %y\n", appendAroundTry(6));
printf("pushAroundTry => %y\n", pushAroundTry(6));
printf("plainBuilder => %d\n", plainBuilder(1000).size());
printf("plainListBuilder => %d\n", plainListBuilder(1000).size());
