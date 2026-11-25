# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientCreatorProof.qc the proof that the creator of a client passed the caller check of a manager

/** HttpClientCreatorProof.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The register of the creator proofs that were issued and of their grants (qore#7262, qore#7236)
/** Not public: only HttpClientCreatorProof uses it.  It is referenced by the class's static member and by every proof,
    so it lives as long as any proof that is recorded in it
*/
class HttpClientCreatorProofRegistry {
    public {
        #! the lock of \c issued and \c grants
        Mutex lock();

        #! the proofs issued and not yet destroyed, by object
        hash<string, bool> issued = {};

        #! the grants of proofs to the next client of their target created in a thread (see
        #! HttpClientCreatorProofGrant), by thread, innermost last; each has the \c id of its grant and its \c proof
        hash<string, list<hash<auto>>> grants = {};
    }
}

#! An opaque proof that the code that created a client passed the caller check of a connection manager (qore#7262)
/** A client that creates its @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager" lazily,
    when it connects or reconnects, or again when it is closed, takes a proof with forClient() when it is created and
    gives it to the manager's constructor each time: a client that trusted code created and lent to restricted code
    therefore keeps working for it, while restricted code that creates a client of its own gets no proof and is refused
    when the client creates its manager, as before.

    A proof is bound to what the code that took it gave: the origin (host and port) of the URL of the client when it was
    created, or of the connection object that creates the client (see
    @ref HttpClientIo::HttpClientCreatorProofGrant "HttpClientCreatorProofGrant"), its proxy, and its CA certificate
    locations.  A manager that is created with a proof for code that could not create one itself checks every
    connection that it creates against them: it connects only to that origin, only through that proxy (or with no
    proxy if the client had none; HTTP/3, which cannot use a proxy, is refused if it had one), and with no other CA
    certificate locations, so restricted code that changes the target, the proxy or the CA locations of a lent client,
    of a lent connection or of its manager, or that obtains the proof, reaches nothing that the client could not; a
    redirect of such a client to the same origin is followed, while one to another origin is refused.

    An origin is whether the connection uses TLS, the host and the port: the same host and port with plain HTTP instead
    of TLS, or the other way round, is another origin.  The scheme decides TLS: \c https, \c wss and the secure schemes
    of connection objects that their clients map to \c https (such as \c jsonrpcs, \c yamlrpcs, \c mcps and
    \c a2as) use it; \c http, \c ws and the plain schemes (\c jsonrpc, \c yamlrpc, \c mcp, \c a2a, ...) do not;
    a URL without a scheme uses TLS, as the manager does, and a URL of another scheme allows no origin.  A URL without
    a port has the default port of its TLS mode (443 or 80).  Host names are compared case-insensitively, without a
    trailing dot and IPv6 addresses without their brackets; other spellings of a host (an internationalized name and
    its punycode form, a name and its address) are other hosts.  The host is resolved by the manager as for any client,
    as the code that created the client chose it.

    Only this class issues proofs: its constructor is private, and a proof is genuine only if it is recorded in the
    class's own register (genuine()), so neither an object of another class with the same name and signature nor a
    copy of a proof is accepted.  A proof covers what the code that took it may do: a proof of code restricted from
    the \c FILESYSTEM functional domain does not allow a manager with CA certificate locations.

    @since HttpClientIo 1.0
*/
public final class HttpClientCreatorProof {
    private:internal {
        #! True if the code that took the proof may give a manager CA certificate locations
        bool ca_locations;

        #! the origins that the proof allows, as \c "scheme|host:port" keys (scheme \c http or \c https)
        hash<string, bool> origins;

        #! the proxy that the proof allows, as a \c "scheme|host:port" key, or nothing for none
        *string proxy;

        #! the CA certificate file and directory that the proof allows, if any
        *string ca_file;
        *string ca_path;

        #! the register of the proofs that were issued and of their grants (see HttpClientCreatorProofRegistry)
        static HttpClientCreatorProofRegistry registry();

        #! the register that this proof is recorded in
        /** Each proof keeps a reference to the register, so that the register outlives every proof that is recorded
            in it: a proof can be destroyed after the static members of the class were cleared, when the program ends
            and an object that holds a proof (a client or a connection manager) is released after them (qore#7236)
        */
        HttpClientCreatorProofRegistry reg;

        #! True for the schemes that use TLS, False for the plain ones, of the schemes that are mapped to HTTP
        const SchemeTls = {
            "http": False, "ws": False, "jsonrpc": False, "jsonrpcio": False, "yamlrpc": False, "yamlrpcio": False,
            "mcp": False, "a2a": False, "sewiows": False, "rest": False,
            "https": True, "wss": True, "jsonrpcs": True, "jsonrpcios": True, "yamlrpcs": True, "yamlrpcios": True,
            "mcps": True, "a2as": True, "sewiowss": True, "rests": True,
        };
    }

