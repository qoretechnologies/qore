# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a local that an outlined helper removes is not taken for assigned by the coordinator, whose call with it
# raises the error of the overload resolution (qore#6653); see AOTOutlineExceptionScopes.qtest

%modern
%loose-types
%no-strict-parse-errors
string sub fs(string s) { return s; }
sub report(string w, hash<ExceptionInfo> ex) { printf("%s: %s\n", w, ex.err); }
sub c_fs_removed() { string s = "x"; remove s; try { printf("no error %y\n", fs(s)); } catch (hash<ExceptionInfo> ex) { report("fs removed", ex); } }
c_fs_removed();
