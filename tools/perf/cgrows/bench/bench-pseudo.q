%modern
# pseudo-method call loops: L=pv (a pseudo-method without an intrinsic: <string>::isEncodingValid()), L=pl (one with
# an intrinsic: <string>::length()), L=pt (<int>::typeCode(), an intrinsic of every type), L=ps (<string>::split() of a
# string local, qore#5578), L=tr (trim() of a string local); N iterations
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
int n = ENV.N.toInt();
switch (ENV.L) {
    case "pv": run_pv(n); break;
    case "pl": run_pl(n); break;
    case "pt": run_pt(n); break;
    case "ps": run_ps(n); break;
    case "tr": run_tr(n); break;
}
