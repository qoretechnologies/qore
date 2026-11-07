/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  HttpFastRoutes.h

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

#ifndef _QORE_INTERN_HTTPFASTROUTES_H

#define _QORE_INTERN_HTTPFASTROUTES_H

// ISOLATION RULE: this header and lib/HttpFastRoutes.cpp are executed on the async I/O threads and must not use the
// Qore interpreter: no QoreValue, QoreHashNode, ExceptionSink, QoreProgram, QoreObject or any other Qore type, and no
// call that can run Qore code, wait for another thread, or block.  They include only the standard library and the
// visibility macros of qore/common.h.  The glue that turns a Qore request header into an HttpFastRequestView and the
// Qore classes that register routes are in other files.  The test examples/test/qore/misc/HttpFastRoutesIsolation.qtest
// fails the build if this rule is broken (see design/http-server-fast-routes.md, "Enforcement").
#include <qore/common.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <qore/intern/HttpFastPublished.h>

//! The reasons a request is not answered by a fast route (the request then takes the normal path)
/** See design/http-server-fast-routes.md.
*/
enum class HttpFastFallback : unsigned {
    NoRoute,         //!< no route has the request's path (or the method is not GET or HEAD)
    NotHttp11,       //!< not an HTTP/1.1 request
    Body,            //!< the request has a body (Content-Length other than 0, or Transfer-Encoding)
    Expect,          //!< the request has an Expect header
    Upgrade,         //!< the request has an Upgrade or HTTP2-Settings header
    Connection,      //!< the request has a Connection header other than keep-alive
    Pipelined,       //!< bytes of a following request are already buffered
    Stale,           //!< a cell that the route's rules read is not current
    NoRule,          //!< no rule of the route holds and the route has no otherwise response
    Count
};

//! Response headers that a listener adds to the responses of its routes (such as \c Alt-Svc)
/** The headers are validated when the object is created, off the I/O thread, and the object is immutable, so a response
    can cache its serialized form for a set of extra headers by the object's identity.  A header that a route's
    response has itself is not added (the route's replaces the listener's, as with the server's configured headers).
*/
class HttpFastExtraHeaders {
public:
    //! The most extra headers a listener has
    static constexpr size_t MaxHeaders = 8;

    //! Creates the headers
    /** @param headers the headers; the framing and connection headers (\c Content-Length, \c Transfer-Encoding,
        \c Connection, \c Date, \c Upgrade, \c Keep-Alive) cannot be given
        @param err set to the reason if the headers are not valid

        @return the headers, or nullptr if they are not valid
    */
    DLLLOCAL static std::shared_ptr<const HttpFastExtraHeaders> create(
            const std::vector<std::pair<std::string, std::string>>& headers, std::string& err);

    DLLLOCAL const std::vector<std::pair<std::string, std::string>>& getHeaders() const {
        return headers;
    }

private:
    DLLLOCAL HttpFastExtraHeaders() = default;

    std::vector<std::pair<std::string, std::string>> headers;
};

//! An immutable, serialized response of a fast route
/** The response is serialized when it is created, off the I/O thread.  A request is answered with a shared reference to
    the serialized bytes; the only per-request work is choosing the cached copy for the current second, because the
    response carries a \c Date header.
*/
class HttpFastResponse {
public:
    //! The largest response (headers and body) that a fast route serves
    static constexpr size_t MaxSize = 64 * 1024;

    //! Creates a response
    /** @param code the status code; one of the codes with a reason phrase known here
        @param headers the response headers; the framing and connection headers (\c Content-Length,
        \c Transfer-Encoding, \c Connection, \c Date) are set here and must not be given
        @param body the response body
        @param err set to the reason if the response is not valid

        @return the response, or nullptr if it is not valid
    */
    DLLLOCAL static std::shared_ptr<const HttpFastResponse> create(int code,
            const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body,
            std::string& err);

