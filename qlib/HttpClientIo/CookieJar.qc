# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CookieJar.qc RFC 6265 cookie jar for HttpClientIo

/** CookieJar.qc Copyright 2026 Qore Technologies, s.r.o.

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
*/

#! Main namespace
public namespace HttpClientIo {

#! RFC 6265 cookie jar — stores, matches, and injects cookies
/** Thread-safe cookie storage for use with HttpClientConnectionManager.
    Delegates parsing and matching to the shared HttpUtil module.

    @since HttpClientIo 1.1
*/
public class CookieJar {
    private {
        #! Stored cookies: key = "domain:name:path" -> CookieInfo
        hash<string, hash<HttpUtil::CookieInfo>> cookies;

        #! Lock for thread safety
        Mutex m();
    }

    #! Processes Set-Cookie headers from a response and stores cookies
    /** @param host the request host (used as default domain)
        @param request_path the request path (used as default path)
        @param is_secure True if the request was over HTTPS
        @param headers the response headers hash (keys normalized to lowercase)
    */
    processResponseHeaders(string host, string request_path, bool is_secure,
            *hash<auto> headers) {
        if (!headers) {
            return;
        }

        # Headers are always normalized to lowercase
        auto set_cookie = headers."set-cookie";
        if (!exists set_cookie) {
            return;
        }

        list<string> cookie_strings;
        if (set_cookie.typeCode() == NT_LIST) {
            cookie_strings = set_cookie;
        } else if (set_cookie.typeCode() == NT_STRING) {
            cookie_strings = (set_cookie,);
        } else {
            return;
        }

        AutoLock al(m);
        foreach string cookie_str in (cookie_strings) {
            *hash<HttpUtil::CookieInfo> entry = HttpUtil::parse_cookie(cookie_str, host,
                request_path);
            if (entry) {
                # Reject Secure cookies from non-secure origins
                if (entry.secure && !is_secure) {
                    continue;
                }
                string key = makeKey(entry.domain, entry.name, entry.path);
                cookies{key} = entry;
            }
        }
    }

    #! Returns the Cookie header value for a request, or NOTHING if no cookies match
    /** @param host the request host
        @param path the request path
        @param is_secure True if the request is over HTTPS
        @return the Cookie header value (e.g., "name1=val1; name2=val2") or NOTHING
    */
    *string getCookieHeader(string host, string path, bool is_secure) {
        AutoLock al(m);

        # Purge expired cookies
        date now = now_us();
        list<string> expired;
        foreach string key in (keys cookies) {
            *date exp = cookies{key}.expires;
            if (exp.typeCode() == NT_DATE && exp < now) {
                expired += key;
            }
        }
        foreach string key in (expired) {
            remove cookies{key};
        }

        # Find matching cookies
        list<string> pairs;
        string host_lower = host.lwr();
        foreach hash<HttpUtil::CookieInfo> entry in (cookies.values()) {
            if (!HttpUtil::domain_matches(host_lower, entry.domain)) {
                continue;
            }
            if (!HttpUtil::path_matches(path, entry.path)) {
                continue;
            }
            if (entry.secure && !is_secure) {
                continue;
            }
            pairs += sprintf("%s=%s", entry.name, entry.value ?? "");
        }

        if (!pairs) {
            return NOTHING;
        }
        return pairs.join("; ");
    }

    #! Returns the number of stored cookies
    int size() {
        AutoLock al(m);
        return cookies.size();
    }

    #! Clears all stored cookies
    clear() {
        AutoLock al(m);
        cookies = {};
    }

    #! Creates a unique key for a cookie
    private static string makeKey(*string domain, string name, string path) {
        return (domain ?? "") + ":" + name + ":" + path;
    }
}

} # namespace HttpClientIo
