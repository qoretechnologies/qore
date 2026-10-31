#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Assigns hashes and lists whose value type was narrowed at runtime to hash<auto>, hash<auto!>, list<auto> and
# list<auto!> lvalues of every kind, writes a value of another type into each, and prints one line per write giving
# its outcome.  A hash<auto!> or list<auto!> lvalue drops the narrowed type of the container assigned to it, but not
# of the containers nested in it; a hash<auto> or list<auto> lvalue keeps it.

%modern

our hash<auto!> g_hash_no_narrow;
our hash<auto> g_hash;
our list<auto!> g_list_no_narrow;
our list<auto> g_list;
our hash<auto!> g_hash_no_narrow_init = mk_hash({"a": "b"});
our hash<auto> g_hash_init = mk_hash({"a": "b"});

class Holder {
    public {
        hash<auto!> hash_no_narrow;
        hash<auto> hash_auto;
        list<auto!> list_no_narrow;
        list<auto> list_auto;
    }
}

hash<auto> sub mk_hash(hash<auto> h) {
    return h;
}

list<auto> sub mk_list(list<auto> l) {
    return l;
}

auto sub id(auto v) {
    return v;
}

#! true in the last run, which prints the outcomes
our bool print_results;

sub check(string label, code op) {
    string outcome;
    try {
        outcome = "ok " + op();
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    if (print_results) {
        printf("%s: %s\n", label, outcome);
    }
}

sub param_hash_no_narrow(hash<auto!> p) {
    check("hash<auto!> parameter", string sub () { p.n = id(1); return p.fullType(); });
}

sub param_hash(hash<auto> p) {
    check("hash<auto> parameter", string sub () { p.n = id(1); return p.fullType(); });
}

sub param_list_no_narrow(list<auto!> p) {
    check("list<auto!> parameter", string sub () { p += id(1); return p.fullType(); });
}

sub param_list(list<auto> p) {
    check("list<auto> parameter", string sub () { p += id(1); return p.fullType(); });
}

#! adds an int default to a caller's option hash with a hash<auto> local
hash<auto> sub defaults_auto(hash<auto> opts0) {
    hash<auto> opts = opts0;
    if (!exists opts.timeout) {
        opts.timeout = 30;
    }
    return opts;
}

#! adds an int default to a caller's option hash with a hash<auto!> local
hash<auto> sub defaults_no_narrow(hash<auto> opts0) {
    hash<auto!> opts = opts0;
    if (!exists opts.timeout) {
        opts.timeout = 30;
    }
    return opts;
}

#! runs every case; the cases are run three times so that tiered execution runs them as native code in the last run
sub run_cases() {
    hash<auto> src_hash = mk_hash({"a": "b"});
    list<auto> src_list = mk_list(("a", "b"));
    if (print_results) {
        printf("source types: %s %s\n", src_hash.fullType(), src_list.fullType());
    }

    hash<auto!> l_hash_no_narrow = src_hash;
    check("hash<auto!> local", string sub () { l_hash_no_narrow.n = id(1); return l_hash_no_narrow.fullType(); });
    hash<auto> l_hash = src_hash;
    check("hash<auto> local", string sub () { l_hash.n = id(1); return l_hash.fullType(); });
    list<auto!> l_list_no_narrow = src_list;
    check("list<auto!> local", string sub () { l_list_no_narrow += id(1); return l_list_no_narrow.fullType(); });
    list<auto> l_list = src_list;
    check("list<auto> local", string sub () { l_list += id(1); return l_list.fullType(); });

    g_hash_no_narrow = src_hash;
    check("hash<auto!> global", string sub () { g_hash_no_narrow.n = id(1); return g_hash_no_narrow.fullType(); });
    g_hash = src_hash;
    check("hash<auto> global", string sub () { g_hash.n = id(1); return g_hash.fullType(); });
    g_list_no_narrow = src_list;
    check("list<auto!> global", string sub () { g_list_no_narrow += id(1); return g_list_no_narrow.fullType(); });
    g_list = src_list;
    check("list<auto> global", string sub () { g_list += id(1); return g_list.fullType(); });

    check("hash<auto!> global initializer", string sub () {
        g_hash_no_narrow_init.n = id(1);
        return g_hash_no_narrow_init.fullType();
    });
    check("hash<auto> global initializer", string sub () { g_hash_init.n = id(1); return g_hash_init.fullType(); });

    Holder holder();
    holder.hash_no_narrow = src_hash;
    check("hash<auto!> member", string sub () { holder.hash_no_narrow.n = id(1); return holder.hash_no_narrow.fullType(); });
    holder.hash_auto = src_hash;
    check("hash<auto> member", string sub () { holder.hash_auto.n = id(1); return holder.hash_auto.fullType(); });
    holder.list_no_narrow = src_list;
    check("list<auto!> member", string sub () { holder.list_no_narrow += id(1); return holder.list_no_narrow.fullType(); });
    holder.list_auto = src_list;
    check("list<auto> member", string sub () { holder.list_auto += id(1); return holder.list_auto.fullType(); });

    param_hash_no_narrow(src_hash);
    param_hash(src_hash);
    param_list_no_narrow(src_list);
    param_list(src_list);

    # the type of the source is unchanged
    if (print_results) {
        printf("source types after: %s %s %y %y\n", src_hash.fullType(), src_list.fullType(), src_hash,
            src_list);
    }

    # only the assigned container loses its narrowed type, not the containers it holds
    hash<auto!> nested = mk_hash({"inner": {"a": "b"}});
    check("hash<auto!> nested top level", string sub () { nested.n = id(1); return nested.fullType(); });
    check("hash<auto!> nested value", string sub () { nested.inner.n = id(1); return nested.inner.fullType(); });

    # a caller's literal option hash is hash<string, string>
    check("hash<auto> option defaults", string sub () { return sprintf("%y", defaults_auto({"url": "x"})); });
    check("hash<auto!> option defaults", string sub () { return sprintf("%y", defaults_no_narrow({"url": "x"})); });
}

for (int i = 0; i < 3; ++i) {
    print_results = (i == 2);
    run_cases();
}