    //! Returns the serialized response with the \c Date of the given second
    /** The copy is cached for each thread that serves the response (a thread never waits for another one, and a
        thread builds a copy once per second and variant); it is built again when the second changes.

        @param epoch_s the time, in seconds since the epoch
        @param close if true, the response has \c Connection: close, for a request that asked for the connection to be
        closed, instead of \c Connection: Keep-Alive
        @param extra the headers that the listener adds, if any; the serialized response is cached for each set of
        extra headers (by identity) that is used with it
        @param head if true, the response to a \c HEAD request: the headers of the response (with its
        \c Content-Length, except for a 204 response) and no body (RFC 9110 section 9.3.2)
    */
    DLLLOCAL std::shared_ptr<const std::string> getWire(int64_t epoch_s, bool close = false,
            const std::shared_ptr<const HttpFastExtraHeaders>& extra = nullptr, bool head = false) const;

    //! Returns the status code
    DLLLOCAL int getCode() const {
        return code;
    }

    //! Returns the body
    DLLLOCAL const std::string& getBody() const {
        return body;
    }

    //! Returns the headers of the response for HTTP/2: the given headers, \c date and, if there is a body,
    //! \c content-length
    /** The names are as given (the HTTP/2 session lower cases them); there are no connection-specific headers.

        @param epoch_s the time for the \c date header, in seconds since the epoch
    */
    DLLLOCAL std::vector<std::pair<std::string, std::string>> getH2Headers(int64_t epoch_s,
            const HttpFastExtraHeaders* extra = nullptr) const;

    //! Returns the time that the response was created, in microseconds since the epoch
    DLLLOCAL int64_t getCreatedUs() const {
        return created_us;
    }

    //! Returns the size of the serialized response
    DLLLOCAL size_t size() const {
        return head_size + body.size();
    }

private:
    DLLLOCAL HttpFastResponse() = default;

    //! status line, the given headers, up to (and excluding) the \c Date header
    std::string head_prefix;
    //! the \c Content-Length header and the empty line; the \c Connection header, which depends on the request, goes
    //! between the \c Date header and this in getWire()
    std::string head_suffix;
    std::string body;
    int code = 0;
    //! the headers as given when the response was created
    std::vector<std::pair<std::string, std::string>> headers;
    size_t head_size = 0;
    int64_t created_us = 0;

    //! identifies the response in the per-thread caches of serialized responses; never reused
    uint64_t id = 0;

    //! Serializes the response for a variant
    DLLLOCAL std::shared_ptr<const std::string> buildWire(int64_t epoch_s, bool close,
            const std::shared_ptr<const HttpFastExtraHeaders>& extra, bool head) const;
};

//! The parts of a request that decide whether a fast route answers it
/** Built by the caller from the parsed request header; the views refer to storage that outlives the call.
*/
struct HttpFastRequestView {
    std::string_view method;
    std::string_view path;
    std::string_view http_version;
    //! the value of the \c Content-Length header, if the request has one
    std::optional<std::string_view> content_length;
    //! the value of the \c Connection header, if the request has one
    std::optional<std::string_view> connection;
    bool has_transfer_encoding = false;
    bool has_expect = false;
    bool has_upgrade = false;
    //! true if bytes of a following request are already buffered
    bool pending = false;
};

//! A value that code outside the I/O threads sets and a route reads
/** The cell is the way a fast route sees state without any hand-off: Qore code (on any thread) sets the value, the
    I/O thread reads it.  Both are single atomic operations on a lock-free integer, so neither can wait for the other,
    and the cell is owned jointly (a shared pointer) by the code that sets it and by every route that reads it, so a
    route can never refer to a cell that is gone.  A boolean is 0 or 1.

    A cell can have a maximum age: when the value has not been set (or touched) for that long, it is stale.  A rule
    never reads a stale cell: the request takes the normal path, so that the handler decides when the code that
    should keep the cell current has stopped.
*/
class HttpFastCell {
public:
    static_assert(std::atomic<int64_t>::is_always_lock_free, "a cell must be lock free: the I/O thread cannot wait");

    //! Creates a cell
    /** @param initial the initial value
        @param max_age_us the age in microseconds after which the value is stale; 0 for never
    */
    DLLLOCAL HttpFastCell(int64_t initial, int64_t max_age_us) : value(initial), updated_us(nowUs()),
            max_age_us(max_age_us) {
    }

