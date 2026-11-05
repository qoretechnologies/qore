// standalone check of the isolated fast-route core (lib/HttpFastRoutes.cpp), not part of the repository
#include "qore/intern/HttpFastRoutes.h"
#include "qore/intern/HttpFastRequest.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
#include <random>
#include <thread>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static std::shared_ptr<const HttpFastResponse> resp(int code, const std::string& body) {
    std::string err;
    auto r = HttpFastResponse::create(code, {{"Content-Type", "text/plain"}, {"Server", "t"}}, body, err);
    if (!r) {
        printf("response error: %s\n", err.c_str());
    }
    return r;
}

static HttpFastRequestView req(const char* path = "/ready") {
    HttpFastRequestView v;
    v.method = "GET";
    v.path = path;
    v.http_version = "1.1";
    return v;
}

int main() {
    std::string err;
    // --- response serialization and validation
    auto ok = resp(200, "ok");
    CHECK(ok);
    auto wire = ok->getWire(1730000000);
    CHECK(wire->find("HTTP/1.1 200 OK\r\n") == 0);
    CHECK(wire->find("Date: Sun, 27 Oct 2024 03:33:20 GMT\r\n") != std::string::npos);
    CHECK(wire->find("Content-Length: 2\r\n") != std::string::npos);
    CHECK(wire->find("Connection: Keep-Alive\r\n") != std::string::npos);
    CHECK(wire->substr(wire->size() - 6) == "\r\n\r\nok");
    CHECK(ok->getWire(1730000000) == wire);   // cached for the second
    CHECK(ok->getWire(1730000001) != wire);
    CHECK(!HttpFastResponse::create(299, {}, "", err));
    CHECK(!HttpFastResponse::create(200, {{"Content-Length", "3"}}, "", err));
    CHECK(!HttpFastResponse::create(200, {{"Connection", "close"}}, "", err));
    CHECK(!HttpFastResponse::create(200, {{"bad name", "x"}}, "", err));
    CHECK(!HttpFastResponse::create(200, {{"X", "a\r\nInjected: 1"}}, "", err));
    CHECK(!HttpFastResponse::create(200, {}, "body without a content type", err));
    CHECK(!HttpFastResponse::create(204, {}, "body", err));
    CHECK(HttpFastResponse::create(204, {}, "", err));
    CHECK(!HttpFastResponse::create(200, {{"Content-Type", "x"}}, std::string(HttpFastResponse::MaxSize, 'x'), err));

    // --- cells and rules
    auto cell = std::make_shared<HttpFastCell>(0, 0);
    auto up = resp(200, "ready");
    auto down = resp(503, "not ready");
    HttpFastRule r;
    r.cell = cell;
    r.op = HttpFastOp::Eq;
    r.operand = 1;
    r.response = up;
    auto spec = HttpFastRouteSpec::create({r}, down, err);
    CHECK(spec);
    HttpFastRoutes routes;
    CHECK(routes.add("/ready", spec, err));
    HttpFastFallback fb;
    auto w = routes.match(req(), 1730000000, fb);
    CHECK(w && w->find("503") != std::string::npos);
    cell->set(1);
    w = routes.match(req(), 1730000000, fb);
    CHECK(w && w->find("200 OK") != std::string::npos);
    cell->set(7);
    w = routes.match(req(), 1730000000, fb);
    CHECK(w && w->find("503") != std::string::npos);

    // all comparison operators
    for (int i = 0; i < 6; ++i) {
        auto c = std::make_shared<HttpFastCell>(5, 0);
        HttpFastRule rr;
        rr.cell = c;
        rr.op = static_cast<HttpFastOp>(i);
        rr.operand = 5;
        rr.response = up;
        auto sp = HttpFastRouteSpec::create({rr}, nullptr, err);
        HttpFastRoutes rt;
        rt.add("/x", sp, err);
        auto got = rt.match(req("/x"), 1, fb);
        bool expect[] = {true, false, false, true, false, true};   // 5 op 5: eq ne lt le gt ge
        CHECK((got != nullptr) == expect[i]);
        if (!got) {
            CHECK(fb == HttpFastFallback::NoRule);
        }
    }

    // a stale cell is never read: the request takes the normal path
    auto stale = std::make_shared<HttpFastCell>(1, 1000);   // 1 ms
    HttpFastRule sr;
    sr.cell = stale;
    sr.op = HttpFastOp::Eq;
    sr.operand = 1;
    sr.response = up;
    HttpFastRoutes rs;
    rs.add("/s", HttpFastRouteSpec::create({sr}, down, err), err);
    CHECK(rs.match(req("/s"), 1, fb));
    HttpFastCell::advanceClockForTest(5000);   // 5 ms
    CHECK(!rs.match(req("/s"), 1, fb));
    CHECK(fb == HttpFastFallback::Stale);
    stale->touch();
    CHECK(rs.match(req("/s"), 1, fb));
    CHECK(rs.getFallbacks(HttpFastFallback::Stale) == 1);

    // --- slots: a response that is published from outside and replaced as a whole
    auto slot = std::make_shared<HttpFastSlot>(0);
    CHECK(!slot->get());                 // never set: nothing to serve
    CHECK(slot->getAgeUs() == -1);
    HttpFastRoutes rsl;
    CHECK(rsl.add("/m", HttpFastRouteSpec::create({}, slot, err), err));
    CHECK(!rsl.match(req("/m"), 1, fb));
    CHECK(fb == HttpFastFallback::Stale);
    slot->set(resp(200, "one"));
    w = rsl.match(req("/m"), 1730000000, fb);
    CHECK(w && w->find("\r\n\r\none") != std::string::npos);
    auto held = slot->get();             // a reference taken for a send survives the replacement
    slot->set(resp(200, "two"));
    w = rsl.match(req("/m"), 1730000000, fb);
    CHECK(w && w->find("\r\n\r\ntwo") != std::string::npos);
    CHECK(held && held->getWire(1730000000)->find("one") != std::string::npos);
    CHECK(slot->getAgeUs() >= 0);

    // a slot with a maximum age is not served when stale, and is again when set
    auto aged = std::make_shared<HttpFastSlot>(1000);   // 1 ms
    aged->set(resp(200, "aged"));
    HttpFastRoutes ra;
    ra.add("/a", HttpFastRouteSpec::create({}, aged, err), err);
    CHECK(ra.match(req("/a"), 1, fb));
    HttpFastCell::advanceClockForTest(5000);   // 5 ms
    CHECK(!ra.match(req("/a"), 1, fb));
    CHECK(fb == HttpFastFallback::Stale);
    aged->set(resp(200, "fresh"));
    CHECK(ra.match(req("/a"), 1, fb));

    // a rule can answer from a slot
    auto rcell = std::make_shared<HttpFastCell>(1, 0);
    auto rslot = std::make_shared<HttpFastSlot>(0);
    HttpFastRule slr;
    slr.cell = rcell;
    slr.op = HttpFastOp::Eq;
    slr.operand = 1;
    slr.response = rslot;
    HttpFastRoutes rr;
    rr.add("/r", HttpFastRouteSpec::create({slr}, down, err), err);
    CHECK(!rr.match(req("/r"), 1, fb) && fb == HttpFastFallback::Stale);   // the rule holds, its slot is empty
    rslot->set(up);
    CHECK(rr.match(req("/r"), 1, fb));
    rcell->set(0);
    w = rr.match(req("/r"), 1, fb);                                         // the rule does not hold
    CHECK(w && w->find("503") != std::string::npos);

    // --- eligibility
    HttpFastRoutes re;
    re.add("/e", HttpFastRouteSpec::create({}, up, err), err);
    CHECK(re.match(req("/e"), 1, fb));
    CHECK(re.match(req("/e?x=1&y=2"), 1, fb));   // the query string is not part of the route
    CHECK(!re.match(req("/e/"), 1, fb));
    CHECK(!re.match(req("/E"), 1, fb));
    HttpFastRequestView v = req("/e");
    v.method = "POST";
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::NoRoute);
    v = req("/e"); v.http_version = "1.0";
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::NotHttp11);
    v = req("/e"); v.content_length = "5";
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Body);
    v = req("/e"); v.content_length = "0";
    CHECK(re.match(v, 1, fb));
    v = req("/e"); v.has_transfer_encoding = true;
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Body);
    v = req("/e"); v.has_expect = true;
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Expect);
    v = req("/e"); v.has_upgrade = true;
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Upgrade);
    v = req("/e"); v.connection = "close";
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Connection);   // the caller does not close the connection
    {
        // a caller that closes the connection after the response gets the response with Connection: close
        bool close_after = true;
        auto cw = re.match(req("/e"), 1730000000, fb, &close_after);
        CHECK(cw && !close_after);
        CHECK(cw && cw->find("Connection: Keep-Alive\r\n") != std::string::npos);
        for (const char* c : {"close", "Close", "CLOSE"}) {
            v = req("/e"); v.connection = c;
            cw = re.match(v, 1730000000, fb, &close_after);
            CHECK(cw && close_after);
            CHECK(cw && cw->find("Connection: close\r\n") != std::string::npos);
            CHECK(cw && cw->find("Connection: Keep-Alive") == std::string::npos);
            CHECK(cw && cw->find("Content-Length:") != std::string::npos);
        }
        v = req("/e"); v.connection = "te";
        CHECK(!re.match(v, 1, fb, &close_after) && fb == HttpFastFallback::Connection && !close_after);
        v = req("/e"); v.connection = "close"; v.pending = true;
        CHECK(!re.match(v, 1, fb, &close_after) && fb == HttpFastFallback::Pipelined);
        // the wire for a closing request is cached apart from the other
        auto k1 = ok->getWire(1730000005, false);
        auto k2 = ok->getWire(1730000005, true);
        CHECK(k1 != k2 && k1->find("Keep-Alive") != std::string::npos && k2->find("close") != std::string::npos);
        CHECK(ok->getWire(1730000005, true) == k2 && ok->getWire(1730000005, false) == k1);
    }
    v = req("/e"); v.connection = "Keep-Alive";
    CHECK(re.match(v, 1, fb));
    v = req("/e"); v.pending = true;
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Pipelined);
    CHECK(re.getFallbacks(HttpFastFallback::Body) == 2);

    // --- HTTP/2: the same rules, no version check, no Connection header, the response parts instead of a wire
    {
        HttpFastRoutes r2;
        auto h2resp = HttpFastResponse::create(200, {{"Content-Type", "text/plain"}, {"X-A", "1"}}, "hello", err);
        CHECK(h2resp);
        CHECK(r2.add("/h2", HttpFastRouteSpec::create({}, h2resp, err), err));
        HttpFastRequestView hv = req("/h2");
        hv.http_version = "2";
        auto got = r2.matchH2(hv, fb);
        CHECK(got && got->getCode() == 200 && got->getBody() == "hello");
        auto hh = got->getH2Headers(1730000000);
        CHECK(hh.size() == 4);
        CHECK(hh[0].first == "Content-Type" && hh[1].first == "X-A");
        CHECK(hh[2].first == "date" && hh[2].second == "Sun, 27 Oct 2024 03:33:20 GMT");
        CHECK(hh[3].first == "content-length" && hh[3].second == "5");
        // no connection-specific header is among them
        for (const auto& h : hh) {
            CHECK(strcasecmp(h.first.c_str(), "connection") && strcasecmp(h.first.c_str(), "transfer-encoding")
                && strcasecmp(h.first.c_str(), "keep-alive"));
        }
        // the query string is not part of the route; an exact path only; GET only
        hv = req("/h2?x=1"); hv.http_version = "2";
        CHECK(r2.matchH2(hv, fb));
        hv = req("/h2/"); hv.http_version = "2";
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::NoRoute);
        hv = req("/h2"); hv.http_version = "2"; hv.method = "POST";
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::NoRoute);
        // a body, Expect, Upgrade, a Connection header: the normal path
        hv = req("/h2"); hv.content_length = "5";
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::Body);
        hv = req("/h2"); hv.content_length = "0";
        CHECK(r2.matchH2(hv, fb));
        hv = req("/h2"); hv.has_transfer_encoding = true;
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::Body);
        hv = req("/h2"); hv.has_expect = true;
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::Expect);
        hv = req("/h2"); hv.has_upgrade = true;
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::Upgrade);
        hv = req("/h2"); hv.connection = "keep-alive";
        CHECK(!r2.matchH2(hv, fb) && fb == HttpFastFallback::Connection);
        // a response without a body has no content-length header
        auto empty = HttpFastResponse::create(204, {}, "", err);
        CHECK(empty && empty->getH2Headers(1730000000).size() == 1);
        // the hits are counted for HTTP/2 too
        CHECK(r2.getRouteHits()[0].second >= 3);
    }

    // --- extra headers of a listener (Alt-Svc)
    {
        std::string e2;
        CHECK(!HttpFastExtraHeaders::create({{"", "x"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Bad Name", "x"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Content-Length", "1"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"connection", "close"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Date", "x"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Upgrade", "x"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Alt-Svc", "a\r\nInjected: 1"}}, e2));
        CHECK(!HttpFastExtraHeaders::create({{"Alt-Svc", std::string("a\0b", 3)}}, e2));
        std::vector<std::pair<std::string, std::string>> many;
        for (size_t i = 0; i <= HttpFastExtraHeaders::MaxHeaders; ++i) {
            many.emplace_back("X-" + std::to_string(i), "v");
        }
        CHECK(!HttpFastExtraHeaders::create(many, e2));

        auto alt = HttpFastExtraHeaders::create({{"Alt-Svc", "h3=\":443\"; ma=3600"}}, e2);
        auto alt2 = HttpFastExtraHeaders::create({{"Alt-Svc", "h3=\":8443\"; ma=60"}}, e2);
        CHECK(alt && alt2);
        auto plain = resp(200, "x");
        // the headers are on the wire, after Date and Connection, and not on a wire without them
        auto w0 = plain->getWire(1730000000, false, nullptr);
        auto w1 = plain->getWire(1730000000, false, alt);
        auto w2 = plain->getWire(1730000000, false, alt2);
        CHECK(w0->find("Alt-Svc") == std::string::npos);
        CHECK(w1->find("Alt-Svc: h3=\":443\"; ma=3600\r\n") != std::string::npos);
        CHECK(w2->find("Alt-Svc: h3=\":8443\"; ma=60\r\n") != std::string::npos);
        CHECK(w1->find("Connection: Keep-Alive\r\nAlt-Svc") != std::string::npos);
        CHECK(w1->find("\r\n\r\nx") != std::string::npos);
        // each variant is cached on its own, by identity, and with the close variant
        CHECK(plain->getWire(1730000000, false, alt) == w1);
        CHECK(plain->getWire(1730000000, false, alt2) == w2);
        CHECK(plain->getWire(1730000000, false, nullptr) == w0);
        auto wc = plain->getWire(1730000000, true, alt);
        CHECK(wc != w1 && wc->find("Connection: close\r\nAlt-Svc") != std::string::npos);
        // a new second builds the variants again
        auto w1b = plain->getWire(1730000001, false, alt);
        CHECK(w1b != w1 && w1b->find("Date: Sun, 27 Oct 2024 03:33:21 GMT") != std::string::npos);
        // more variants than are cached: all are still correct
        std::vector<std::shared_ptr<const HttpFastExtraHeaders>> sets;
        for (int i = 0; i < 20; ++i) {
            sets.push_back(HttpFastExtraHeaders::create({{"X-Set", std::to_string(i)}}, e2));
        }
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 20; ++i) {
                auto ww = plain->getWire(1730000002, false, sets[i]);
                CHECK(ww->find("X-Set: " + std::to_string(i) + "\r\n") != std::string::npos);
            }
        }
        // a header that the route's response has is not replaced
        auto own = HttpFastResponse::create(200, {{"Content-Type", "text/plain"}, {"alt-svc", "clear"}}, "y", e2);
        auto wo = own->getWire(1730000000, false, alt);
        CHECK(wo->find("alt-svc: clear\r\n") != std::string::npos);
        CHECK(wo->find("Alt-Svc: h3") == std::string::npos);
        // HTTP/2 and HTTP/3: the pairs
        auto hx = plain->getH2Headers(1730000000, alt.get());
        bool found_alt = false;
        for (const auto& h : hx) {
            if (h.first == "Alt-Svc" && h.second == "h3=\":443\"; ma=3600") {
                found_alt = true;
            }
        }
        CHECK(found_alt);
        auto hy = own->getH2Headers(1730000000, alt.get());
        size_t alts = 0;
        for (const auto& h : hy) {
            if (!strcasecmp(h.first.c_str(), "alt-svc")) {
                ++alts;
            }
        }
        CHECK(alts == 1);
        CHECK(plain->getH2Headers(1730000000, nullptr).size() == plain->getH2Headers(1730000000).size());

        // match() passes the headers on
        HttpFastRoutes rx;
        rx.add("/x", HttpFastRouteSpec::create({}, plain, e2), e2);
        auto mw = rx.match(req("/x"), 1730000000, fb, nullptr, alt);
        CHECK(mw && mw->find("Alt-Svc: h3") != std::string::npos);
        mw = rx.match(req("/x"), 1730000000, fb, nullptr);
        CHECK(mw && mw->find("Alt-Svc") == std::string::npos);

        // the context shares routes and extra headers; a change is seen by whoever holds it
        auto ctx = std::make_shared<HttpFastContext>();
        auto ctx_copy = ctx;
        CHECK(!ctx->get().routes && !ctx->get().extra);
        auto rts = std::make_shared<HttpFastRoutes>();
        ctx->setRoutes(rts);
        ctx->setExtraHeaders(alt);
        CHECK(ctx_copy->get().routes == rts && ctx_copy->get().extra == alt);
        ctx->setExtraHeaders(nullptr);
        CHECK(!ctx_copy->get().extra && ctx_copy->get().routes == rts);

        // many threads read the variants of a response while the extra headers change
        std::atomic<bool> stop2{false};
        std::atomic<uint64_t> bad2{0};
        std::vector<std::thread> readers;
        for (int t = 0; t < 4; ++t) {
            readers.emplace_back([&] {
                while (!stop2.load()) {
                    auto snap = ctx->get();
                    auto ww = plain->getWire(1730000000, false, snap.extra);
                    bool has = ww->find("Alt-Svc") != std::string::npos;
                    if (has != (snap.extra != nullptr)) {
                        ++bad2;
                    }
                }
            });
        }
        for (int i = 0; i < 20000; ++i) {
            ctx->setExtraHeaders((i & 1) ? alt : (i & 2 ? alt2 : nullptr));
        }
        stop2 = true;
        for (auto& th : readers) {
            th.join();
        }
        CHECK(bad2 == 0);
    }

    // --- registration
    HttpFastRoutes rg;
    auto okspec = HttpFastRouteSpec::create({}, up, err);
    CHECK(!rg.add("noslash", okspec, err));
    CHECK(!rg.add("/q?x", okspec, err));
    CHECK(!rg.add("/a b", okspec, err));
    CHECK(!rg.add("/" + std::string(HttpFastRoutes::MaxPathSize, 'a'), okspec, err));
    CHECK(rg.add("/ok", okspec, err));
    CHECK(rg.size() == 1);
    CHECK(rg.match(req("/ok"), 1, fb));
    CHECK(rg.match(req("/ok"), 1, fb));
    CHECK(rg.getRouteHits()[0].second == 2);
    CHECK(rg.add("/ok", okspec, err));            // replaced: the hit count carries over
    CHECK(rg.getRouteHits()[0].second == 2);
    CHECK(rg.remove("/ok"));
    CHECK(!rg.remove("/ok"));
    CHECK(!rg.match(req("/ok"), 1, fb));
    CHECK(!HttpFastRouteSpec::create({}, nullptr, err));
    std::vector<HttpFastRule> many(HttpFastRouteSpec::MaxRules + 1, r);
    CHECK(!HttpFastRouteSpec::create(many, down, err));
    HttpFastRule bad = r; bad.cell = nullptr;
    CHECK(!HttpFastRouteSpec::create({bad}, down, err));

    // --- the table can be changed while another thread matches (no torn state)
    HttpFastRoutes rc;
    auto cc = std::make_shared<HttpFastCell>(0, 0);
    HttpFastRule cr; cr.cell = cc; cr.op = HttpFastOp::Eq; cr.operand = 1; cr.response = up;
    auto cspec = HttpFastRouteSpec::create({cr}, down, err);
    rc.add("/c", cspec, err);
    auto cslot = std::make_shared<HttpFastSlot>(0);
    cslot->set(up);
    rc.add("/slot", HttpFastRouteSpec::create({}, cslot, err), err);
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> served{0}, bad_answers{0};
    std::thread reader([&] {
        while (!stop.load()) {
            HttpFastFallback f;
            auto sgot = rc.match(req("/slot"), 1, f);
            if (!sgot || (sgot->find("ready") == std::string::npos && sgot->find("not ready") == std::string::npos)) {
                ++bad_answers;
            }
            auto got = rc.match(req("/c"), 1, f);
            if (got) {
                ++served;
                if (got->find("200 OK") == std::string::npos && got->find("503") == std::string::npos) {
                    ++bad_answers;
                }
            }
        }
    });
    for (int i = 0; i < 20000; ++i) {
        cc->set(i & 1);
        cslot->set(i & 1 ? up : down);
        if (i % 100 == 0) {
            rc.add("/c", cspec, err);
            rc.add("/other" + std::to_string(i % 7), cspec, err);
        }
    }
    stop = true;
    reader.join();
    CHECK(served > 0);
    CHECK(bad_answers == 0);


    // --- the strict request parser
    {
        // the views of a parsed request refer to the input, which is kept until the next parse
        std::string keep;
        auto parse = [&keep](const std::string& h, HttpFastRequestView& v) {
            keep = h;
            return HttpFastRequestParser::parse(keep.data(), keep.size(), v);
        };
        const std::string base = "GET /ready HTTP/1.1\r\nHost: x\r\n\r\n";
        HttpFastRequestView pv;
        CHECK(parse(base, pv));
        CHECK(pv.method == "GET" && pv.path == "/ready" && pv.http_version == "1.1");
        CHECK(!pv.content_length && !pv.connection && !pv.has_transfer_encoding && !pv.has_expect && !pv.has_upgrade);
        // the views refer to the input
        CHECK(pv.path.data() >= keep.data() && pv.path.data() < keep.data() + keep.size());

        CHECK(parse("GET /a/b?x=1&y=2;z=3 HTTP/1.1\r\nhOsT:   x \t\r\nConnection: keep-alive\r\n"
            "Content-Length: 0\r\nAccept: */*\r\nUser-Agent: a b: c\r\nX-Empty:\r\n\r\n", pv));
        CHECK(pv.path == "/a/b?x=1&y=2;z=3");
        CHECK(pv.connection && *pv.connection == "keep-alive");
        CHECK(pv.content_length && *pv.content_length == "0");
        CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nExpect: 100-continue\r\n"
            "Upgrade: h2c\r\n\r\n", pv));
        CHECK(pv.has_transfer_encoding && pv.has_expect && pv.has_upgrade);
        CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\nHTTP2-Settings: AAMAAABk\r\n\r\n", pv) && pv.has_upgrade);

        // everything else is rejected
        const char* rejected[] = {
            "",
            "GET /ready HTTP/1.1\r\n\r\n",                                   // no Host
            "GET /ready HTTP/1.0\r\nHost: x\r\n\r\n",                         // not 1.1
            "POST /ready HTTP/1.1\r\nHost: x\r\n\r\n",
            "get /ready HTTP/1.1\r\nHost: x\r\n\r\n",
            "GET  /ready HTTP/1.1\r\nHost: x\r\n\r\n",                        // two spaces
            "GET ready HTTP/1.1\r\nHost: x\r\n\r\n",                         // no leading slash
            "GET http://x/ready HTTP/1.1\r\nHost: x\r\n\r\n",
            "GET /re%61dy HTTP/1.1\r\nHost: x\r\n\r\n",                      // percent-encoding
            "GET /re#ady HTTP/1.1\r\nHost: x\r\n\r\n",
            "GET /re ady HTTP/1.1\r\nHost: x\r\n\r\n",
            "GET /ready HTTP/1.1\nHost: x\n\n",                                // bare LF
            "GET /ready HTTP/1.1\r\nHost: x\r\n\n",
            "GET /ready HTTP/1.1\r\nHost: x\rX: y\r\n\r\n",                    // bare CR
            "GET /ready HTTP/1.1\r\nHost : x\r\n\r\n",                        // space before the colon
            "GET /ready HTTP/1.1\r\n Host: x\r\n\r\n",                        // leading whitespace / fold
            "GET /ready HTTP/1.1\r\nHost: x\r\n y\r\n\r\n",                   // obsolete fold
            "GET /ready HTTP/1.1\r\nHost: x\r\nX: a\tb\x01\r\n\r\n",           // control character
            "GET /ready HTTP/1.1\r\nHost: x\r\nX: \xc3\xa9\r\n\r\n",           // non-ASCII
            "GET /ready HTTP/1.1\r\nHost: x\r\nBad Name: y\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\n: y\r\n\r\n",                  // empty name
            "GET /ready HTTP/1.1\r\nHost: x\r\nNoColon\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\nHost: y\r\n\r\n",              // duplicates
            "GET /ready HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\nConnection: a\r\nconnection: b\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\nContent-Length: +1\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\nContent-Length: 1 2\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\nContent-Length:\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost:\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x\r\n\r\nGET / HTTP/1.1\r\n\r\n",   // bytes beyond the header
            "GET /ready HTTP/1.1\r\nHost: x\r\n\r\n\r\n",
            "GET /ready HTTP/1.1\r\nHost: x",                                // no end
            "GET /ready HTTP/1.1\r\nHost: x\r\n",
        };
        for (const char* r : rejected) {
            std::string h(r);
            HttpFastRequestView rv;
            if (parse(h, rv)) {
                printf("FAIL: accepted: %s\n", r);
                ++failures;
            }
        }
        // the NUL character, which a C string literal cannot hold
        std::string nul = "GET /ready HTTP/1.1\r\nHost: x\r\nX: a_b\r\n\r\n";
        nul[nul.find('_')] = '\0';
        CHECK(!parse(nul, pv));
        // limits
        std::string longpath = "GET /" + std::string(HttpFastRequestParser::MaxTargetSize, 'a') + " HTTP/1.1\r\nHost: x\r\n\r\n";
        CHECK(!parse(longpath, pv));
        std::string many = "GET / HTTP/1.1\r\nHost: x\r\n";
        for (size_t i = 0; i < HttpFastRequestParser::MaxHeaders; ++i) {
            many += "X-" + std::to_string(i) + ": v\r\n";
        }
        CHECK(!parse(many + "\r\n", pv));
        std::string big = "GET / HTTP/1.1\r\nHost: x\r\nX: " + std::string(HttpFastRequestParser::MaxHeaderSize, 'v') + "\r\n\r\n";
        CHECK(!parse(big, pv));

        // a parsed request goes through the matcher like one built from a hash
        HttpFastRoutes rp;
        rp.add("/ready", HttpFastRouteSpec::create({}, up, err), err);
        CHECK(parse("GET /ready?x=1 HTTP/1.1\r\nHost: x\r\n\r\n", pv) && rp.match(pv, 1, fb));
        CHECK(parse("GET /ready HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\n", pv));
        CHECK(!rp.match(pv, 1, fb) && fb == HttpFastFallback::Body);

        // random mutations of an accepted header never crash (this runs under the sanitizers) and the parser never
        // reads past the input: it is given a buffer of exactly the size
        std::mt19937 rng(20260101);
        const std::string seed_hdr = "GET /a/b?c=d HTTP/1.1\r\nHost: example.org\r\nConnection: keep-alive\r\n"
            "Content-Length: 0\r\nAccept: */*\r\n\r\n";
        size_t accepted = 0;
        for (int i = 0; i < 200000; ++i) {
            std::string m = seed_hdr;
            int edits = 1 + rng() % 4;
            for (int e = 0; e < edits; ++e) {
                size_t pos = rng() % m.size();
                switch (rng() % 4) {
                    case 0: m[pos] = static_cast<char>(rng() % 256); break;
                    case 1: m.erase(pos, 1 + rng() % 3); break;
                    case 2: m.insert(pos, 1, static_cast<char>(rng() % 256)); break;
                    default: m.insert(pos, "\r\n"); break;
                }
                if (m.empty()) {
                    break;
                }
            }
            std::unique_ptr<char[]> exact(new char[m.size()]);
            memcpy(exact.get(), m.data(), m.size());
            HttpFastRequestView fv;
            if (HttpFastRequestParser::parse(exact.get(), m.size(), fv)) {
                ++accepted;
                // an accepted request is whole: it is GET, 1.1, and its views are inside the input
                CHECK(fv.method == "GET" && fv.http_version == "1.1");
                CHECK(fv.path.data() >= exact.get() && fv.path.data() + fv.path.size() <= exact.get() + m.size());
                CHECK(m.size() >= 4 && m.compare(m.size() - 4, 4, "\r\n\r\n") == 0);
            }
        }
        CHECK(accepted > 0);
    }

    printf(failures ? "FAILED: %d\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