    #! Issues a proof
    private:internal constructor(bool ca_locations, hash<string, bool> origins, *hash<auto> opts) {
        self.ca_locations = ca_locations;
        self.origins = origins;
        if (opts.proxy.val()) {
            proxy = HttpClientCreatorProof::getProxyKey(opts.proxy);
        }
        ca_file = opts.ssl_ca_file;
        ca_path = opts.ssl_ca_path;
        reg = registry;
        AutoLock al(reg.lock);
        reg.issued{self.uniqueHash()} = True;
    }

    #! Withdraws the proof
    /** Uses the register that the proof was recorded in, which the proof keeps, so that it works whenever the proof is
        destroyed, also after the static members of the class were cleared at the end of the program (qore#7236)
    */
    destructor() {
        AutoLock al(reg.lock);
        remove reg.issued{self.uniqueHash()};
    }

    #! A copy of a proof is not a proof
    /** @throw PERMISSION-DENIED always
    */
    copy() {
        throw "PERMISSION-DENIED", "an HTTP client creator proof cannot be copied";
    }

    #! Returns a proof for the calling code, bound to the given target
    /** @param url the URL of the target
        @param opts the options that the proof is bound to: \c proxy (the URL of the proxy, if any), \c ssl_ca_file
        and \c ssl_ca_path (the CA certificate locations, if any)

        @return a proof that the calling code passed the caller check of a connection manager

        @throw PERMISSION-DENIED the calling code is restricted from the \c NETWORK functional domain
    */
    static HttpClientCreatorProof get(string url, *hash<auto> opts) {
        if (SandboxManager::isLentToRestricted(DOM_NETWORK)) {
            throw "PERMISSION-DENIED", "the calling code may not create an HTTP client connection manager, as it is "
                "restricted from the NETWORK functional domain";
        }
        return new HttpClientCreatorProof(!SandboxManager::isLentToRestricted(DOM_FILESYSTEM),
            HttpClientCreatorProof::getOrigins(url), opts);
    }

    #! Returns a proof for the calling code bound to the given target, or nothing if it cannot have one
    /** @param url the URL of the target; with no URL, no proof is returned
        @param opts the options that the proof is bound to (see get())

        @return a proof if the calling code may create a connection manager, otherwise nothing: a client created by
        such code is refused when it creates its manager
    */
    static *HttpClientCreatorProof tryGet(*string url, *hash<auto> opts) {
        if (!url.val() || SandboxManager::isLentToRestricted(DOM_NETWORK)) {
            return;
        }
        return new HttpClientCreatorProof(!SandboxManager::isLentToRestricted(DOM_FILESYSTEM),
            HttpClientCreatorProof::getOrigins(url), opts);
    }

    #! Returns the proof of a client that is being created
    /** In order: the innermost proof that a connection object grants in this thread to the next client of its target
        (see HttpClientCreatorProofGrant) whose origins include the origin of the client's URL, which is taken, so that a
        client of another target that is created in the meantime (to fetch a token, for example) does not take it; else
        a proof for the calling code, bound to the client's URL (tryGet())

        @param url the URL of the client
        @param opts the options that a proof for the calling code is bound to (see get())

        @return the proof of the client, or nothing

        @note a granted proof is bound to what the connection that grants it fixed when it was created, so code that
        takes it reaches only what that connection's clients can reach
    */
    static *HttpClientCreatorProof forClient(*string url, *hash<auto> opts) {
        if (url.val()) {
            # the origin of the client's URL (a URL has one origin at most)
            *string want = HttpClientCreatorProof::getOrigins(url).firstKey();
            if (want) {
                HttpClientCreatorProofRegistry r = registry;
                AutoLock al(r.lock);
                string tid = string(gettid());
                if (*list<hash<auto>> l = r.grants{tid}) {
                    for (int i = l.size() - 1; i >= 0; --i) {
                        HttpClientCreatorProof granted = l[i].proof;
                        if (granted.origins{want}) {
                            splice l, i, 1;
                            if (l) {
                                r.grants{tid} = l;
                            } else {
                                remove r.grants{tid};
                            }
                            return granted;
                        }
                    }
                }
            }
        }
        return HttpClientCreatorProof::tryGet(url, opts);
    }

