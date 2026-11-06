/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  HttpFastRoutes.cpp

  Qore Programming Language

  Copyright (C) 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

// ISOLATION RULE: see HttpFastRoutes.h; this file must not use the Qore interpreter
#include "qore/intern/HttpFastRoutes.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

struct HttpFastReason {
    int code;
    const char* text;
};

// the status codes that a fast route can answer with
const HttpFastReason HttpFastReasons[] = {
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {500, "Internal Server Error"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
};

const char* const HttpFastFallbackNames[] = {
    "no_route", "not_http_1_1", "body", "expect", "upgrade", "connection", "pipelined", "stale", "no_rule",
};

static_assert(sizeof(HttpFastFallbackNames) / sizeof(HttpFastFallbackNames[0])
    == static_cast<size_t>(HttpFastFallback::Count), "a name is missing for a fallback reason");

const char* const WeekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

//! an HTTP token character (RFC 9110 section 5.6.2)
bool is_token_char(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c && strchr("!#$%&'*+-.^_`|~", c);
}

bool iequals(std::string_view a, const char* b) {
    size_t len = strlen(b);
    if (a.size() != len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

//! formats a date as an HTTP date (IMF-fixdate, RFC 9110 section 5.6.7); the day and month names do not depend on the
//! locale
std::string format_http_date(int64_t epoch_s) {
    time_t t = static_cast<time_t>(epoch_s);
    struct tm tm;
    if (!gmtime_r(&t, &tm)) {
        return std::string();
    }
    char buf[40];
    snprintf(buf, sizeof buf, "%s, %02d %s %04d %02d:%02d:%02d GMT", WeekDays[tm.tm_wday], tm.tm_mday,
        Months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

} // anonymous namespace

std::shared_ptr<const HttpFastExtraHeaders> HttpFastExtraHeaders::create(
        const std::vector<std::pair<std::string, std::string>>& headers, std::string& err) {
    if (headers.size() > MaxHeaders) {
        err = "a listener has at most " + std::to_string(MaxHeaders) + " extra headers";
        return nullptr;
    }
    for (const auto& [name, value] : headers) {
        if (name.empty()) {
            err = "a header name is empty";
            return nullptr;
        }
        for (unsigned char ch : name) {
            if (!is_token_char(ch)) {
                err = "the header name '" + name + "' is not valid";
                return nullptr;
            }
        }
        if (iequals(name, "content-length") || iequals(name, "transfer-encoding") || iequals(name, "connection")
                || iequals(name, "date") || iequals(name, "upgrade") || iequals(name, "keep-alive")) {
            err = "the header '" + name + "' is set by the server and cannot be given";
            return nullptr;
        }
        for (unsigned char ch : value) {
            if (ch == '\r' || ch == '\n' || ch == '\0') {
                err = "the value of the header '" + name + "' has a control character";
                return nullptr;
            }
        }
    }
    // the constructor is private
    std::shared_ptr<HttpFastExtraHeaders> rv(new HttpFastExtraHeaders);
    rv->headers = headers;
    return rv;
}

// identifies a response in the per-thread caches of serialized responses (see getWire()); never reused
static std::atomic<uint64_t> next_response_id{1};

std::shared_ptr<const HttpFastResponse> HttpFastResponse::create(int code,
        const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body, std::string& err) {
    const char* reason = nullptr;
    for (const auto& r : HttpFastReasons) {
        if (r.code == code) {
            reason = r.text;
            break;
        }
    }
    if (!reason) {
        err = "the status code " + std::to_string(code) + " is not supported by a fast route";
        return nullptr;
    }

    std::string prefix = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n";
    bool has_content_type = false;
    for (const auto& [name, value] : headers) {
        if (name.empty()) {
            err = "a header name is empty";
            return nullptr;
        }
        for (unsigned char c : name) {
            if (!is_token_char(c)) {
                err = "the header name '" + name + "' is not valid";
                return nullptr;
            }
        }
        // the connection and the framing of the response are set here
        if (iequals(name, "content-length") || iequals(name, "transfer-encoding") || iequals(name, "connection")
                || iequals(name, "date") || iequals(name, "upgrade") || iequals(name, "keep-alive")) {
            err = "the header '" + name + "' is set by the server and cannot be given";
            return nullptr;
        }
        for (unsigned char c : value) {
            if (c == '\r' || c == '\n' || c == '\0') {
                err = "the value of the header '" + name + "' has a control character";
                return nullptr;
            }
        }
        if (iequals(name, "content-type")) {
            has_content_type = true;
        }
        prefix += name;
        prefix += ": ";
        prefix += value;
        prefix += "\r\n";
    }
    if (!body.empty() && !has_content_type) {
        err = "a response with a body needs a Content-Type header";
        return nullptr;
    }
    if (code == 204 && !body.empty()) {
        err = "a 204 response cannot have a body";
        return nullptr;
    }
    // the response of a handler to a request that is not a HEAD request has a Content-Length header, also with the
    // status 204 (RFC 9110 section 8.6 says that a server must not send one with 204; the handler path does, and a
    // fast route answers exactly as it does so that the two cannot be told apart)
    std::string suffix = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

    // "Date: " + a 29-character date + the line end
    static constexpr size_t DateLineSize = 6 + 29 + 2;
    static constexpr size_t KeepAliveLineSize = sizeof("Connection: Keep-Alive\r\n") - 1;
    size_t head_size = prefix.size() + DateLineSize + KeepAliveLineSize + suffix.size();
    if (head_size + body.size() > MaxSize) {
        err = "the response is larger than the " + std::to_string(MaxSize) + " bytes that a fast route serves";
        return nullptr;
    }

    // the constructor is private
    std::shared_ptr<HttpFastResponse> rv(new HttpFastResponse);
    rv->code = code;
    rv->headers = headers;
    rv->head_prefix = std::move(prefix);
    rv->head_suffix = std::move(suffix);
    rv->body = body;
    rv->head_size = head_size;
    rv->id = next_response_id.fetch_add(1, std::memory_order_relaxed);
    rv->created_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return rv;
}

int64_t HttpFastCell::nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() + clock_offset_us.load(std::memory_order_relaxed);
}

void HttpFastSlot::set(std::shared_ptr<const HttpFastResponse> new_response) {
    auto next = std::make_shared<Value>();
    next->response = std::move(new_response);
    next->set_us = HttpFastCell::monotonicUs();
    // the previous response is destroyed by this thread, not by a reader
    value.set(std::move(next));
}

std::shared_ptr<const HttpFastResponse> HttpFastSlot::get() const {
    std::shared_ptr<const Value> v = value.get();
    if (v->response && max_age_us && HttpFastCell::monotonicUs() - v->set_us > max_age_us) {
        return nullptr;
    }
    return v->response;
}

int64_t HttpFastSlot::getAgeUs() const {
    std::shared_ptr<const Value> v = value.getLocked();
    if (!v->response) {
        return -1;
    }
    return HttpFastCell::monotonicUs() - v->set_us;
}

std::shared_ptr<const HttpFastRouteSpec> HttpFastRouteSpec::create(std::vector<HttpFastRule> rules,
        HttpFastResponseSource otherwise, std::string& err) {
    if (rules.size() > MaxRules) {
        err = "a route has at most " + std::to_string(MaxRules) + " rules";
        return nullptr;
    }
    if (rules.empty() && !otherwise) {
        err = "a route needs a response, or rules";
        return nullptr;
    }
    for (const HttpFastRule& r : rules) {
        if (!r.cell) {
            err = "a rule has no cell";
            return nullptr;
        }
        if (!r.response) {
            err = "a rule has no response";
            return nullptr;
        }
    }
    // the constructor is private
    std::shared_ptr<HttpFastRouteSpec> rv(new HttpFastRouteSpec);
    rv->rules = std::move(rules);
    rv->otherwise = std::move(otherwise);
    return rv;
}

std::shared_ptr<const HttpFastResponse> HttpFastRouteSpec::choose(HttpFastFallback& why) const {
    // a cell is read once per request, so that rules over the same cell see the same value even if it is set
    // meanwhile (with "ready == 1" and "ready == 0" as the rules, a change between two reads would hold for neither);
    // different cells are read one after the other and are not atomic together
    const HttpFastCell* seen_cell[MaxRules];
    int64_t seen_value[MaxRules];
    size_t seen = 0;
    for (const HttpFastRule& r : rules) {
        int64_t v = 0;
        bool found = false;
        for (size_t i = 0; i < seen; ++i) {
            if (seen_cell[i] == r.cell.get()) {
                v = seen_value[i];
                found = true;
                break;
            }
        }
        if (!found) {
            if (!r.cell->load(v)) {
                why = HttpFastFallback::Stale;
                return nullptr;
            }
            seen_cell[seen] = r.cell.get();
            seen_value[seen] = v;
            ++seen;
        }
        bool holds;
        switch (r.op) {
            case HttpFastOp::Eq: holds = v == r.operand; break;
            case HttpFastOp::Ne: holds = v != r.operand; break;
            case HttpFastOp::Lt: holds = v < r.operand; break;
            case HttpFastOp::Le: holds = v <= r.operand; break;
            case HttpFastOp::Gt: holds = v > r.operand; break;
            case HttpFastOp::Ge: holds = v >= r.operand; break;
            default: holds = false; break;
        }
        if (holds) {
            std::shared_ptr<const HttpFastResponse> rv = r.response.get();
            if (!rv) {
                why = HttpFastFallback::Stale;
            }
            return rv;
        }
    }
    if (otherwise) {
        std::shared_ptr<const HttpFastResponse> rv = otherwise.get();
        if (!rv) {
            why = HttpFastFallback::Stale;
        }
        return rv;
    }
    why = HttpFastFallback::NoRule;
    return nullptr;
}

std::vector<std::pair<std::string, std::string>> HttpFastResponse::getH2Headers(int64_t epoch_s,
        const HttpFastExtraHeaders* extra) const {
    std::vector<std::pair<std::string, std::string>> rv;
    rv.reserve(headers.size() + 2 + (extra ? extra->getHeaders().size() : 0));
    rv = headers;
    if (extra) {
        // a header that the route has replaces the listener's
        for (const auto& h : extra->getHeaders()) {
            bool found = false;
            for (const auto& r : headers) {
                if (iequals(r.first, h.first.c_str())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                rv.push_back(h);
            }
        }
    }
    rv.emplace_back("date", format_http_date(epoch_s));
    // as the handler path sends them over HTTP/2: a Content-Length when there is a body
    if (!body.empty()) {
        rv.emplace_back("content-length", std::to_string(body.size()));
    }
    return rv;
}

namespace {
// The serialized responses that a thread has built.  Each thread that serves fast routes keeps its own, so a request
// takes no lock and never waits for another thread that builds the serialized response for a new second: the threads
// build their own copy, once per second, a variant and a response.  An entry is found by the id of the response (never
// reused), the variant and the second; the entries of responses that are gone stay until they are replaced, so a
// thread holds at most MaxWireCacheEntries serialized responses.
constexpr size_t MaxWireCacheEntries = 16;

struct WireCacheEntry {
    uint64_t response_id = 0;
    bool close = false;
    bool head = false;
    int64_t second = -1;
    std::shared_ptr<const HttpFastExtraHeaders> extra;
    std::shared_ptr<const std::string> wire;
};

struct WireCache {
    std::array<WireCacheEntry, MaxWireCacheEntries> entries;
    size_t next = 0;
};
}

std::shared_ptr<const std::string> HttpFastResponse::getWire(int64_t epoch_s, bool close,
        const std::shared_ptr<const HttpFastExtraHeaders>& extra, bool head) const {
    static thread_local WireCache cache;
    // the variant is found by identity of the extra headers (a listener has one set of them)
    WireCacheEntry* entry = nullptr;
    for (WireCacheEntry& e : cache.entries) {
        if (e.response_id == id && e.close == close && e.head == head && e.extra == extra) {
            if (e.second == epoch_s) {
                return e.wire;
            }
            entry = &e;
            break;
        }
    }

    std::shared_ptr<const std::string> wire = buildWire(epoch_s, close, extra, head);
    if (!entry) {
        // more variants than are cached: the oldest entry is replaced
        entry = &cache.entries[cache.next++ % MaxWireCacheEntries];
        entry->response_id = id;
        entry->close = close;
        entry->head = head;
        entry->extra = extra;
    }
    entry->second = epoch_s;
    entry->wire = wire;
    return wire;
}

std::shared_ptr<const std::string> HttpFastResponse::buildWire(int64_t epoch_s, bool close,
        const std::shared_ptr<const HttpFastExtraHeaders>& extra, bool head) const {
    auto wire = std::make_shared<std::string>();
    wire->reserve(head_size + body.size() + (extra ? 64 : 0));
    *wire += head_prefix;
    *wire += "Date: ";
    *wire += format_http_date(epoch_s);
    *wire += "\r\n";
    // as the handler path sends them: "Keep-Alive" for a request that keeps the connection, "close" for one that
    // closes it
    *wire += close ? "Connection: close\r\n" : "Connection: Keep-Alive\r\n";
    if (extra) {
        // the headers of the listener, except those that the route has itself
        for (const auto& h : extra->getHeaders()) {
            bool found = false;
            for (const auto& r : headers) {
                if (iequals(r.first, h.first.c_str())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                *wire += h.first;
                *wire += ": ";
                *wire += h.second;
                *wire += "\r\n";
            }
        }
    }
    if (head) {
        // the response to a HEAD request has the headers of the GET response, with its Content-Length, and no body;
        // a 204 response has no Content-Length (RFC 9110 section 8.6), as when the handler path answers HEAD
        *wire += code == 204 ? "\r\n" : head_suffix;
    } else {
        *wire += head_suffix;
        *wire += body;
    }

    return wire;
}

std::shared_ptr<const HttpFastRoutes::table_t> HttpFastRoutes::snapshot() const {
    return table.get();
}

std::shared_ptr<const HttpFastRoutes::table_t> HttpFastRoutes::snapshotLocked() const {
    return table.getLocked();
}

bool HttpFastRoutes::add(const std::string& path, std::shared_ptr<const HttpFastRouteSpec> spec,
        std::string& err) {
    if (path.empty() || path[0] != '/') {
        err = "the path must start with '/'";
        return false;
    }
    if (path.size() > MaxPathSize) {
        err = "the path is too long";
        return false;
    }
    for (unsigned char c : path) {
        // the request's query string and fragment are not part of a route, and a space or control character cannot be
        // in a request target
        if (c <= ' ' || c == 0x7f || c == '?' || c == '#') {
            err = "the path has a character that cannot be in a route";
            return false;
        }
    }
    if (!spec) {
        err = "no route specification";
        return false;
    }

    // the new table is built from the current one while only other changes wait; the I/O threads read the table that
    // they have, and take the new one at their next request
    table.update([&path, &spec](const std::shared_ptr<const table_t>& cur) {
        auto next = std::make_shared<table_t>(*cur);
        auto route = std::make_shared<HttpFastRoute>(path, spec);
        auto it = next->find(path);
        if (it != next->end()) {
            // the hit count carries over to the new response
            route->hits.store(it->second->hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
            it->second = std::move(route);
        } else {
            next->emplace(path, std::move(route));
        }
        return std::shared_ptr<const table_t>(std::move(next));
    });
    return true;
}

bool HttpFastRoutes::remove(const std::string& path) {
    bool removed = false;
    table.update([&path, &removed](const std::shared_ptr<const table_t>& cur) {
        if (!cur->count(path)) {
            return cur;
        }
        auto next = std::make_shared<table_t>(*cur);
        next->erase(path);
        removed = true;
        return std::shared_ptr<const table_t>(std::move(next));
    });
    return removed;
}

size_t HttpFastRoutes::size() const {
    return snapshotLocked()->size();
}

std::shared_ptr<const HttpFastResponse> HttpFastRoutes::chooseRoute(const HttpFastRequestView& req,
        HttpFastFallback& fallback, bool* close_after, bool h2) {
    if (close_after) {
        *close_after = false;
    }
    std::shared_ptr<const table_t> snap = snapshot();
    if (snap->empty()) {
        // nothing is registered: the normal path, without counting it
        fallback = HttpFastFallback::NoRoute;
        return nullptr;
    }

    auto fail = [this, &fallback](HttpFastFallback reason) -> std::shared_ptr<const HttpFastResponse> {
        fallback = reason;
        fallbacks[static_cast<size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    };

    // a HEAD request is answered with the headers of the response to a GET request
    if (req.method != "GET" && req.method != "HEAD") {
        return fail(HttpFastFallback::NoRoute);
    }
    // the query string is not part of the route
    std::string_view path = req.path;
    size_t q = path.find('?');
    if (q != std::string_view::npos) {
        path = path.substr(0, q);
    }
    // the table is keyed by std::string; a path is at most a few hundred bytes, and one that is longer than any route
    // cannot match
    if (path.size() > MaxPathSize) {
        return fail(HttpFastFallback::NoRoute);
    }
    auto it = snap->find(std::string(path));
    if (it == snap->end()) {
        return fail(HttpFastFallback::NoRoute);
    }

    if (!h2 && req.http_version != "1.1") {
        return fail(HttpFastFallback::NotHttp11);
    }
    // anything that changes the framing or the connection takes the normal path
    if ((req.content_length && *req.content_length != "0") || req.has_transfer_encoding) {
        return fail(HttpFastFallback::Body);
    }
    if (req.has_expect) {
        return fail(HttpFastFallback::Expect);
    }
    if (req.has_upgrade) {
        return fail(HttpFastFallback::Upgrade);
    }
    bool close = false;
    if (req.connection) {
        // HTTP/2 has no Connection header
        if (h2) {
            return fail(HttpFastFallback::Connection);
        }
        if (!iequals(*req.connection, "keep-alive")) {
            // a request that asks for the connection to be closed is answered if the caller closes it afterwards
            if (!close_after || !iequals(*req.connection, "close")) {
                return fail(HttpFastFallback::Connection);
            }
            close = true;
        }
    }
    if (req.pending) {
        return fail(HttpFastFallback::Pipelined);
    }

    const HttpFastRoute& route = *it->second;
    HttpFastFallback why = HttpFastFallback::NoRule;
    std::shared_ptr<const HttpFastResponse> response = route.spec->choose(why);
    if (!response) {
        return fail(why);
    }
    route.hits.fetch_add(1, std::memory_order_relaxed);
    if (close_after) {
        *close_after = close;
    }
    return response;
}

std::shared_ptr<const std::string> HttpFastRoutes::match(const HttpFastRequestView& req, int64_t epoch_s,
        HttpFastFallback& fallback, bool* close_after,
        const std::shared_ptr<const HttpFastExtraHeaders>& extra) {
    std::shared_ptr<const HttpFastResponse> response = chooseRoute(req, fallback, close_after, false);
    if (!response) {
        return nullptr;
    }
    return response->getWire(epoch_s, close_after && *close_after, extra, req.method == "HEAD");
}

std::shared_ptr<const HttpFastResponse> HttpFastRoutes::matchH2(const HttpFastRequestView& req,
        HttpFastFallback& fallback) {
    return chooseRoute(req, fallback, nullptr, true);
}

std::vector<std::pair<std::string, uint64_t>> HttpFastRoutes::getRouteHits() const {
    std::vector<std::pair<std::string, uint64_t>> rv;
    std::shared_ptr<const table_t> snap = snapshotLocked();
    rv.reserve(snap->size());
    for (const auto& [path, route] : *snap) {
        rv.emplace_back(path, route->hits.load(std::memory_order_relaxed));
    }
    std::sort(rv.begin(), rv.end());
    return rv;
}

const char* HttpFastRoutes::getFallbackName(HttpFastFallback reason) {
    assert(static_cast<size_t>(reason) < static_cast<size_t>(HttpFastFallback::Count));
    return HttpFastFallbackNames[static_cast<size_t>(reason)];
}
