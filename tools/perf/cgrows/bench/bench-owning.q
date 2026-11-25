%modern
# owning-call loops (qore#7043, #6860): a method that writes a parameter, given a temporary list, so that the call owns
# its argument: L=sf_try_w (final static method in a try block), L=s_try_w (static method), L=m_try_w (instance
# method), L=mf_try_w (final instance method), L=sf_w (final static method outside a try block), L=sf_try_r (final
# static method that only reads its parameter); N iterations
class C {
    static final int sf(list<auto> l, int i) { l = (); return i; }
    static int s(list<auto> l, int i) { l = (); return i; }
    int m(list<auto> l, int i) { l = (); return i; }
    final int mf(list<auto> l, int i) { l = (); return i; }
    static final int sr(list<auto> l, int i) { return i + l.size(); }
}
int sub f(int a) { return a; }
int sub run_sf_try_w(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        try {
            t += C::sf((i, 1), i);
        } catch (hash<ExceptionInfo> ex) {
            t = -1;
        }
    }
    return t;
}
int sub run_s_try_w(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        try {
            t += C::s((i, 1), i);
        } catch (hash<ExceptionInfo> ex) {
            t = -1;
        }
    }
    return t;
}
int sub run_m_try_w(int n) {
    C o();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        try {
            t += o.m((i, 1), i);
        } catch (hash<ExceptionInfo> ex) {
            t = -1;
        }
    }
    return t;
}
int sub run_mf_try_w(int n) {
    C o();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        try {
            t += o.mf((i, 1), i);
        } catch (hash<ExceptionInfo> ex) {
            t = -1;
        }
    }
    return t;
}
int sub run_sf_w(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += C::sf((i, 1), i);
    }
    return t;
}
int sub run_sf_try_r(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        try {
            t += C::sr((i, 1), i);
        } catch (hash<ExceptionInfo> ex) {
            t = -1;
        }
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "sf_try_w": run_sf_try_w(n); break;
    case "s_try_w": run_s_try_w(n); break;
    case "m_try_w": run_m_try_w(n); break;
    case "mf_try_w": run_mf_try_w(n); break;
    case "sf_w": run_sf_w(n); break;
    case "sf_try_r": run_sf_try_r(n); break;
}
