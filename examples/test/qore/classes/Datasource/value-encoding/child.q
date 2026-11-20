%modern
%enable-all-warnings

# Copyright 2026 Qore Technologies, s.r.o.
#
# run by value-encoding.qtest with QORE_CHARSET=ISO-8859-1: a datasource keeps its connection values (user name,
# password, database name, database encoding name, hostname) as bytes in the default encoding, which its drivers read
# them in; a value given in another encoding must be converted to the default encoding when it is set, through every
# entry point, and a value that the default encoding cannot represent must raise ENCODING-CONVERSION-ERROR instead of
# being kept as bytes that its encoding tag misdescribes (the jdbc driver of module-jni got the UTF-8 bytes of a
# database name built from a UTF-8 string literal and converted them to Java as ISO-8859-1)
#
# prints "ok: <case>" for each check that passes or "FAIL: <case>: <details>", then "done: <number of checks>"; this
# file has ASCII text only, and its values are built with explicit encodings, so that it means the same with any
# encoding of string literals

#! "f", a-umlaut, sharp s in UTF-8
const Utf8Bytes = <66c3a4c39f>;
#! the same in ISO-8859-1
const Latin1Bytes = <66e4df>;
#! U+1F600 in UTF-8, which ISO-8859-1 cannot represent
const EmojiBytes = <f09f9880>;

our int checks = 0;

#! returns the string in UTF-8
string sub u(string str) {
    return convert_encoding(str, "UTF-8");
}

#! "fäß" as a UTF-8 string
string sub fass() {
    return binary_to_string(Utf8Bytes, "UTF-8");
}

#! U+1F600 as a UTF-8 string
string sub emoji() {
    return binary_to_string(EmojiBytes, "UTF-8");
}

sub ok(string name, bool cond, string details = "") {
    ++checks;
    if (cond) {
        printf("ok: %s\n", name);
    } else {
        printf("FAIL: %s: %s\n", name, details);
    }
}

#! checks that a value is "fäß" in the default encoding: tagged ISO-8859-1, with the ISO-8859-1 bytes
sub check_value(string name, auto val) {
    if (val.typeCode() != NT_STRING) {
        ok(name, False, sprintf("expected a string; got %s", val.type()));
        return;
    }
    string str = val;
    ok(name, str.encoding() == "ISO-8859-1" && binary(str) == Latin1Bytes,
        sprintf("expected ISO-8859-1 %s; got %s %s", make_hex_string(Latin1Bytes), str.encoding(),
            make_hex_string(binary(str))));
}

#! checks the connection values of a datasource or a pool, with or without its encoding name
sub check_values(string name, object ds, bool charset = True) {
    check_value(name + ": user name", ds.getUserName());
    check_value(name + ": password", ds.getPassword());
    check_value(name + ": database name", ds.getDBName());
    if (charset) {
        check_value(name + ": encoding", ds.getDBEncoding());
        check_value(name + ": encoding (charset)", ds.getDBCharset());
    }
    check_value(name + ": hostname", ds.getHostName());
    hash<auto> conf = ds.getConfigHash();
    foreach string key in (("user", "pass", "db", "host") + (charset ? ("charset",) : ())) {
        check_value(sprintf("%s: config hash %y", name, key), conf{key});
    }
    # the configuration string is in the default encoding, and parsed again gives the same values
    string cs = ds.getConfigString();
    ok(name + ": config string encoding", cs.encoding() == "ISO-8859-1", cs.encoding());
    hash<auto> parsed = parse_datasource(cs);
    foreach string key in (("user", "pass", "db", "host") + (charset ? ("charset",) : ())) {
        check_value(sprintf("%s: config string %y", name, key), parsed{key});
    }
}

#! a datasource string with "fäß" in each connection value, in UTF-8
string sub ds_string(bool charset = True, string driver = "dbitest") {
    string f = fass();
    return u(driver + ":" + f + "/" + f + "@" + f + (charset ? "(" + f + ")" : "") + "%" + f);
}

#! checks that the code raises ENCODING-CONVERSION-ERROR for a value that the default encoding cannot represent,
#! naming the value but not showing it
sub check_unrepresentable(string name, string what, code c) {
    try {
        c();
        ok(name, False, "no exception was raised");
    } catch (hash<ExceptionInfo> ex) {
        ok(name, ex.err == "ENCODING-CONVERSION-ERROR"
                && u(ex.desc) == sprintf("the %s cannot be converted from \"UTF-8\" to the default encoding "
                    "\"ISO-8859-1\", in which datasource values are kept", what),
            sprintf("%s: %s", ex.err, u(ex.desc)));
    }
}