    #! Returns True if the given object is a proof that this class issued
    static bool genuine(object proof) {
        HttpClientCreatorProofRegistry r = registry;
        AutoLock al(r.lock);
        return r.issued{proof.uniqueHash()} ?? False;
    }


    #! Returns True if the proof allows connections to the given origin
    /** @param tls True if the connection uses TLS
        @param host the host of the target
        @param port the port of the target
    */
    bool allowsOrigin(bool tls, string host, int port) {
        return origins{HttpClientCreatorProof::getKey(tls, host, port)} ?? False;
    }

    #! Returns True if the proof allows connections through the given proxy, or with no proxy
    /** @param proxy_info the \c host, \c port and \c ssl of the proxy, or nothing for a connection without a proxy

        @return True if the proxy is the one that the proof is bound to, or if there is none and the proof has none
    */
    bool allowsProxy(*hash<auto> proxy_info) {
        if (!proxy_info) {
            return !exists proxy;
        }
        return exists proxy
            && proxy == HttpClientCreatorProof::getKey(proxy_info.ssl ?? False, proxy_info.host ?? "",
                proxy_info.port ?? 0);
    }

    #! Returns True if the proof allows a connection to the given URL through the given proxy
    /** The origin of the URL and the proxy are derived as for the proof itself (see the class description): this is
        the check for a client that is not created with the proof, such as a synchronous client that a connection
        object creates (see @ref ConnectionProvider::AbstractConnection::checkLentNetworkTarget()
        "AbstractConnection::checkLentNetworkTarget()")

        @param url the URL that the client connects to
        @param proxy_url the URL of the client's proxy, if any

        @return True if the origin of \a url is one that the proof allows and \a proxy_url is its proxy (or both have
        none)

        The URLs checked here (\a url and \a proxy_url alike) allow nothing if they have user information (a
        connection class gives the target URL without it, ex: \c HTTPClient::getURL(UC_TARGET), as the code that
        was lent the connection can set the credentials so that the rendered URL reads as another target), if their
        authority is not of the \c "scheme://authority" form, or, for \a url, if a socket path and a socket
        authority do not match exactly (see socketAuthorityMatches()); an IPv6 host without brackets allows nothing
        (ambiguousHost()), and any error in checking a URL allows nothing.  These checks apply only to the checked
        URLs: the URL that a proof is issued for (a creator's configured URL, also with credentials) and the URL of a
        client that takes a granted proof are derived as before.

        @since HttpClientIo 1.0 (qore#7262); qore#7236 refuses ambiguous URLs
    */
    bool allowsUrl(string url, *string proxy_url) {
        try {
            if (!HttpClientCreatorProof::checkedUrlOk(url)
                    || (proxy_url.val() && !HttpClientCreatorProof::checkedUrlOk(proxy_url))) {
                return False;
            }
            *string key = HttpClientCreatorProof::getOrigins(url, True).firstKey();
            if (!key || !origins{key}) {
                return False;
            }
            if (proxy_url.val()) {
                # an ambiguous or invalid proxy URL has no key and matches nothing, also not a proof's own such proxy
                *string proxy_key = HttpClientCreatorProof::getProxyKey(proxy_url);
                return exists proxy && proxy_key.val() && proxy == proxy_key;
            }
            return !exists proxy;
        } catch (hash<ExceptionInfo> ex) {
            # a URL that cannot be checked (one that is not valid in its encoding, for example) allows nothing
            return False;
        }
    }

    #! Returns True if the proof allows the given CA certificate locations
    /** @param file the CA certificate file, if any
        @param path the CA certificate directory, if any
    */
    bool allowsCaLocations(*string file, *string path) {
        if (!file.val() && !path.val()) {
            return True;
        }
        return ca_locations && (file ?? "") == (ca_file ?? "") && (path ?? "") == (ca_path ?? "");
    }

