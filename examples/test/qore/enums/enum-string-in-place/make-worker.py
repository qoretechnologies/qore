#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Generates enum-string-in-place-worker.qr from the operations and lvalue kinds below and worker-header.txt

usage: make-worker.py <output file> all
("subst" in the place of "all" makes the regex substitution and transliteration checks only)
"""
import os
import sys

OUT = sys.argv[1]
SET = sys.argv[2]

# name: (operation on "{}", whether it is a regex substitution or transliteration)
SUBST_OPS = [
    ("rs", "{} =~ s/a/X/"),
    ("rsg", "{} =~ s/ /_/g"),
    ("tl", "{} =~ tr/a-z/A-Z/"),
]
# operations whose value is used: the result is stored in "r", printed after the lvalue
SUBST_RV_OPS = [
    ("rs-rv", "r = ({} =~ s/a/X/)"),
    ("tl-rv", "r = ({} =~ tr/a-z/A-Z/)"),
]
OTHER_OPS = [
    ("pe", "{} += \"x\""),
    ("pe-int", "{} += 1"),
    ("sp", "splice {}, 0, 1"),
    ("sp-len", "splice {}, 1, 2"),
    ("sp-rep", "splice {}, 0, 1, \"zz\""),
    ("ex", "extract {}, 0, 1"),
    ("ex-rep", "extract {}, 1, 2, \"zz\""),
    ("tr", "trim {}"),
    ("ch", "chomp {}"),
    # a single character is only read: the lvalue is not changed
    ("rm", "remove {}[1]"),
    ("del", "delete {}[1]"),
    ("rm-list", "remove {}[1, 2]"),
    ("rm-range", "remove {}[1..2]"),
    ("del-list", "delete {}[1, 2]"),
    ("del-range", "delete {}[1..2]"),
]
OTHER_RV_OPS = [
    ("pe-rv", "r = ({} += \"x\")"),
    ("ex-rv", "r = (extract {}, 0, 2)"),
    ("tr-rv", "r = (trim {})"),
    ("ch-rv", "r = (chomp {})"),
    ("rm-rv", "r = (remove {}[1])"),
    ("rm-range-rv", "r = (remove {}[1..2])"),
]

OPS = SUBST_OPS + (OTHER_OPS if SET == "all" else [])
RV_OPS = SUBST_RV_OPS + (OTHER_RV_OPS if SET == "all" else [])

# enum prefix, type, member
ENUMS = [("ess", "ESS", "ESS::A"), ("esl", "ESL", "ESL::B")]

def kinds(p, t, init):
    # name: (setup lines, lvalue, value read afterwards)
    return [
        ("local", [f"{t} v = {init};"], "v", "v"),
        ("closure", [f"{t} v = {init};", "code get = auto sub () { return v; };"], "v", "get()"),
        ("global", [f"g_{p} = {init};"], f"g_{p}", f"g_{p}"),
        ("member", ["Holder o();"], f"o.{p}", f"o.{p}"),
        ("hashdecl", ["hash<HolderHash> h();"], f"h.{p}", f"h.{p}"),
        ("hash", [f"hash<string, {t}> h = {{\"k\": {init}}};"], "h.k", "h.k"),
        ("hash-dyn", [f"hash<string, {t}> h = {{\"k\": {init}}};", "string key = \"k\";"], "h{key}", "h{key}"),
        ("list", [f"list<{t}> l = ({init},);"], "l[0]", "l[0]"),
        ("ref", [f"{t} v = {init};", f"reference<{t}> rf = \\v;"], "rf", "v"),
    ]

# lvalues with no value: (name, setup, lvalue, value read afterwards)
NV_KINDS = [
    ("local", ["ESL v;"], "v", "v"),
    ("local-or-nothing", ["*ESL v;"], "v", "v"),
    ("member", ["Holder o();"], "o.eslnv", "o.eslnv"),
    ("member-or-nothing", ["Holder o();"], "o.eslonv", "o.eslonv"),
    ("hash", ["hash<string, *ESL> h = {\"k\": NOTHING};"], "h.k", "h.k"),
    ("global", ["remove g_eslnv;"], "g_eslnv", "g_eslnv"),
]

# controls, which the operations change: a string local and an auto local holding an enum value
CONTROLS = [
    ("string", ["string v = ESL::B;"], "v", "v"),
    ("auto", ["auto v = ESL::B;"], "v", "v"),
    ("auto-short", ["auto v = ESS::A;"], "v", "v"),
    ("auto-hash", ["hash<auto> h = {};", "h.k = ESL::B;"], "h.k", "h.k"),
]

out = []

def keyname(k):
    return k if "-" not in k else '"' + k + '"'

def check(indent, setup, op, lv, getter, key, rv):
    p = " " * indent
    k = keyname(key)
    lines = [p + "{"]
    for s in setup:
        lines.append(p + "    " + s)
    if rv:
        lines.append(p + "    auto r;")
        res_ok = f"ok({getter}) + \"|\" + q(r)"
        res_err = f"err(ex, {getter}) + \"|\" + q(r)"
    else:
        res_ok = f"ok({getter})"
        res_err = f"err(ex, {getter})"
    lines.append(p + f"    try {{ {op.format(lv)}; rv.{k} = {res_ok}; }}")
    lines.append(p + f"    catch (hash<ExceptionInfo> ex) {{ rv.{k} = {res_err}; }}")
    lines.append(p + "}")
    return lines

def group(fn, doc, body, group_name):
    out.append(f"#! {doc}")
    out.append(f"hash<string, string> sub {fn}() {{")
    out.append("    hash<string, string> rv;")
    out.extend(body)
    out.append("    return rv;")
    out.append("}")
    out.append("")
    main.append(f"print_results(\"{group_name}\", {fn}());")

main = []

for p, t, init in ENUMS:
    for kname, setup, lv, getter in kinds(p, t, init):
        body = []
        for opname, op in OPS:
            body += check(4, setup, op, lv, getter, opname, False)
        for opname, op in RV_OPS:
            body += check(4, setup, op, lv, getter, opname, True)
        fn = f"{p}_{kname.replace('-', '_')}_in"
        group(fn, f"each operation on a {t} lvalue ({kname}) in a try block", body, f"{p}-{kname}-in")

    # an operation in a method on a member of the object (the self member paths)
    body = []
    for opname, op in OPS:
        k = keyname(opname)
        body.append("    {")
        body.append("        Holder o();")
        body.append(f"        try {{ o.{p}_{opname.replace('-', '_')}(); rv.{k} = ok(o.{p}); }}")
        body.append(f"        catch (hash<ExceptionInfo> ex) {{ rv.{k} = err(ex, o.{p}); }}")
        body.append("    }")
    group(f"{p}_self_in", f"each operation on a {t} member in a method of its object", body, f"{p}-self-in")

    # an operation in a function with no try block, whose caller catches the error
    out.append(f"#! applies an operation to a {t} local with no try block")
    out.append(f"sub {p}_local_raise(string op) {{")
    out.append(f"    {t} v = {init};")
    out.append("    switch (op) {")
    for opname, op in OPS:
        out.append(f"        case \"{opname}\": {op.format('v')}; break;")
    out.append("    }")
    out.append("}")
    out.append("")
    body = []
    for opname, op in OPS:
        k = keyname(opname)
        body.append(f"    try {{ {p}_local_raise(\"{opname}\"); rv.{k} = \"none\"; }}")
        body.append(f"    catch (hash<ExceptionInfo> ex) {{ rv.{k} = err_out(ex); }}")
    group(f"{p}_local_out", f"each operation on a {t} local raised in a function that it calls", body,
        f"{p}-local-out")

    # an operation in a try block in a loop
    body = []
    for opname, op in OPS:
        k = keyname(opname)
        body.append("    {")
        body.append(f"        {t} v = {init};")
        body.append("        int caught = 0;")
        body.append("        for (int i = 0; i < 50; ++i) {")
        body.append(f"            try {{ {op.format('v')}; }} catch (hash<ExceptionInfo> ex) {{ ++caught; }}")
        body.append("        }")
        body.append(f"        rv.{k} = sprintf(\"%s (caught %d)\", ok(v), caught);")
        body.append("    }")
    group(f"{p}_local_loop", f"each operation on a {t} local in a try block in a loop", body, f"{p}-local-loop")

    # parameters
    body = []
    for opname, op in OPS:
        k = keyname(opname)
        fname = f"{p}_param_{opname.replace('-', '_')}"
        out.append(f"string sub {fname}({t} v) {{")
        out.append(f"    try {{ {op.format('v')}; return ok(v); }} catch (hash<ExceptionInfo> ex) {{ return err(ex, v); }}")
        out.append("}")
        rname = f"{p}_refparam_{opname.replace('-', '_')}"
        out.append(f"sub {rname}(reference<{t}> r) {{")
        out.append(f"    {op.format('r')};")
        out.append("}")
        body.append(f"    rv.{keyname('param-' + opname)} = {fname}({init});")
        body.append("    {")
        body.append(f"        {t} v = {init};")
        body.append(f"        try {{ {rname}(\\v); rv.{keyname('refparam-' + opname)} = ok(v); }}")
        body.append(f"        catch (hash<ExceptionInfo> ex) {{ rv.{keyname('refparam-' + opname)} = err(ex, v); }}")
        body.append("    }")
    out.append("")
    group(f"{p}_params", f"each operation on a {t} parameter and through a reference parameter", body, f"{p}-params")

for kname, setup, lv, getter in NV_KINDS:
    body = []
    for opname, op in OPS:
        body += check(4, setup, op, lv, getter, opname, False)
    for opname, op in RV_OPS:
        body += check(4, setup, op, lv, getter, opname, True)
    fn = f"eslnv_{kname.replace('-', '_')}_in"
    group(fn, f"each operation on an ESL lvalue with no value ({kname})", body, f"eslnv-{kname}-in")

for kname, setup, lv, getter in CONTROLS:
    body = []
    for opname, op in OPS:
        body += check(4, setup, op, lv, getter, opname, False)
    for opname, op in RV_OPS:
        body += check(4, setup, op, lv, getter, opname, True)
    fn = f"ctl_{kname.replace('-', '_')}_in"
    group(fn, f"each operation on a control lvalue ({kname})", body, f"{kname}-in")

if SET == "all":
    # trim and chomp of containers whose elements are enum values
    CONT = [
        ("hash", "hash<string, ESL> c = {\"a\": ESL::B, \"b\": ESL::B};", "c.a"),
        ("hash-short", "hash<string, ESS> c = {\"a\": ESS::A};", "c.a"),
        ("list", "list<ESL> c = (ESL::B, ESL::B);", "c[0]"),
        ("list-short", "list<ESS> c = (ESS::A,);", "c[0]"),
        ("list-or-nothing", "list<*ESL> c = (NOTHING, ESL::B);", "c[1]"),
        # the string member is first: it must not be changed when the enum member refuses the result
        ("hashdecl", "hash<MixedHash> c();", "c.s + \"/\" + c.e"),
        ("member-hash", "Holder o(); o.hesl = {\"a\": ESL::B};", "o.hesl.a"),
        ("auto-hash", "hash<auto> c = {}; c.a = ESL::B; c.s = \" s \";", ""),
        ("auto-list", "list<auto> c = (); push c, ESL::B; push c, \" s \";", ""),
    ]
    for cname, setup, getter in CONT:
        body = []
        target = "o.hesl" if cname == "member-hash" else "c"
        for opname, op in (("tr", "trim {}"), ("ch", "chomp {}"), ("tr-rv", "r = (trim {})"),
                ("ch-rv", "r = (chomp {})")):
            rv = opname.endswith("-rv")
            k = keyname(opname)
            body.append("    {")
            body.append("        " + setup)
            if rv:
                body.append("        auto r;")
            res_ok = f"okc({target})" + (" + \"|\" + q(r)" if rv else "")
            res_err = f"errc(ex, {target})" + (" + \"|\" + q(r)" if rv else "")
            body.append(f"        try {{ {op.format(target)}; rv.{k} = {res_ok}; }}")
            body.append(f"        catch (hash<ExceptionInfo> ex) {{ rv.{k} = {res_err}; }}")
            body.append("    }")
        fn = f"cont_{cname.replace('-', '_')}"
        group(fn, f"trim and chomp of a container ({cname})", body, f"cont-{cname}")

# methods of Holder for the self member checks
methods = []
for p, t, init in ENUMS:
    for opname, op in OPS:
        methods.append(f"    {p}_{opname.replace('-', '_')}() {{ {op.format(p)}; }}")

head = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "worker-header.txt")).read()
head = head.replace("@METHODS@", "\n".join(methods))
with open(OUT, "w") as f:
    f.write(head)
    f.write("\n".join(out))
    f.write("\n# the checks\n")
    f.write("\n".join(main))
    f.write("\n")
    # top-level code
    f.write("{\n    hash<string, string> rv;\n")
    for opname, op in OPS:
        for line in check(4, ["ESL v = ESL::B;"], op, "v", "v", opname, False):
            f.write(line + "\n")
    f.write("    print_results(\"esl-toplevel\", rv);\n}\n")
    f.write("printf(\"es-members=%s\\n\", replace(sprintf(\"%y %y\", ESS::A, ESL::B), \"\\n\", \"\\\\n\"));\n")
