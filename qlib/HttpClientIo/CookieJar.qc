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

#! Stored cookie entry
public hashdecl CookieEntry {
    #! Cookie name
    string name;
    #! Cookie value
    string value;
    #! Domain for matching (lowercase, may have leading dot for suffix matching)
    string domain;
    #! Path for matching
    string path = "/";
    #! Expiry time (NOTHING = session cookie)
    *date expires;
    #! Only send over HTTPS
    bool secure = False;
    #! Not accessible to JavaScript (informational for client-side)
    bool http_only = False;
    #! Creation timestamp
    date created = now_us();
}

#! RFC 6265 cookie jar — stores, matches, and injects cookies
/** Thread-safe cookie storage for use with HttpClientConnectionManager.
    Automatically processes Set-Cookie headers from responses and injects
    matching cookies into requests.

    @par Example
    @code{.py}
    CookieJar jar();
    # Process a response
    jar.processResponseHeaders("example.com", "/api", True, response.headers);
    # Get cookies for a request
    *string cookie_hdr = jar.getCookieHeader("example.com", "/api/users", True);
    @endcode

    @since HttpClientIo 1.1
*/
public class CookieJar {
    private {
        #! Stored cookies: key = "domain:name:path" → CookieEntry
        hash<string, hash<CookieEntry>> cookies;

        #! Lock for thread safety
        Mutex m();
    }

    #! Processes Set-Cookie headers from a response and stores cookies
    /** @param host the request host (used as default domain)
        @param request_path the request path (used as default path)
        @param is_secure True if the request was over HTTPS
        @param headers the response headers hash
    */
    processResponseHeaders(string host, string request_path, bool is_secure,
            *hash<auto> headers) {
        if (!headers) {
            return;
        }

        # Set-Cookie — headers are always normalized to lowercase
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
            *hash<CookieEntry> entry = parseCookie(cookie_str, host, request_path);
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
        foreach hash<CookieEntry> entry in (cookies.values()) {
            if (!domainMatches(host_lower, entry.domain)) {
                continue;
            }
            if (!pathMatches(path, entry.path)) {
                continue;
            }
            if (entry.secure && !is_secure) {
                continue;
            }
            pairs += sprintf("%s=%s", entry.name, entry.value);
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

    #! Parses a Set-Cookie header string into a CookieEntry
    /** @param cookie_str the Set-Cookie header value
        @param default_domain the request host (used if no Domain attribute)
        @param default_path the request path (used to compute default path)
        @return the parsed cookie entry, or NOTHING if invalid
    */
    private *hash<CookieEntry> parseCookie(string cookie_str, string default_domain,
            string default_path) {
        # Split on ";" to get name=value and attributes
        list<string> parts = cookie_str.split(";");
        if (!parts) {
            return NOTHING;
        }

        # First part is name=value
        string nv = trim(parts[0]);
        int eq_pos = nv.find("=");
        if (eq_pos < 0) {
            return NOTHING;
        }
        string name = trim(nv.substr(0, eq_pos));
        string value = trim(nv.substr(eq_pos + 1));
        if (!name) {
            return NOTHING;
        }

        hash<CookieEntry> entry = <CookieEntry>{
            "name": name,
            "value": value,
            "domain": default_domain.lwr(),
            "path": computeDefaultPath(default_path),
        };

        # Parse attributes
        for (int i = 1; i < parts.size(); ++i) {
            string attr = trim(parts[i]);
            int attr_eq = attr.find("=");
            string attr_name;
            *string attr_value;
            if (attr_eq >= 0) {
                attr_name = trim(attr.substr(0, attr_eq)).lwr();
                attr_value = trim(attr.substr(attr_eq + 1));
            } else {
                attr_name = attr.lwr();
            }

            if (attr_name == "domain") {
                if (exists attr_value) {
                    string dom = attr_value.lwr();
                    if (dom[0] == ".") {
                        dom = dom.substr(1);
                    }
                    entry.domain = dom;
                }
            } else if (attr_name == "path") {
                if (attr_value && attr_value[0] == "/") {
                    entry.path = attr_value;
                }
            } else if (attr_name == "max-age") {
                if (exists attr_value) {
                    int max_age = attr_value.toInt();
                    if (max_age <= 0) {
                        entry.expires = 1970-01-01T00:00:00Z;
                    } else {
                        entry.expires = now_us() + seconds(max_age);
                    }
                }
            } else if (attr_name == "expires") {
                if (entry.expires.typeCode() != NT_DATE && attr_value) {
                    try {
                        entry.expires = date(attr_value);
                    } catch () {
                    }
                }
            } else if (attr_name == "secure") {
                entry.secure = True;
            } else if (attr_name == "httponly") {
                entry.http_only = True;
            }
        }

        return entry;
    }

    #! Computes the default cookie path from a request path (RFC 6265 Section 5.1.4)
    private static string computeDefaultPath(string request_path) {
        if (!request_path || request_path[0] != "/") {
            return "/";
        }
        int last_slash = request_path.rfind("/");
        if (last_slash <= 0) {
            return "/";
        }
        return request_path.substr(0, last_slash);
    }

    #! Domain matching per RFC 6265 Section 5.1.3
    private static bool domainMatches(string host, string cookie_domain) {
        if (host == cookie_domain) {
            return True;
        }
        # Suffix matching: host ends with ".domain"
        if (host.size() > cookie_domain.size()
                && host.substr(host.size() - cookie_domain.size()) == cookie_domain
                && host[host.size() - cookie_domain.size() - 1] == ".") {
            return True;
        }
        return False;
    }

    #! Path matching per RFC 6265 Section 5.1.4
    private static bool pathMatches(string request_path, string cookie_path) {
        if (request_path == cookie_path) {
            return True;
        }
        if (request_path.size() > cookie_path.size()
                && request_path.substr(0, cookie_path.size()) == cookie_path) {
            if (cookie_path[cookie_path.size() - 1] == "/") {
                return True;
            }
            if (request_path[cookie_path.size()] == "/") {
                return True;
            }
        }
        return False;
    }

    #! Creates a unique key for a cookie
    private static string makeKey(string domain, string name, string path) {
        return domain + ":" + name + ":" + path;
    }
}

} # namespace HttpClientIo