    #! Grants this proof to the next client created in this thread, while the returned object exists
    /** @return an object that removes the grant when it goes out of scope, if it was not taken
    */
    HttpClientCreatorProofGrant grantNext() {
        HttpClientCreatorProofGrant grant(self);
        AutoLock al(reg.lock);
        hash<auto> entry = {"id": grant.uniqueHash(), "proof": self};
        reg.grants{string(gettid())} += (entry,);
        return grant;
    }

    #! Removes the given grant of this proof in this thread if it was not taken (see HttpClientCreatorProofGrant)
    /** @param id the ID of the grant: the unique hash of its HttpClientCreatorProofGrant object
    */
    revokeGrant(string id) {
        AutoLock al(reg.lock);
        string tid = string(gettid());
        *list<hash<auto>> l = reg.grants{tid};
        for (int i = l.size() - 1; i >= 0; --i) {
            if (l[i].id == id && l[i].proof === self) {
                splice l, i, 1;
                if (l) {
                    reg.grants{tid} = l;
                } else {
                    remove reg.grants{tid};
                }
                return;
            }
        }
    }

    #! Returns the key of an origin or a proxy: \c "scheme|host:port"
    private:internal static string getKey(bool tls, string host, int port) {
        return sprintf("%s|%s:%d", tls ? "https" : "http", HttpClientCreatorProof::canonicalHost(host), port);
    }

    #! Returns the canonical form of a host name: lower case, without a trailing dot and IPv6 brackets
    /** A UNIX domain socket path is returned as it is
    */
    private:internal static string canonicalHost(string host) {
        if (host =~ /^\//) {
            return host;
        }
        host = host.lwr();
        if (host =~ /^\[.*\]$/) {
            host = host.substr(1, -1);
        }
        return regex_subst(host, "\\.+$", "");
    }

    #! Returns the origins of the given URL, as \c "host:port" keys
    private:internal static hash<string, bool> getOrigins(string url, *bool strict) {
        hash<auto> u;
        try {
            u = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            return {};
        }
        if (!u.host.val() || HttpClientCreatorProof::ambiguousHost(url, u.host)) {
            return {};
        }
        # a URL that is checked against a proof (allowsUrl()) gives a socket path only with a canonical socket
        # authority, and a socket authority only a socket path (qore#7236)
        if (strict && !HttpClientCreatorProof::socketAuthorityMatches(url, u)) {
            return {};
        }
        # a UNIX domain socket target: plain HTTP with port 0, as the manager connects to it
        if (!u.port && u.host =~ /^\//) {
            return (u.protocol ?? "http") == "http" ? {HttpClientCreatorProof::getKey(False, u.host, 0): True} : {};
        }
        *bool tls = SchemeTls{(u.protocol ?? "https").lwr()};
        if (!exists tls) {
            return {};
        }
        return {HttpClientCreatorProof::getKey(tls, u.host, u.port ?? (tls ? 443 : 80)): True};
    }