    //! Sets the value
    DLLLOCAL void set(int64_t v) {
        value.store(v, std::memory_order_relaxed);
        updated_us.store(nowUs(), std::memory_order_release);
    }

    //! Marks the value as current without changing it
    DLLLOCAL void touch() {
        updated_us.store(nowUs(), std::memory_order_release);
    }

    //! Returns the value
    DLLLOCAL int64_t get() const {
        return value.load(std::memory_order_relaxed);
    }

    //! Returns the age of the value in microseconds
    DLLLOCAL int64_t getAgeUs() const {
        return nowUs() - updated_us.load(std::memory_order_acquire);
    }

    //! Reads the value if it is current
    /** @param v set to the value
        @return false if the value is stale
    */
    DLLLOCAL bool load(int64_t& v) const {
        if (max_age_us && getAgeUs() > max_age_us) {
            return false;
        }
        v = value.load(std::memory_order_relaxed);
        return true;
    }

    //! Returns the maximum age in microseconds; 0 for none
    DLLLOCAL int64_t getMaxAgeUs() const {
        return max_age_us;
    }

    //! Returns the monotonic time in microseconds
    DLLLOCAL static int64_t monotonicUs() {
        return nowUs();
    }

    //! Moves the clock that cells and slots age by forward; for tests, so that they need not wait for time to pass
    /** The clock only moves forward, for every cell and slot of the process.
    */
    DLLLOCAL static void advanceClockForTest(int64_t us) {
        clock_offset_us.fetch_add(us, std::memory_order_relaxed);
    }

private:
    //! monotonic time in microseconds
    DLLLOCAL static int64_t nowUs();

    //! the total of advanceClockForTest()
    static inline std::atomic<int64_t> clock_offset_us{0};

    std::atomic<int64_t> value;
    std::atomic<int64_t> updated_us;
    const int64_t max_age_us;
};

//! How a rule compares a cell with its operand
enum class HttpFastOp {
    Eq, Ne, Lt, Le, Gt, Ge
};

//! A response that code outside the I/O threads publishes and a route serves
/** The holder keeps one immutable serialized response.  Qore code (on any thread) replaces it as a whole with set(); the
    I/O thread takes a reference to the current one with get(), which takes no lock (see HttpFastPublished), so
    neither side can wait for the other.  A slot can have a maximum age: when the response has not been set for that
    long, or was never set, get() returns nothing and the request takes the normal path, so that the handler decides
    when the code that should keep the slot current has stopped.
*/
class HttpFastSlot {
public:
    //! Creates an empty slot
    /** @param max_age_us the age in microseconds after which the response is stale; 0 for never
    */
    DLLLOCAL explicit HttpFastSlot(int64_t max_age_us) : value(std::make_shared<const Value>()),
            max_age_us(max_age_us) {
    }

    //! Replaces the response
    DLLLOCAL void set(std::shared_ptr<const HttpFastResponse> response);

    //! Returns the current response, or nullptr if there is none or it is stale; takes no lock (for the I/O threads)
    DLLLOCAL std::shared_ptr<const HttpFastResponse> get() const;

    //! Returns the age of the response in microseconds, or -1 if none was set (for threads that are not I/O threads)
    DLLLOCAL int64_t getAgeUs() const;

    //! Returns the maximum age in microseconds; 0 for none
    DLLLOCAL int64_t getMaxAgeUs() const {
        return max_age_us;
    }

private:
    //! the response and the time it was set, published as a whole
    struct Value {
        std::shared_ptr<const HttpFastResponse> response;
        int64_t set_us = 0;
    };

    HttpFastPublished<const Value> value;
    const int64_t max_age_us;
};

//! What a route answers a rule (or the route) with: a fixed response or the response of a slot
class HttpFastResponseSource {
public:
    DLLLOCAL HttpFastResponseSource() = default;
    DLLLOCAL HttpFastResponseSource(std::nullptr_t) {
    }
    DLLLOCAL HttpFastResponseSource(std::shared_ptr<const HttpFastResponse> fixed) : fixed(std::move(fixed)) {
    }
    DLLLOCAL HttpFastResponseSource(std::shared_ptr<HttpFastSlot> slot) : slot(std::move(slot)) {
    }

