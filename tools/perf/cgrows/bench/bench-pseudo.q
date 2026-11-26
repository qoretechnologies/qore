%modern
# pseudo-method call loops: L=pv (a pseudo-method without an intrinsic: <string>::isEncodingValid()), L=pl (one with
# an intrinsic: <string>::length()), L=pt (<int>::typeCode(), an intrinsic of every type), L=ps (<string>::split() of a
# string local, qore#5578), L=tr (trim() of a string local), L=lw (<string>::lwr() of a string local, qore#7711),
# L=ix (index() of a string local), L=il (index() of the lwr() of a string local), L=jl (<list>::join() of a
# list<string> local, qore#7731), L=jf (join() of it), L=jm (<list>::join() of a map of it, a fused join); N iterations
int sub f(int a) { return a; }
int sub run_pv(int n) {
    string s = "abc";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += s.isEncodingValid() ? 1 : 0;
    }
    return t;
}
int sub run_pl(int n) {
    string s = "abc";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += s.length();
    }
    return t;
}
int sub run_pt(int n) {
    auto v = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += v.typeCode();
    }
    return t;
}
int sub run_ps(int n) {
    string s = " a b c ";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += s.split(" ").size();
    }
    return t;
}
int sub run_tr(int n) {
    string s = " a b c ";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += trim(s).size();
    }
    return t;
}
int sub run_lw(int n) {
    string s = " A B C ";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        string l = s.lwr();
        t += l.size();
    }
    return t;
}
int sub run_ix(int n) {
    string s = " a b c ";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += index(s, "c");
    }
    return t;
}
int sub run_il(int n) {
    string s = " A B C ";
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += index(s.lwr(), "c");
    }
    return t;
}
int sub run_jl(int n) {
    list<string> l = ("a", "bb", "ccc", "dd", "e");
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += l.join(",").size();
    }
    return t;
}
int sub run_jf(int n) {
    list<string> l = ("a", "bb", "ccc", "dd", "e");
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += join(",", l).size();
    }
    return t;
}
int sub run_jm(int n) {
    list<string> l = ("a", "bb", "ccc", "dd", "e");
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (map $1 + "x", l).join(",").size();
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "pv": run_pv(n); break;
    case "pl": run_pl(n); break;
    case "pt": run_pt(n); break;
    case "ps": run_ps(n); break;
    case "tr": run_tr(n); break;
    case "lw": run_lw(n); break;
    case "ix": run_ix(n); break;
    case "il": run_il(n); break;
    case "jl": run_jl(n); break;
    case "jf": run_jf(n); break;
    case "jm": run_jm(n); break;
}
