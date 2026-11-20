# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the value of an in-region return that the coordinator's return type rejects is released after the locals of the
# function, as the function releases it (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
class U { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub a6() { T z("a6-z"); foreach auto x in ((new U("a6-u"),)) { T q("a6-q"); return x; } }
try { a6(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
printf("%y\n", log);
