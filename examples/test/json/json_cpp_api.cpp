// Phase 1 verification: exercise the public JSON C++ API the way modules/avro will,
// i.e. linking libqore only, with no json module loaded and no Qore-language code.
#include <qore/Qore.h>
#include <qore/QoreJson.h>

#include <cassert>
#include <cstdio>
#include <cstring>

static int failures = 0;

static void check(bool ok, const char* what) {
    printf("%-58s %s\n", what, ok ? "OK" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

int main() {
    qore_init(QL_MIT);
    {
        ExceptionSink xsink;

        // round trip: parse a document covering every JSON type
        QoreString src("{\"i\":42,\"f\":1.5,\"b\":true,\"n\":null,"
            "\"s\":\"a\\\"b\\\\c\\u00e9\\u0041\",\"l\":[1,\"two\",false,{\"k\":[]}]}");
        ValueHolder v(parse_json(&src, &xsink), &xsink);
        check(!xsink, "parse_json(): no exception");
        check(v->getType() == NT_HASH, "parse_json(): returns a hash");

        const QoreHashNode* h = v->get<const QoreHashNode>();
        check(h->getKeyValue("i").getAsBigInt() == 42, "parse_json(): integer value");
        check(h->getKeyValue("f").getAsFloat() == 1.5, "parse_json(): float value");
        check(h->getKeyValue("b").getAsBool(), "parse_json(): boolean value");
        check(h->getKeyValue("n").isNothing(), "parse_json(): null -> NOTHING");

        // escapes, including a non-ASCII \u codepoint that exercises the
        // ctype-on-signed-char path in the number/whitespace scanners
        const QoreValue sv = h->getKeyValue("s");
        check(sv.getType() == NT_STRING, "parse_json(): string value");
        check(!strcmp(sv.get<const QoreStringNode>()->c_str(), "a\"b\\c\xc3\xa9" "A"),
            "parse_json(): string escapes and \\u decoding");

        const QoreValue lv = h->getKeyValue("l");
        check(lv.getType() == NT_LIST, "parse_json(): list value");
        check(lv.get<const QoreListNode>()->size() == 4, "parse_json(): list size");

        // serialize the parsed value back and re-parse it: must be identical
        QoreStringNodeHolder out(make_json(*v, JGF_NONE, nullptr, &xsink));
        check(!xsink && out, "make_json(): no exception");
        check(out->getEncoding() == QCS_UTF8, "make_json(): defaults to UTF-8");
        ValueHolder v2(parse_json(*out, &xsink), &xsink);
        check(!xsink, "make_json() -> parse_json(): round trip parses");
        check(v->isEqualHard(*v2), "make_json() -> parse_json(): round trip is lossless");

        // formatted output must differ textually but parse to the same value
        QoreStringNodeHolder fmt(make_json(*v, JGF_ADD_FORMATTING, nullptr, &xsink));
        check(!xsink && fmt, "make_json(JGF_ADD_FORMATTING): no exception");
        check(strchr(fmt->c_str(), '\n'), "make_json(JGF_ADD_FORMATTING): emits line breaks");
        ValueHolder v3(parse_json(*fmt, &xsink), &xsink);
        check(!xsink && v->isEqualHard(*v3), "formatted output parses to the same value");

        // append API: compose a document from separately-serialized parts
        QoreString doc(QCS_UTF8);
        doc.concat("{\"wrapped\":");
        check(!json_serialize_value(doc, *v, -1, &xsink), "json_serialize_value(): returns 0");
        doc.concat("}");
        ValueHolder wrapped(parse_json(&doc, &xsink), &xsink);
        check(!xsink && wrapped->getType() == NT_HASH, "json_serialize_value(): composed doc parses");
        check(wrapped->get<const QoreHashNode>()->getKeyValue("wrapped").get<const QoreHashNode>()
            ->getKeyValue("i").getAsBigInt() == 42, "json_serialize_value(): nested value survives");

        // list append with an offset, as the JSON-RPC builders use it
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), &xsink);
        l->push(1, &xsink);
        l->push(2, &xsink);
        l->push(3, &xsink);
        QoreString ldoc(QCS_UTF8);
        check(!json_serialize_list(ldoc, *l, -1, &xsink, 1), "json_serialize_list(): returns 0");
        check(!strcmp(ldoc.c_str(), "[2,3]"), "json_serialize_list(): offset skips leading elements");

        // non-UTF-8 output encoding
        QoreStringNodeHolder l1(make_json(*v, JGF_NONE, QEM.findCreate("ISO-8859-1"), &xsink));
        check(!xsink && l1 && l1->getEncoding() == QEM.findCreate("ISO-8859-1"),
            "make_json(): honours the requested encoding");

        // negative: trailing garbage after a complete value
        QoreString bad("{\"a\":1} trailing");
        ValueHolder bv(parse_json(&bad, &xsink), &xsink);
        check(xsink.isException(), "parse_json(): rejects trailing text");
        xsink.clear();

        // negative: truncated document
        QoreString trunc("{\"a\":");
        ValueHolder tv(parse_json(&trunc, &xsink), &xsink);
        check(xsink.isException(), "parse_json(): rejects truncated input");
        xsink.clear();

        // negative: unserializable value (an object) must raise, not crash
        QoreString empty("");
        ValueHolder ev(parse_json(&empty, &xsink), &xsink);
        check(!xsink && ev->isNothing(), "parse_json(): empty input -> NOTHING, no exception");

        // BOM handling
        QoreString bom("\xEF\xBB\xBF{\"a\":1}");
        ValueHolder bomv(parse_json(&bom, &xsink), &xsink);
        check(!xsink && bomv->getType() == NT_HASH, "parse_json(): skips a UTF-8 BOM");

        // deeply nested but well-formed input round-trips.  NB the
        // JSON_MAX_NESTING_DEPTH cap is sandbox-only -- QoreSandboxManagerHelper yields
        // nullptr with no current QoreProgram, so it is not exercised here; that belongs
        // in the Qore-language suite, which runs inside a Program.
        const unsigned depth = 200;
        QoreString deep;
        for (unsigned i = 0; i < depth; ++i) {
            deep.concat('[');
        }
        deep.concat('1');
        for (unsigned i = 0; i < depth; ++i) {
            deep.concat(']');
        }
        ValueHolder dv(parse_json(&deep, &xsink), &xsink);
        check(!xsink && dv->getType() == NT_LIST, "parse_json(): deep well-formed input parses");
        QoreStringNodeHolder deep_out(make_json(*dv, JGF_NONE, nullptr, &xsink));
        check(!xsink && deep_out && !strcmp(deep_out->c_str(), deep.c_str()),
            "make_json(): deep round trip is byte-identical");

        // unbalanced nesting is a parse error, not a crash
        QoreString unbalanced;
        for (unsigned i = 0; i < depth; ++i) {
            unbalanced.concat('[');
        }
        ValueHolder uv(parse_json(&unbalanced, &xsink), &xsink);
        check(xsink.isException(), "parse_json(): rejects unbalanced nesting");
        xsink.clear();
    }
    qore_cleanup();

    printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