    //! true if there is a fixed response or a slot
    DLLLOCAL explicit operator bool() const {
        return fixed || slot;
    }

    //! Returns the response to serve, or nullptr if the source is a slot with no current response
    DLLLOCAL std::shared_ptr<const HttpFastResponse> get() const {
        return slot ? slot->get() : fixed;
    }

    DLLLOCAL const std::shared_ptr<HttpFastSlot>& getSlot() const {
        return slot;
    }

private:
    std::shared_ptr<const HttpFastResponse> fixed;
    std::shared_ptr<HttpFastSlot> slot;
};

//! One rule of a route: if the cell compares true with the operand, the route answers with the response
struct HttpFastRule {
    std::shared_ptr<HttpFastCell> cell;
    HttpFastOp op = HttpFastOp::Eq;
    int64_t operand = 0;
    HttpFastResponseSource response;
};

//! What a route answers with: the rules in order, the first that holds wins, otherwise the \c otherwise response
/** There is no nesting, no loop and no computation: at most MaxRules comparisons of an atomic integer with a constant.
    A route without rules answers with its \c otherwise response.  A route whose rules do not hold and that has no
    \c otherwise response is not eligible, so the request takes the normal path.
*/
class HttpFastRouteSpec {
public:
    //! The most rules that a route has
    static constexpr size_t MaxRules = 8;

    //! Creates a route specification
    /** @param rules the rules, in order
        @param otherwise the response when no rule holds; can be null
        @param err set to the reason if the specification is not valid

        @return the specification, or nullptr if it is not valid
    */
    DLLLOCAL static std::shared_ptr<const HttpFastRouteSpec> create(std::vector<HttpFastRule> rules,
            HttpFastResponseSource otherwise, std::string& err);

    //! Selects the response
    /** @param why set to the reason if there is none: \c Stale if a cell that a rule reads is not current or the
        slot of the chosen response has no current response, \c NoRule if no rule holds and there is no \c otherwise
        response

        @return the response (a reference that keeps it alive while it is sent) or nullptr
    */
    DLLLOCAL std::shared_ptr<const HttpFastResponse> choose(HttpFastFallback& why) const;

    DLLLOCAL const std::vector<HttpFastRule>& getRules() const {
        return rules;
    }

private:
    DLLLOCAL HttpFastRouteSpec() = default;

    std::vector<HttpFastRule> rules;
    HttpFastResponseSource otherwise;
};

//! A route: a path and what it is answered with
class HttpFastRoute {
public:
    DLLLOCAL HttpFastRoute(std::string path, std::shared_ptr<const HttpFastRouteSpec> spec) :
            path(std::move(path)), spec(std::move(spec)) {
    }

    const std::string path;
    const std::shared_ptr<const HttpFastRouteSpec> spec;
    //! the number of requests answered
    mutable std::atomic<uint64_t> hits{0};
};

//! The fast routes of a listener
/** A listener's table is shared by the connections accepted from it, so a change of the routes reaches the connections
    that are already open.  The table itself is an immutable snapshot that is replaced as a whole: the I/O thread
    takes a reference to the current snapshot without a lock (see HttpFastPublished) and then reads it without any
    lock.  A change builds the new table outside the I/O threads' way: only other changes wait for it.
*/
class HttpFastRoutes {
public:
    //! The longest route path
    static constexpr size_t MaxPathSize = 2048;

    //! Adds or replaces the route for a path
    /** @return false and \c err set if the path is not valid
    */
    DLLLOCAL bool add(const std::string& path, std::shared_ptr<const HttpFastRouteSpec> spec, std::string& err);

    //! Removes the route for a path; returns true if there was one
    DLLLOCAL bool remove(const std::string& path);

    //! Returns the number of routes
    DLLLOCAL size_t size() const;