sub datasource_cases() {
    string f = fass();
    {
        Datasource ds("dbitest", f, f, f, f, f);
        check_values("Datasource constructor arguments", ds);
    }
    {
        Datasource ds(ds_string());
        check_values("Datasource datasource string", ds);
    }
    {
        # four arguments: the variant with the driver argument, which can be a datasource string
        Datasource ds(ds_string(), NOTHING, NOTHING, NOTHING);
        check_values("Datasource driver argument as a datasource string", ds);
    }
    {
        Datasource ds({"type": u("dbitest"), "user": f, "pass": f, "db": f, "charset": f, "host": f});
        check_values("Datasource hash", ds);
    }
    {
        Datasource ds({"type": "dbitest"});
        ds.setUserName(f);
        ds.setPassword(f);
        ds.setDBName(f);
        ds.setDBEncoding(f);
        ds.setHostName(f);
        check_values("Datasource setters", ds);
        ds.setDBEncoding("x");
        ds.setDBCharset(f);
        check_value("Datasource setDBCharset()", ds.getDBEncoding());
    }
    {
        # a deserialized datasource: the configuration hash and the legacy configuration string
        Datasource ds({"type": "dbitest"});
        hash<SerializationInfo> data = ds.serializeToData();
        data._index."0"._class_data."Qore::SQL::Datasource".conf = {"type": u("dbitest"), "user": f, "pass": f,
            "db": f, "charset": f, "host": f};
        Datasource restored = Serializable::deserialize(data);
        check_values("Datasource deserialized from a hash", restored);
        data._index."0"._class_data."Qore::SQL::Datasource".conf = ds_string();
        restored = Serializable::deserialize(data);
        check_values("Datasource deserialized from a string", restored);
    }
    {
        # an option name is a hash key in the default encoding
        Datasource ds({"type": "dbitest"});
        try {
            ds.setOption(f, 1);
            ok("Datasource option name", False, "no exception was raised");
        } catch (hash<ExceptionInfo> ex) {
            ok("Datasource option name", ex.err == "DBI-OPTION-ERROR" && u(ex.desc).find(f) != -1,
                sprintf("%s: %s", ex.err, u(ex.desc)));
        }
    }

    string e = emoji();
    check_unrepresentable("Datasource constructor user name", "user name",
        sub () { Datasource ds("dbitest", e); });
    check_unrepresentable("Datasource constructor password", "password",
        sub () { Datasource ds("dbitest", "u", e); });
    check_unrepresentable("Datasource constructor database name", "database name",
        sub () { Datasource ds("dbitest", "u", "p", e); });
    check_unrepresentable("Datasource constructor encoding", "encoding",
        sub () { Datasource ds("dbitest", "u", "p", "db", e); });
    check_unrepresentable("Datasource constructor hostname", "hostname",
        sub () { Datasource ds("dbitest", "u", "p", "db", NOTHING, e); });
    check_unrepresentable("Datasource constructor driver", "driver or datasource string",
        sub () { Datasource ds(u("dbitest:u/") + e + "@db", NOTHING, NOTHING, NOTHING); });
    check_unrepresentable("Datasource datasource string", "datasource string",
        sub () { Datasource ds(u("dbitest:u/") + e + "@db"); });
    check_unrepresentable("Datasource hash", "'pass' value",
        sub () { Datasource ds({"type": "dbitest", "pass": e, "db": "db"}); });
    {
        Datasource ds({"type": "dbitest"});
        check_unrepresentable("Datasource setUserName()", "user name", sub () { ds.setUserName(e); });
        check_unrepresentable("Datasource setPassword()", "password", sub () { ds.setPassword(e); });
        check_unrepresentable("Datasource setDBName()", "database name", sub () { ds.setDBName(e); });
        check_unrepresentable("Datasource setDBEncoding()", "encoding", sub () { ds.setDBEncoding(e); });
        check_unrepresentable("Datasource setDBCharset()", "encoding", sub () { ds.setDBCharset(e); });
        check_unrepresentable("Datasource setHostName()", "hostname", sub () { ds.setHostName(e); });
        check_unrepresentable("Datasource getOption()", "option name", sub () { ds.getOption(e); });
        check_unrepresentable("Datasource setOption()", "option name", sub () { ds.setOption(e, 1); });
        # a value that is refused is not kept: the values set before are unchanged
        ds.setUserName(fass());
        check_unrepresentable("Datasource setUserName() again", "user name", sub () { ds.setUserName(e); });
        check_value("Datasource user name kept after a refused value", ds.getUserName());
    }
}