    #! Returns True if a URL that allowsUrl() checks has an authority and no user information (qore#7236)
    /** The URL must be of the \c "scheme://authority" form, the authority (ending at the first \c "/", \c "?" or
        \c "#") must have no \c "@" and must not be followed by \c "?" or \c "#" (which a client reads as part of the
        host when a path follows), and parse_url() must give no user name or password: a URL that a client renders
        with its credentials, which the code that was lent the connection can set (a password with \c "@host:port/"
        or \c "@host?"), can be read as another target

        @param url the URL

        @return True if the URL has an authority without user information; any error gives False
    */
    private:internal static bool checkedUrlOk(string url) {
        try {
            *list<*string> m = url =~ x/^[A-Za-z][-A-Za-z0-9+.]*:\/\/([^\/?#]*)/;
            if (!m || !(m[0] ?? "").val() || (m[0] ?? "") =~ /@/) {
                return False;
            }
            # an authority followed by "?" or "#" is ambiguous: a client's URL with a path after it (QoreURL ends the
            # authority at the first "/" and reads "?" as a query only when no path follows) has another host and port
            # than the URL without the path, as a target-only URL is given (qore#7236)
            if (url =~ /^[A-Za-z][-A-Za-z0-9+.]*:\/\/[^\/?#]*[?#]/) {
                return False;
            }
            hash<auto> u = parse_url(url);
            return !exists u.username && !exists u.password;
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
    }

    #! Returns True if a URL gives a UNIX domain socket path exactly when its authority is a canonical socket authority
    /** The host and port after the user information must be \c "socket=" followed by the path that parse_url() gives
        with every reserved character encoded (as @ref encode_url() "encode_url(path, True)" and
        @ref Qore::HTTPClient::getURL() "HTTPClient::getURL()" give it), with no port, so that the path is the only
        one that the URL can be read as: a raw \c "@", \c "%", \c ":", \c "#" or \c "?" in the path, or an encoded
        path that decodes to another path, does not match; and a URL whose authority is not a socket authority must not
        give a socket path (qore#7236)

        @param url the URL
        @param u the parsed URL

        @return True if the socket path and the socket authority match (or the URL has neither); any error gives False
    */
    private:internal static bool socketAuthorityMatches(string url, hash<auto> u) {
        try {
            string hostport = (url =~ x/^[^\/]*\/\/([^\/?#]*)/)[0] ?? "";
            int at = hostport.rfind("@");
            if (at >= 0) {
                hostport = hostport.substr(at + 1);
            }
            bool socket_authority = hostport.lwr().equalPartial("socket=");
            bool socket_path = (u.host ?? "") =~ /^\//;
            if (!socket_authority && !socket_path) {
                return True;
            }
            return socket_authority && socket_path && !u.port && hostport == "socket=" + encode_url(u.host, True);
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
    }

    #! Returns True if the host of a URL is an IPv6 address without brackets
    /** Its end and the start of the port cannot be told apart, so such a URL allows nothing (a client can render an
        IPv6 host without brackets; RFC 3986 requires them)
    */
    private:internal static bool ambiguousHost(string url, string host) {
        # a URL that is not valid in its encoding cannot be checked: it is ambiguous (qore#6553)
        try {
            if (host !~ /:/ || host =~ /^\//) {
                return False;
            }
            # the host and port of the authority follow its last "@": userinfo can hold brackets too
            string hostport = (url =~ x/^[^\/]*\/\/([^\/?#]*)/)[0] ?? "";
            int at = hostport.rfind("@");
            if (at >= 0) {
                hostport = hostport.substr(at + 1);
            }
            return hostport !~ /^\[/;
        } catch (hash<ExceptionInfo> ex) {
            return True;
        }
    }

    #! Returns the \c "host:port" key of a proxy URL
    private:internal static *string getProxyKey(string url) {
        hash<auto> u;
        try {
            u = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            return "";
        }
        if (!u.host.val() || HttpClientCreatorProof::ambiguousHost(url, u.host)) {
            return "";
        }
        bool tls = (u.protocol == "https");
        return HttpClientCreatorProof::getKey(tls, u.host, u.port ?? (tls ? 443 : 80));
    }
}

#! The grant of a proof to the next client that the code of a connection object creates in this thread (qore#7262)
/** A connection object takes a proof when it is created, bound to its URL, and grants it with
    @ref HttpClientIo::HttpClientCreatorProof::grantNext() "HttpClientCreatorProof::grantNext()" while it creates its
    client in @ref ConnectionProvider::AbstractConnection::get() "get()" or
    @ref ConnectionProvider::AbstractConnection::getAsync() "getAsync()", also for restricted code that was lent the
    connection: the first client of its target created in this thread takes it (see
    @ref HttpClientIo::HttpClientCreatorProof::forClient() "HttpClientCreatorProof::forClient()").  The proof is never
    handed to the code of the connection's class, so code that derives a class from it cannot read it, and it is bound
    to the connection's URL when it was created, so changing the connection's URL or options does not move it.

    @since HttpClientIo 1.0
*/
public final class HttpClientCreatorProofGrant {
    private:internal {
        HttpClientCreatorProof proof;
    }

    #! Records the grant (see HttpClientCreatorProof::grantNext())
    constructor(HttpClientCreatorProof proof) {
        self.proof = proof;
    }

    #! Removes the grant if it was not taken
    destructor() {
        proof.revokeGrant(self.uniqueHash());
    }
}
}