    //! Answers a request if a route qualifies
    /** @param req the request
        @param epoch_s the current time in seconds since the epoch, for the \c Date header
        @param fallback set to the reason if the request is not answered

        @return the serialized response to send, or nullptr
    */
    DLLLOCAL std::shared_ptr<const std::string> match(const HttpFastRequestView& req, int64_t epoch_s,
            HttpFastFallback& fallback, bool* close_after = nullptr,
            const std::shared_ptr<const HttpFastExtraHeaders>& extra = nullptr);

    //! Answers an HTTP/2 or HTTP/3 request if a route qualifies
    /** The request is a \c GET or \c HEAD without a body (\c END_STREAM, or \c FIN, was on its \c HEADERS frame).  The eligibility rules are
        those of match(); \c http_version is not checked and the request must not have a \c Connection header
        (connection-specific headers are not allowed in HTTP/2 and HTTP/3).

        @param req the request
        @param fallback set to the reason if the request is not answered

        @return the response to send with HTTP/2, or nullptr
    */
    DLLLOCAL std::shared_ptr<const HttpFastResponse> matchH2(const HttpFastRequestView& req,
            HttpFastFallback& fallback);

    //! Returns the number of requests that took the normal path for a reason
    DLLLOCAL uint64_t getFallbacks(HttpFastFallback reason) const {
        return fallbacks[static_cast<size_t>(reason)].load(std::memory_order_relaxed);
    }

    //! Returns the paths of the routes with the number of requests that each has answered
    DLLLOCAL std::vector<std::pair<std::string, uint64_t>> getRouteHits() const;

    //! Returns the name of a fallback reason
    DLLLOCAL static const char* getFallbackName(HttpFastFallback reason);

private:
    typedef std::unordered_map<std::string, std::shared_ptr<HttpFastRoute>> table_t;

    //! The current table, without a lock (for the I/O threads)
    DLLLOCAL std::shared_ptr<const table_t> snapshot() const;

    //! The current table with the mutex of the writers (for threads that are not I/O threads)
    DLLLOCAL std::shared_ptr<const table_t> snapshotLocked() const;

    //! Selects the response for a request; the shared part of match() and matchH2()
    DLLLOCAL std::shared_ptr<const HttpFastResponse> chooseRoute(const HttpFastRequestView& req,
            HttpFastFallback& fallback, bool* close_after, bool h2);

    HttpFastPublished<const table_t> table{std::make_shared<const table_t>()};
    std::array<std::atomic<uint64_t>, static_cast<size_t>(HttpFastFallback::Count)> fallbacks{};
};

//! The fast routes and the extra response headers of a listener, shared by the listener and the sockets accepted from it
/** The listener and every connection accepted from it (and every QUIC session of a listener) refer to one context, so
    that routes or extra headers that are set after a connection was accepted apply to it.  A request reads both with
    no lock (a copy of two shared pointers of an immutable pair; see HttpFastPublished).
*/
class HttpFastContext {
public:
    struct Snapshot {
        std::shared_ptr<HttpFastRoutes> routes;
        std::shared_ptr<const HttpFastExtraHeaders> extra;
    };

    DLLLOCAL HttpFastContext() : published(std::make_shared<const Snapshot>()) {
    }

    //! Returns the routes and the extra headers, without a lock (for the I/O threads)
    DLLLOCAL Snapshot get() const noexcept {
        return *published.get();
    }

    //! Returns the routes and the extra headers with the mutex of the writers (for threads that are not I/O threads)
    DLLLOCAL Snapshot getLocked() const {
        return *published.getLocked();
    }

    DLLLOCAL void setRoutes(std::shared_ptr<HttpFastRoutes> routes) {
        published.update([&routes](const std::shared_ptr<const Snapshot>& cur) {
            auto next = std::make_shared<Snapshot>(*cur);
            next->routes = routes;
            return std::shared_ptr<const Snapshot>(std::move(next));
        });
    }

    DLLLOCAL void setExtraHeaders(std::shared_ptr<const HttpFastExtraHeaders> extra) {
        published.update([&extra](const std::shared_ptr<const Snapshot>& cur) {
            auto next = std::make_shared<Snapshot>(*cur);
            next->extra = extra;
            return std::shared_ptr<const Snapshot>(std::move(next));
        });
    }

private:
    HttpFastPublished<const Snapshot> published;
};

#endif