sub pool_cases() {
    string f = fass();
    {
        DatasourcePool pool("dbitest", f, f, f, f, f, 1, 2);
        check_values("DatasourcePool constructor arguments", pool);
    }
    {
        DatasourcePool pool(ds_string());
        check_values("DatasourcePool datasource string", pool);
    }
    {
        # the variant with the driver argument, which can be a datasource string
        DatasourcePool pool(ds_string(), NOTHING, NOTHING, NOTHING, NOTHING, NOTHING);
        check_values("DatasourcePool driver argument as a datasource string", pool);
    }
    {
        DatasourcePool pool({"type": u("dbitest"), "user": f, "pass": f, "db": f, "charset": f, "host": f});
        check_values("DatasourcePool hash", pool);
    }
    {
        DatasourcePool pool("dbitest", "u", "p", "db", NOTHING, NOTHING, 1, 2);
        hash<SerializationInfo> data = pool.serializeToData();
        data._index."0"._class_data."Qore::SQL::DatasourcePool".conf = {"type": u("dbitest"), "user": f, "pass": f,
            "db": f, "charset": f, "host": f};
        DatasourcePool restored = Serializable::deserialize(data);
        check_values("DatasourcePool deserialized from a hash", restored);
        data._index."0"._class_data."Qore::SQL::DatasourcePool".conf = ds_string();
        restored = Serializable::deserialize(data);
        check_values("DatasourcePool deserialized from a string", restored);

        try {
            pool.getOption(f);
            ok("DatasourcePool option name", False, "no exception was raised");
        } catch (hash<ExceptionInfo> ex) {
            ok("DatasourcePool option name", ex.err == "DBI-OPTION-ERROR" && u(ex.desc).find(f) != -1,
                sprintf("%s: %s", ex.err, u(ex.desc)));
        }
        check_unrepresentable("DatasourcePool getOption()", "option name", sub () { pool.getOption(emoji()); });
    }

    string e = emoji();
    check_unrepresentable("DatasourcePool constructor user name", "user name",
        sub () { DatasourcePool pool("dbitest", e, "p", "db", NOTHING, NOTHING, 1, 2); });
    check_unrepresentable("DatasourcePool constructor password", "password",
        sub () { DatasourcePool pool("dbitest", "u", e, "db", NOTHING, NOTHING, 1, 2); });
    check_unrepresentable("DatasourcePool constructor database name", "database name",
        sub () { DatasourcePool pool("dbitest", "u", "p", e, NOTHING, NOTHING, 1, 2); });
    check_unrepresentable("DatasourcePool constructor encoding", "encoding",
        sub () { DatasourcePool pool("dbitest", "u", "p", "db", e, NOTHING, 1, 2); });
    check_unrepresentable("DatasourcePool constructor hostname", "hostname",
        sub () { DatasourcePool pool("dbitest", "u", "p", "db", NOTHING, e, 1, 2); });
    check_unrepresentable("DatasourcePool datasource string", "datasource string",
        sub () { DatasourcePool pool(u("dbitest:u/") + e + "@db"); });
    check_unrepresentable("DatasourcePool hash", "'user' value",
        sub () { DatasourcePool pool({"type": "dbitest", "user": e, "db": "db"}); });
}

sub parse_cases() {
    hash<auto> h = parse_datasource(ds_string());
    foreach string key in ("user", "pass", "db", "charset", "host") {
        check_value(sprintf("parse_datasource() %y", key), h{key});
    }
    check_value("parse_datasource() driver", parse_datasource(ds_string(True, fass())).type);
    # the deprecated function is called by name, which raises no deprecation warning
    h = call_function("parseDatasource", ds_string());
    foreach string key in ("user", "pass", "db", "charset", "host") {
        check_value(sprintf("parseDatasource() %y", key), h{key});
    }
    # option names and values
    string f = fass();
    h = parse_datasource(u("dbitest:u/p@db{") + f + "=" + f + "}");
    check_value("parse_datasource() option name", (keys h.options)[0]);
    check_value("parse_datasource() option value", h.options.firstValue());

    check_unrepresentable("parse_datasource()", "datasource string",
        auto sub () { return parse_datasource(u("dbitest:u/") + emoji() + "@db"); });
    check_unrepresentable("parseDatasource()", "datasource string",
        auto sub () { return call_function("parseDatasource", u("dbitest:u/") + emoji() + "@db"); });
}

printf("default: %s\n", get_default_encoding());
datasource_cases();
pool_cases();
parse_cases();
printf("done: %d\n", checks);
