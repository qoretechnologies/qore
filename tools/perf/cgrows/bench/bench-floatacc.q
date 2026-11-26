%modern
# += on a float local (qore#5578): L=fl (a float local operand), fi (an int operand), fd (a float function's call), fs (a
# float static method's call), fr (a code<float()> call reference's result, which takes the lvalue path); N iterations
int sub f(int a) { return a + 1; }
float sub g(float a) { return 65535.5 - a; }
float sub h() { return 0.25; }
class S {
    static float s(float a) { return 65535.5 - a; }
}
int sub run_fl(int n) {
    int t = 0;
    float sum = 0.0;
    float delta = 0.25;
    for (int i = 0; i < n; ++i) {
        sum += delta;
    }
    t += int(sum);
    return t;
}
int sub run_fi(int n) {
    int t = 0;
    float sum = 0.5;
    for (int i = 0; i < n; ++i) {
        sum += i;
    }
    t += int(sum);
    return t;
}
int sub run_fd(int n) {
    int t = 0;
    float sum = 0.0;
    float value = 7.0;
    for (int i = 0; i < n; ++i) {
        sum += g(value);
    }
    t += int(sum);
    return t;
}
int sub run_fs(int n) {
    int t = 0;
    float sum = 0.0;
    float value = 7.0;
    for (int i = 0; i < n; ++i) {
        sum += S::s(value);
    }
    t += int(sum);
    return t;
}
int sub run_fr(int n) {
    int t = 0;
    code<float()> fn = \h();
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += fn();
    }
    t += int(sum);
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "fl": run_fl(n); break;
    case "fi": run_fi(n); break;
    case "fd": run_fd(n); break;
    case "fs": run_fs(n); break;
    case "fr": run_fr(n); break;
}
