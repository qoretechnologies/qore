%modern
%allow-weak-references
%requires json
%requires yaml
%requires avro
%requires dataframe
# the program of weak-reference-module-read-during-free.qtest: a binary module reads a list or hash that it reaches
# through a weak reference while another thread frees it as freed, as the language does (qore#7231); every execution
# mode must print the same output
#
# Each case holds the thread that releases the last reference to the container at the start of its free with a debug
# hook (dbg_hold_container_free()), has a module serialize or key a value that holds a weak reference to it, then lets
# the free finish.  The modules read the target with QoreValue::resolveIndirectRef(), which reads a list or hash whose
# free has started as an empty one; before the fix they read it borrowed with QoreValue::resolveIndirect() and walked
# the entries that the free was releasing (the entries "s0".."s4" were printed, and the dataframe counted the value as
# equal to a copy of the container as it was before its free).

class Holder {
    public {
        list<auto> l;
        hash<auto> h;
    }
}

list<auto> sub mklist(int n) {
    list<auto> r = ();
    for (int i = 0; i < n; ++i) {
        r += sprintf("s%d", i);
    }
    return r;
}

hash<auto> sub mkhash(int n) {
    hash<auto> r = {};
    for (int i = 0; i < n; ++i) {
        r{"k" + i} = sprintf("s%d", i);
    }
    return r;
}

#! runs \a free in a thread that is held when the free of a container of the given type starts, then \a read, then
#! lets the free finish
sub during_free(string type, code free, code read) {
    Counter held(1);
    Counter release(1);
    Counter done(1);
    call_function("dbg_hold_container_free", type, held, release);
    background sub () {
        on_exit done.dec();
        free();
    }();
    held.waitForZero();
    on_exit {
        release.dec();
        done.waitForZero();
    }
    read();
}

#! has \a read read a list whose only element is a weak reference to a list that is being freed
sub list_case(string label, code read) {
    Holder c();
    c.l = mklist(5);
    list<auto!> holder = (1,);
    holder[0] := c.l;
    auto result;
    during_free("list", sub () { c.l = (); }, sub () { result = read(holder); });
    printf("%s list: %y\n", label, result);
}

#! has \a read read a hash whose only value is a weak reference to a hash that is being freed
sub hash_case(string label, code read) {
    Holder c();
    c.h = mkhash(5);
    hash<auto!> holder = {"a": 1};
    holder.a := c.h;
    auto result;
    during_free("hash", sub () { c.h = {}; }, sub () { result = read(holder); });
    printf("%s hash: %y\n", label, result);
}

#! writes a value as one YAML document with YamlStreamWriter
string sub yaml_stream(auto data) {
    StringOutputStream sos();
    YamlStreamWriter writer(sos);
    writer.startDocument();
    writer.write(data);
    writer.endDocument();
    writer.close();
    return sos.getData();
}

#! counts the unique values of a column that holds a weak reference to a list that is being freed and a copy of the
#! list as it was before its free (the key of a value of a column that holds any value)
sub dataframe_list_case() {
    Holder c();
    c.l = mklist(5);
    list<auto> k = (NOTHING, mklist(5));
    k[0] := c.l;
    DataFrame::DataFrame df({"k": k});
    int n;
    during_free("list", sub () { c.l = (); }, sub () { n = df.nunique("k"); });
    printf("dataframe list: %d\n", n);
}

#! as dataframe_list_case() for a hash
sub dataframe_hash_case() {
    Holder c();
    c.h = mkhash(5);
    list<auto> k = (NOTHING, mkhash(5));
    k[0] := c.h;
    DataFrame::DataFrame df({"k": k});
    int n;
    during_free("hash", sub () { c.h = {}; }, sub () { n = df.nunique("k"); });
    printf("dataframe hash: %d\n", n);
}

# json: make_json()
list_case("json", string sub (auto v) { return make_json(v); });
hash_case("json", string sub (auto v) { return make_json(v); });

# yaml: make_yaml() (QoreYamlEmitter) and YamlStreamWriter
list_case("yaml", auto sub (auto v) { return parse_yaml(make_yaml(v)); });
hash_case("yaml", auto sub (auto v) { return parse_yaml(make_yaml(v)); });
list_case("yaml stream", auto sub (auto v) { return parse_yaml(yaml_stream(v)); });
hash_case("yaml stream", auto sub (auto v) { return parse_yaml(yaml_stream(v)); });

# avro: AvroSchema::encode()
AvroSchema avro_list_schema('{"type": "array", "items": {"type": "array", "items": "string"}}');
list_case("avro", auto sub (auto v) { return avro_list_schema.decode(avro_list_schema.encode(v)); });
AvroSchema avro_hash_schema('{"type": "map", "values": {"type": "map", "values": "string"}}');
hash_case("avro", auto sub (auto v) { return avro_hash_schema.decode(avro_hash_schema.encode(v)); });

# dataframe: DataFrame::nunique()
dataframe_list_case();
dataframe_hash_case();
