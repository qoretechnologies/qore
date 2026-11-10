%modern
# the single-argument builtins with fast paths in the compiled modes (length(), strlen(), abs(), trim(), tolower(),
# toupper()) return what the builtin returns, or raise its error, for any argument; user functions with the same names
# are called normally; see compiled-overload-errors.qtest
# each call is in a function of its own without a try block, as the compiled modes call a builtin in a try block
# without its fast path
class C { public { int x = 1; } }

namespace User {
    string sub length(auto v) { return "User::length"; }
    string sub abs(auto v) { return "User::abs"; }
}

# arguments without a type known at parse time
auto sub call_length(auto v) { return length(v); }
auto sub call_strlen(auto v) { return strlen(v); }
auto sub call_abs(auto v) { return abs(v); }
auto sub call_trim(auto v) { return trim(v); }
auto sub call_tolower(auto v) { return tolower(v); }
auto sub call_toupper(auto v) { return toupper(v); }
auto sub call_user_length(auto v) { return User::length(v); }
auto sub call_user_abs(auto v) { return User::abs(v); }

# typed arguments: the fast paths
int sub length_string(string s) { return length(s); }
int sub strlen_string(string s) { return strlen(s); }
int sub length_binary(binary b) { return length(b); }
int sub length_nothing(*string s) { return length(s); }
int sub strlen_nothing(*string s) { return strlen(s); }
int sub abs_int(int i) { return abs(i); }
float sub abs_float(float f) { return abs(f); }
number sub abs_number(number n) { return abs(n); }
string sub trim_string(string s) { return trim(s); }
string sub tolower_string(string s) { return tolower(s); }
string sub toupper_string(string s) { return toupper(s); }

list<auto> vals = (-3, 1.5, True, new C(), (1, 2), \printf(), <0102>, 2026-01-01T00:00:00Z, "  -7 Äb ", {"a": 1},
    2.5n, -2.5n, NOTHING, NULL, "x");
foreach auto v in (vals) {
    string t = v.typeCode() == NT_OBJECT ? "object" : v.type();
    try {
        printf("length(%s)=%y\n", t, call_length(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("length(%s)=%s\n", t, ex.err);
    }
    try {
        printf("strlen(%s)=%y\n", t, call_strlen(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("strlen(%s)=%s\n", t, ex.err);
    }
    try {
        printf("abs(%s)=%y\n", t, call_abs(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("abs(%s)=%s\n", t, ex.err);
    }
    try {
        printf("trim(%s)=%y\n", t, call_trim(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("trim(%s)=%s\n", t, ex.err);
    }
    try {
        printf("tolower(%s)=%y\n", t, call_tolower(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("tolower(%s)=%s\n", t, ex.err);
    }
    try {
        printf("toupper(%s)=%y\n", t, call_toupper(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("toupper(%s)=%s\n", t, ex.err);
    }
    try {
        printf("User::length(%s)=%y\n", t, call_user_length(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("User::length(%s)=%s\n", t, ex.err);
    }
    try {
        printf("User::abs(%s)=%y\n", t, call_user_abs(v));
    } catch (hash<ExceptionInfo> ex) {
        printf("User::abs(%s)=%s\n", t, ex.err);
    }
}

printf("length(string)=%y\n", length_string("ÄbC"));
printf("strlen(string)=%y\n", strlen_string("ÄbC"));
printf("length(binary)=%y\n", length_binary(<010203>));
printf("length(*string NOTHING)=%y\n", length_nothing());
printf("strlen(*string NOTHING)=%y\n", strlen_nothing());
printf("abs(int)=%y\n", abs_int(-7));
printf("abs(float)=%y\n", abs_float(-2.5));
printf("abs(-0.0)=%y\n", abs_float(-0.0));
printf("abs(number)=%y\n", abs_number(-3.25n));
printf("trim(string)=%y\n", trim_string(" ÄbC "));
printf("tolower(string)=%y\n", tolower_string("ÄbC"));
printf("toupper(string)=%y\n", toupper_string("ÄbC"));
