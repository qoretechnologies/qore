%modern
# reads through a reference whose target has no value: every use of the value must behave as in the AST interpreter;
# see reference-nothing.qtest
# every function is called by name from top-level code, which --exec-mode=jit compiles (and can inline the function
# into); each use is read outside of a try block and inside one, which the compiled modes compile differently
class C {
    public { int m = 1; }
}


sub clear(reference<int> r) {
    remove r;
}

# use: store
int sub use_store(reference<int> p) { int x = p; return x; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
int sub deleted_store(reference<int> p, C c) { delete c; int x = p; return x; }
# the target is a hash key or a variable removed before the read
int sub removed_key_store(reference<int> p, reference<hash<string, int>> h) { remove h.k; int x = p; return x; }
int sub removed_variable_store(reference<int> p) { clear(\p); int x = p; return x; }
# the same reads in a try block
string sub try_deleted_store(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        int x = p; rv = x;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_store(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        int x = p; rv = x;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: return
int sub use_return(reference<int> p) { return p; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
int sub deleted_return(reference<int> p, C c) { delete c; return p; }
# the target is a hash key or a variable removed before the read
int sub removed_key_return(reference<int> p, reference<hash<string, int>> h) { remove h.k; return p; }
int sub removed_variable_return(reference<int> p) { clear(\p); return p; }
# the same reads in a try block
string sub try_deleted_return(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        rv = p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_return(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        rv = p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: add
int sub use_add(reference<int> p) { return p + 1; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
int sub deleted_add(reference<int> p, C c) { delete c; return p + 1; }
# the target is a hash key or a variable removed before the read
int sub removed_key_add(reference<int> p, reference<hash<string, int>> h) { remove h.k; return p + 1; }
int sub removed_variable_add(reference<int> p) { clear(\p); return p + 1; }
# the same reads in a try block
string sub try_deleted_add(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        rv = p + 1;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_add(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        rv = p + 1;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: compare
bool sub use_compare(reference<int> p) { return p == 0; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
bool sub deleted_compare(reference<int> p, C c) { delete c; return p == 0; }
# the target is a hash key or a variable removed before the read
bool sub removed_key_compare(reference<int> p, reference<hash<string, int>> h) { remove h.k; return p == 0; }
bool sub removed_variable_compare(reference<int> p) { clear(\p); return p == 0; }
# the same reads in a try block
string sub try_deleted_compare(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        rv = p == 0;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_compare(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        rv = p == 0;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: increment
int sub use_increment(reference<int> p) { return ++p; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
int sub deleted_increment(reference<int> p, C c) { delete c; return ++p; }
# the target is a hash key or a variable removed before the read
int sub removed_key_increment(reference<int> p, reference<hash<string, int>> h) { remove h.k; return ++p; }
int sub removed_variable_increment(reference<int> p) { clear(\p); return ++p; }
# the same reads in a try block
string sub try_deleted_increment(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        rv = ++p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_increment(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        rv = ++p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: add-assign
int sub use_add_assign(reference<int> p) { p += 2; return p; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
int sub deleted_add_assign(reference<int> p, C c) { delete c; p += 2; return p; }
# the target is a hash key or a variable removed before the read
int sub removed_key_add_assign(reference<int> p, reference<hash<string, int>> h) { remove h.k; p += 2; return p; }
int sub removed_variable_add_assign(reference<int> p) { clear(\p); p += 2; return p; }
# the same reads in a try block
string sub try_deleted_add_assign(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        p += 2; rv = p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_add_assign(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        p += 2; rv = p;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: format
string sub use_format(reference<int> p) { return sprintf("%d", p); }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
string sub deleted_format(reference<int> p, C c) { delete c; return sprintf("%d", p); }
# the target is a hash key or a variable removed before the read
string sub removed_key_format(reference<int> p, reference<hash<string, int>> h) { remove h.k; return sprintf("%d", p); }
string sub removed_variable_format(reference<int> p) { clear(\p); return sprintf("%d", p); }
# the same reads in a try block
string sub try_deleted_format(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        rv = sprintf("%d", p);
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_format(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        rv = sprintf("%d", p);
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: float
float sub use_float(reference<int> p) { float f = p; return f; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
float sub deleted_float(reference<int> p, C c) { delete c; float f = p; return f; }
# the target is a hash key or a variable removed before the read
float sub removed_key_float(reference<int> p, reference<hash<string, int>> h) { remove h.k; float f = p; return f; }
float sub removed_variable_float(reference<int> p) { clear(\p); float f = p; return f; }
# the same reads in a try block
string sub try_deleted_float(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        float f = p; rv = f;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_float(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        float f = p; rv = f;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# use: auto
auto sub use_auto(reference<int> p) { auto a = p; return a; }
# the object is deleted in the function that reads its member through the reference parameter (issue #5685)
auto sub deleted_auto(reference<int> p, C c) { delete c; auto a = p; return a; }
# the target is a hash key or a variable removed before the read
auto sub removed_key_auto(reference<int> p, reference<hash<string, int>> h) { remove h.k; auto a = p; return a; }
auto sub removed_variable_auto(reference<int> p) { clear(\p); auto a = p; return a; }
# the same reads in a try block
string sub try_deleted_auto(reference<int> p, C c) {
    delete c;
    try {
        auto rv;
        auto a = p; rv = a;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub try_removed_variable_auto(reference<int> p) {
    clear(\p);
    try {
        auto rv;
        auto a = p; rv = a;
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

{
    C c();
    try {
        printf("%s store: %y\n", "deleted member", deleted_store(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s store: %y\n", "removed key", removed_key_store(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s store: %y\n", "removed variable", removed_variable_store(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s store: %y\n", "try deleted member", try_deleted_store(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s store: %y\n", "try removed variable", try_removed_variable_store(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s store: %y\n", "assigned", use_store(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s store: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s return: %y\n", "deleted member", deleted_return(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s return: %y\n", "removed key", removed_key_return(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s return: %y\n", "removed variable", removed_variable_return(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s return: %y\n", "try deleted member", try_deleted_return(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s return: %y\n", "try removed variable", try_removed_variable_return(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s return: %y\n", "assigned", use_return(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s return: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s add: %y\n", "deleted member", deleted_add(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s add: %y\n", "removed key", removed_key_add(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s add: %y\n", "removed variable", removed_variable_add(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s add: %y\n", "try deleted member", try_deleted_add(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s add: %y\n", "try removed variable", try_removed_variable_add(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s add: %y\n", "assigned", use_add(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s compare: %y\n", "deleted member", deleted_compare(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s compare: %y\n", "removed key", removed_key_compare(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s compare: %y\n", "removed variable", removed_variable_compare(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s compare: %y\n", "try deleted member", try_deleted_compare(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s compare: %y\n", "try removed variable", try_removed_variable_compare(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s compare: %y\n", "assigned", use_compare(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s compare: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s increment: %y\n", "deleted member", deleted_increment(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s increment: %y\n", "removed key", removed_key_increment(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s increment: %y\n", "removed variable", removed_variable_increment(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s increment: %y\n", "try deleted member", try_deleted_increment(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s increment: %y\n", "try removed variable", try_removed_variable_increment(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s increment: %y\n", "assigned", use_increment(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s increment: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s add-assign: %y\n", "deleted member", deleted_add_assign(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s add-assign: %y\n", "removed key", removed_key_add_assign(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s add-assign: %y\n", "removed variable", removed_variable_add_assign(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s add-assign: %y\n", "try deleted member", try_deleted_add_assign(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s add-assign: %y\n", "try removed variable", try_removed_variable_add_assign(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s add-assign: %y\n", "assigned", use_add_assign(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s add-assign: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s format: %y\n", "deleted member", deleted_format(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s format: %y\n", "removed key", removed_key_format(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s format: %y\n", "removed variable", removed_variable_format(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s format: %y\n", "try deleted member", try_deleted_format(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s format: %y\n", "try removed variable", try_removed_variable_format(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s format: %y\n", "assigned", use_format(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s format: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s float: %y\n", "deleted member", deleted_float(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s float: %y\n", "removed key", removed_key_float(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s float: %y\n", "removed variable", removed_variable_float(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s float: %y\n", "try deleted member", try_deleted_float(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s float: %y\n", "try removed variable", try_removed_variable_float(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s float: %y\n", "assigned", use_float(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s float: %s\n", "assigned", ex.err);
    }
}
{
    C c();
    try {
        printf("%s auto: %y\n", "deleted member", deleted_auto(\c.m, c));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "deleted member", ex.err);
    }
    hash<string, int> h = {"k": 1};
    try {
        printf("%s auto: %y\n", "removed key", removed_key_auto(\h.k, \h));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "removed key", ex.err);
    }
    int v = 1;
    try {
        printf("%s auto: %y\n", "removed variable", removed_variable_auto(\v));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "removed variable", ex.err);
    }
    C tc();
    try {
        printf("%s auto: %y\n", "try deleted member", try_deleted_auto(\tc.m, tc));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "try deleted member", ex.err);
    }
    int tv = 1;
    try {
        printf("%s auto: %y\n", "try removed variable", try_removed_variable_auto(\tv));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "try removed variable", ex.err);
    }
    int w = 5;
    try {
        printf("%s auto: %y\n", "assigned", use_auto(\w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s auto: %s\n", "assigned", ex.err);
    }
}

# other target types and locals, methods, an overloaded call, and a loop in which a later iteration loses the target
string sub use_string(reference<string> p) { string x = p; return x; }
int sub use_softint(reference<*string> p) { softint x = p; return x; }
int sub use_timeout(reference<*date> p) { timeout x = p; return x; }
int sub use_bigint(reference<int> p) { int x = p; return x; }
string sub ov(int i) { return "ov(int)"; }
string sub ov(*string s) { return sprintf("ov(*string) %y", s); }
string sub use_overload(reference<int> p) { return ov(p); }
class RefUser {
    int meth(reference<int> p) { int x = p; return x; }
    static int smeth(reference<int> p) { int x = p; return x; }
}
string sub use_loop(reference<hash<string, int>> h) {
    string rv;
    for (int i = 0; i < 2; ++i) {
        reference<int> r = \h.k;
        try {
            int x = r;
            rv += sprintf("%d:%d ", i, x);
        } catch (hash<ExceptionInfo> ex) {
            rv += sprintf("%d:%s ", i, ex.err);
        }
        remove h.k;
    }
    return rv;
}

sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

{
    string rs;
    show("string unassigned", auto sub () { return use_string(\rs); });
    string rs2 = "abc";
    show("string assigned", auto sub () { return use_string(\rs2); });
    *string ss;
    show("softint unassigned", auto sub () { return use_softint(\ss); });
    *string ss2 = "12";
    show("softint assigned", auto sub () { return use_softint(\ss2); });
    *date dd;
    show("timeout unassigned", auto sub () { return use_timeout(\dd); });
    *date dd2 = 2s;
    show("timeout relative date", auto sub () { return use_timeout(\dd2); });
    int big = 9223372036854775807;
    show("big int", auto sub () { return use_bigint(\big); });
    int neg = -140737488355329;
    show("big negative int", auto sub () { return use_bigint(\neg); });
    int ou;
    show("overload unassigned", auto sub () { return use_overload(\ou); });
    int oa = 1;
    show("overload assigned", auto sub () { return use_overload(\oa); });
    RefUser u();
    int mu;
    show("method unassigned", auto sub () { return u.meth(\mu); });
    int smu;
    show("static method unassigned", auto sub () { return RefUser::smeth(\smu); });
    hash<string, int> lh = {"k": 4};
    show("loop", auto sub () { return use_loop(\lh); });
}
