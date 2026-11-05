// standalone check of the isolated fast-route core (lib/HttpFastRoutes.cpp), not part of the repository
#include "qore/intern/HttpFastRoutes.h"

#include <cassert>
#include <cstdio>
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
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Connection);
    v = req("/e"); v.connection = "Keep-Alive";
    CHECK(re.match(v, 1, fb));
    v = req("/e"); v.pending = true;
    CHECK(!re.match(v, 1, fb) && fb == HttpFastFallback::Pipelined);
    CHECK(re.getFallbacks(HttpFastFallback::Body) == 2);

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

    printf(failures ? "FAILED: %d\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
