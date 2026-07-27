// Exercises the JSON C++ API the way modules/avro does: linking libqore only, including only
// installed public headers, resolving the json module's QoreJsonApi struct at run time through
// q_get_module_cpp_api(), and running no Qore-language code at all.
//
// This is not built by default; it needs the json module on the module path, so build and run it
// against the build tree:
//
//   cmake --build build --target qore-json-cpp-api-smoke
//   QORE_MODULE_DIR=build/modules/json LD_LIBRARY_PATH=build build/qore-json-cpp-api-smoke
//
// Without QORE_MODULE_DIR the installed json module is used, which tests the installed contract
// instead of the build tree.
#include <qore/Qore.h>
#include <qore/QoreJsonApi.h>
#include <qore/QoreModuleCppApi.h>

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

        // resolve the API exactly as a consuming binary module does; this loads the json module
        const QoreJsonApi* json = qore_json_api(&xsink);
        check(!xsink && json, "qore_json_api(): resolves the json module's C++ API");
        if (!json) {
            xsink.handleExceptions();
            qore_cleanup();
            printf("\nFAILURES\n");
            return 1;
        }
        check(json->hdr.major == QORE_JSON_CPP_API_MAJOR, "resolved struct: major version matches");
        check(json->hdr.minor >= QORE_JSON_CPP_API_MINOR, "resolved struct: minor version is sufficient");

        // a version the module cannot serve must be a clean exception, not a bad pointer
        check(!q_get_module_cpp_api("json", QORE_JSON_CPP_API_MAJOR + 1, 0, &xsink) && xsink,
            "q_get_module_cpp_api(): rejects an unsupported major version");
        xsink.clear();

        // round trip: parse a document covering every JSON type
        QoreString src("{\"i\":42,\"f\":1.5,\"b\":true,\"n\":null,"
            "\"s\":\"a\\\"b\\\\c\\u00e9\\u0041\",\"l\":[1,\"two\",false,{\"k\":[]}]}");
        ValueHolder v(json->parse(src, &xsink), &xsink);
        check(!xsink, "parse(): no exception");
        check(v->getType() == NT_HASH, "parse(): returns a hash");

        const QoreHashNode* h = v->get<const QoreHashNode>();
        check(h->getKeyValue("i").getAsBigInt() == 42, "parse(): integer value");
        check(h->getKeyValue("f").getAsFloat() == 1.5, "parse(): float value");
        check(h->getKeyValue("b").getAsBool(), "parse(): boolean value");
        check(h->getKeyValue("n").isNothing(), "parse(): null -> NOTHING");

        // escapes, including a non-ASCII \u codepoint that exercises the
        // ctype-on-signed-char path in the number/whitespace scanners
        const QoreValue sv = h->getKeyValue("s");
        check(sv.getType() == NT_STRING, "parse(): string value");
        check(!strcmp(sv.get<const QoreStringNode>()->c_str(), "a\"b\\c\xc3\xa9" "A"),
            "parse(): string escapes and \\u decoding");

        const QoreValue lv = h->getKeyValue("l");
        check(lv.getType() == NT_LIST, "parse(): list value");
        check(lv.get<const QoreListNode>()->size() == 4, "parse(): list size");

        // serialize the parsed value back and re-parse it: must be identical
        QoreStringNodeHolder out(json->generate(*v, JGF_NONE, nullptr, &xsink));
        check(!xsink && out, "generate(): no exception");
        check(out->getEncoding() == QCS_UTF8, "generate(): defaults to UTF-8");
        ValueHolder v2(json->parse(**out, &xsink), &xsink);
        check(!xsink, "generate() -> parse(): round trip parses");
        check(v->isEqualHard(*v2), "generate() -> parse(): round trip is lossless");

        // formatted output must differ textually but parse to the same value
        QoreStringNodeHolder fmt(json->generate(*v, JGF_ADD_FORMATTING, nullptr, &xsink));
        check(!xsink && fmt, "generate(JGF_ADD_FORMATTING): no exception");
        check(strchr(fmt->c_str(), '\n'), "generate(JGF_ADD_FORMATTING): emits line breaks");
        ValueHolder v3(json->parse(**fmt, &xsink), &xsink);
        check(!xsink && v->isEqualHard(*v3), "formatted output parses to the same value");

        // append API: compose a document from separately-serialized parts
        QoreString doc(QCS_UTF8);
        doc.concat("{\"wrapped\":");
        check(!json->serialize_value(doc, *v, -1, &xsink), "serialize_value(): returns 0");
        doc.concat("}");
        ValueHolder wrapped(json->parse(doc, &xsink), &xsink);
        check(!xsink && wrapped->getType() == NT_HASH, "serialize_value(): composed doc parses");
        check(wrapped->get<const QoreHashNode>()->getKeyValue("wrapped").get<const QoreHashNode>()
            ->getKeyValue("i").getAsBigInt() == 42, "serialize_value(): nested value survives");

        // list append with an offset, as the JSON-RPC builders use it
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), &xsink);
        l->push(1, &xsink);
        l->push(2, &xsink);
        l->push(3, &xsink);
        QoreString ldoc(QCS_UTF8);
        check(!json->serialize_list(ldoc, *l, -1, &xsink, 1), "serialize_list(): returns 0");
        check(!strcmp(ldoc.c_str(), "[2,3]"), "serialize_list(): offset skips leading elements");

        // non-UTF-8 output encoding
        QoreStringNodeHolder l1(json->generate(*v, JGF_NONE, QEM.findCreate("ISO-8859-1"), &xsink));
        check(!xsink && l1 && l1->getEncoding() == QEM.findCreate("ISO-8859-1"),
            "generate(): honours the requested encoding");

        // negative: trailing garbage after a complete value
        QoreString bad("{\"a\":1} trailing");
        ValueHolder bv(json->parse(bad, &xsink), &xsink);
        check(xsink.isException(), "parse(): rejects trailing text");
        xsink.clear();

        // negative: truncated document
        QoreString trunc("{\"a\":");
        ValueHolder tv(json->parse(trunc, &xsink), &xsink);
        check(xsink.isException(), "parse(): rejects truncated input");
        xsink.clear();

        // empty input is not an error
        QoreString empty("");
        ValueHolder ev(json->parse(empty, &xsink), &xsink);
        check(!xsink && ev->isNothing(), "parse(): empty input -> NOTHING, no exception");

        // BOM handling
        QoreString bom("\xEF\xBB\xBF{\"a\":1}");
        ValueHolder bomv(json->parse(bom, &xsink), &xsink);
        check(!xsink && bomv->getType() == NT_HASH, "parse(): skips a UTF-8 BOM");

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
        ValueHolder dv(json->parse(deep, &xsink), &xsink);
        check(!xsink && dv->getType() == NT_LIST, "parse(): deep well-formed input parses");
        QoreStringNodeHolder deep_out(json->generate(*dv, JGF_NONE, nullptr, &xsink));
        check(!xsink && deep_out && !strcmp(deep_out->c_str(), deep.c_str()),
            "generate(): deep round trip is byte-identical");

        // unbalanced nesting is a parse error, not a crash
        QoreString unbalanced;
        for (unsigned i = 0; i < depth; ++i) {
            unbalanced.concat('[');
        }
        ValueHolder uv(json->parse(unbalanced, &xsink), &xsink);
        check(xsink.isException(), "parse(): rejects unbalanced nesting");
        xsink.clear();
    }
    qore_cleanup();

    printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
