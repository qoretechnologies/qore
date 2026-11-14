#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# NOTHING given to parameters that do not accept it, each call made directly in a function of its own: a variant
# that the argument may not match is not committed at parse time, so every execution mode resolves the call when it
# runs and raises RUNTIME-OVERLOAD-ERROR, as the AST interpreter does (issue #5771)

%modern
%loose-types
# the unassigned values are deliberate: their run-time handling is tested
%no-strict-parse-errors
class C {
    int m(int v) { return v; }
    static int sm(int v) { return v; }
    string ms(string s) { return s; }
}
int sub fi(int v) { return v; }
string sub fs(string s) { return s; }
int sub fh(hash<auto> h) { return h.size(); }
int sub fo(C c) { return 1; }
int sub f2(int v) { return v; }
int sub f2(string s) { return 2; }
auto sub nothing_value() {
}

sub report(string name, hash<ExceptionInfo> ex) {
    printf("%s: %s\n", name, ex.err);
}

sub c_fi_loose() { int i; try { printf("no error %y\n", fi(i)); } catch (hash<ExceptionInfo> ex) { report("fi loose", ex); } }
sub c_fs_loose() { string s; try { printf("no error %y\n", fs(s)); } catch (hash<ExceptionInfo> ex) { report("fs loose", ex); } }
sub c_fh_loose() { hash<auto> h; try { printf("no error %y\n", fh(h)); } catch (hash<ExceptionInfo> ex) { report("fh loose", ex); } }
sub c_fo_loose() { C c; try { printf("no error %y\n", fo(c)); } catch (hash<ExceptionInfo> ex) { report("fo loose", ex); } }
sub c_fi_deleted() { int i = 1; delete i; try { printf("no error %y\n", fi(i)); } catch (hash<ExceptionInfo> ex) { report("fi deleted", ex); } }
sub c_fs_removed() { string s = "x"; remove s; try { printf("no error %y\n", fs(s)); } catch (hash<ExceptionInfo> ex) { report("fs removed", ex); } }
sub c_fi_auto() { auto a = nothing_value(); try { printf("no error %y\n", fi(a)); } catch (hash<ExceptionInfo> ex) { report("fi auto", ex); } }
sub c_m_loose() { C o(); int i; try { printf("no error %y\n", o.m(i)); } catch (hash<ExceptionInfo> ex) { report("m loose", ex); } }
sub c_sm_loose() { int i; try { printf("no error %y\n", C::sm(i)); } catch (hash<ExceptionInfo> ex) { report("sm loose", ex); } }
sub c_ms_removed() { C o(); string s = "x"; remove s; try { printf("no error %y\n", o.ms(s)); } catch (hash<ExceptionInfo> ex) { report("ms removed", ex); } }
sub c_closure() { code cl = int sub (int v) { return v; }; int i; try { printf("no error %y\n", cl(i)); } catch (hash<ExceptionInfo> ex) { report("closure", ex); } }
sub c_callref() { code cr = \fi(); int i; try { printf("no error %y\n", cr(i)); } catch (hash<ExceptionInfo> ex) { report("call ref", ex); } }
sub c_f2_loose() { int i; try { printf("no error %y\n", f2(i)); } catch (hash<ExceptionInfo> ex) { report("f2 loose", ex); } }
sub c_f2_deleted() { int i = 1; delete i; try { printf("no error %y\n", f2(i)); } catch (hash<ExceptionInfo> ex) { report("f2 deleted", ex); } }
sub c_expr() { int i; try { printf("no error %y\n", fi(i) + 1); } catch (hash<ExceptionInfo> ex) { report("expr", ex); } }
sub c_nested() { int i; try { printf("no error %y\n", fi(fi(i))); } catch (hash<ExceptionInfo> ex) { report("nested", ex); } }
sub c_loop() { int i; for (int k = 0; k < 2; ++k) { try { printf("no error %y\n", fi(i)); } catch (hash<ExceptionInfo> ex) { report("loop", ex); } } }

for (int k = 0; k < 3; ++k) {
    c_fi_loose(); c_fs_loose(); c_fh_loose(); c_fo_loose(); c_fi_deleted(); c_fs_removed(); c_fi_auto(); c_m_loose();
    c_sm_loose(); c_ms_removed(); c_closure(); c_callref(); c_f2_loose(); c_f2_deleted(); c_expr(); c_nested();
    c_loop();
}
