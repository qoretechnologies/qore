# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaHostRouter.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! Describes where an action's request host comes from, when it is not the connection's own
/** A connection addresses one host, and every action on it sends there.  That is true of nearly every
    API, and it is what makes a connection's URL the boundary that sandboxing and network allow-listing
    are expressed against.

    It is not true of an API whose data plane is sharded per object.  Pinecone's control plane lives at
    \c api.pinecone.io and describes indexes; the 24 operations that actually read and write vectors go to
    a <b>per-index</b> host that only the control plane knows - its own description declares the data-plane
    server as the template \c "https://{index_host}".  The host is therefore a function of one of the
    request's own options, and cannot be known when the connection is made.

    Declaring that relationship here supplies it without any application code: the action names a host
    source, the host source names the action that resolves it and the option carrying the lookup key, and
    the request is sent to the resolved host by a client copied from the connection's own - so the
    connection's credentials, headers and validator all carry over unchanged.

    @par Security
    A host taken from a response is a host the <b>server</b> chose, and the request that follows carries the
    connection's credentials to it.  @ref allowed_host_suffixes is therefore not optional: a resolved host
    that does not match one of the declared suffixes is refused and no request is made, and the scheme is
    taken from @ref scheme rather than from the response, so that a server cannot downgrade a request to
    cleartext and read the credential off the wire.

    @see @ref RestSchemaActions::RestSchemaActionInfo::host_from "RestSchemaActionInfo::host_from"

    @since %RestSchemaActions 1.0
*/
public hashdecl RestSchemaHostSourceInfo {
    #! The host source name, as named by an action's \c host_from
    string name;

    #! The action that resolves the host
    /** Must be exported by the set that declares this host source, and must not itself take its host from
        a host source - a lookup that needed its own host resolved first could never be made.
    */
    string action;

    #! The option of the <b>calling</b> action supplying the lookup key
    /** Every action naming this host source must declare it, which is checked when the action set is
        built.  Pair it with
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ref_data "ref_data" naming the same collection,
        so that the user picks the object from a dropdown and never sees a host at all.
    */
    string key_option;

    #! The option of the lookup action the key is passed as
    /** Defaults to @ref key_option, which is the usual case: an index is named \c index on both sides.
    */
    *string lookup_option;

    #! Fixed options passed to the lookup action
    *hash<auto> options;

    #! A path of field names leading from the lookup action's output to the host
    /** The value must be a non-empty string: a bare host, or an absolute URL whose scheme matches
        @ref scheme.
    */
    softlist<string> host_field;

    #! The URI scheme the resolved host is addressed over
    /** Taken from here and never from the response, so that a compromised or misconfigured service cannot
        move a credentialed request onto cleartext HTTP.  An absolute URL in the response whose scheme
        differs from this one is refused.
    */
    string scheme = "https";

    #! The host suffixes a resolved host is permitted to have
    /** The connection's URL is what normally bounds where its credentials go.  A host that comes out of a
        response is outside that bound, so it is checked against this list and the request is refused when
        it matches none of them.

        A suffix matches a host that equals it or is a subdomain of it, so \c "pinecone.io" admits
        \c "idx-1234.svc.aped-1234-a56b.pinecone.io" and rejects \c "evil-pinecone.io".  A leading \c "."
        is accepted and ignored, so \c ".pinecone.io" means the same thing.

        Declaring no suffix at all is an error rather than "anything goes".
    */
    softlist<string> allowed_host_suffixes;

    #! The overlay applied to the synthesized key option in every action that names this host source
    /** The key option is not in the schema - an API that shards its data plane per object addresses the
        object with the host, so nothing describes it - so it is synthesized, and this is where its
        presentation is declared.  Give it a display name, a description and a
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ref_data "ref_data" naming the listing the
        objects come from, and every action that names this host source offers the same dropdown without
        repeating any of it.

        An action's own overlay entry for the same option refines this rather than replacing it, so a
        single action can still say something the others do not.
    */
    *hash<RestSchemaFieldOverlayInfo> key_overlay;

    #! A description of the host source
    /** Used as the key option's description when @ref key_overlay does not give one.
    */
    *string desc;
}

#! Resolves and caches the per-host REST clients an action set's host sources need
/** One client per <b>host</b>, not per endpoint: every action routed to the same resolved host shares one,
    and each is a copy of the connection's own client made with
    @ref RestClient::AbstractRestClient::copyWithUrl() "copyWithUrl()", so it keeps the connection's
    configuration - tokens, default headers, validator, OAuth2 state - and shares its connection manager,
    which pools by URL.  Hosts common to the connection's URL and a resolved one therefore reuse pooled
    connections.

    A router is bound to the client it copies from, which is what makes the cache safe: two connections to
    the same service are two routers, so one account's resolved host can never be used with another
    account's credentials.

    The cache lives as long as the router does.  Give a router the lifetime of the client it wraps to get
    the most out of it; an application that constructs one per data provider instance still gets a correct
    result, only more lookups.

    @note This class is thread-safe.  A resolution is performed <b>outside</b> the lock, because it is a
    network call and holding a lock across one would serialize every thread using the router behind the
    slowest lookup.  Two threads racing for the same key may therefore both resolve it; the first to
    finish wins and the loser discards its copy, which costs one redundant lookup and keeps the invariant
    that a given key always yields the same client object.

    @since %RestSchemaActions 1.0
*/
public class RestSchemaHostRouter {
    private {
        #! The client every per-host client is copied from
        AbstractRestClient parent;

        #! Guards @ref cache
        Mutex lck();

        #! Per-host clients, keyed by host source name and then by lookup key
        /** Two levels rather than one composite key: a %Qore string is NUL-terminated, so a separator
            cannot be a character that is guaranteed not to appear in either half, and every printable
            separator is legal in some lookup key.
        */
        hash<string, hash<string, AbstractRestClient>> cache = {};
    }

    #! Creates the router
    /** @param parent the connection's own REST client, which every per-host client is copied from
    */
    constructor(AbstractRestClient parent) {
        self.parent = parent;
    }

    #! Returns the client every per-host client is copied from
    AbstractRestClient getParentClient() {
        return parent;
    }

    #! Returns the number of resolved hosts held in the cache
    /** Intended for tests and diagnostics; a caller cannot rely on a particular value because another
        thread may resolve a host at any time.
    */
    int getCachedCount() {
        AutoLock al(lck);
        int rv = 0;
        map rv += $1.size(), cache.iterator();
        return rv;
    }

    #! Returns the client for the given lookup key, resolving and caching its host on the first use
    /** @param src the host source declaration
        @param key the lookup key, as supplied by the calling action's
        @ref RestSchemaActions::RestSchemaHostSourceInfo::key_option "key_option"
        @param resolve returns the host for \a key; called at most once per key per router, and only
        when the key is not already cached

        @return the client addressing the resolved host

        @throw REST-SCHEMA-HOST-ERROR the resolved host is empty, carries credentials, uses a scheme other
        than the declared one, or does not match any declared host suffix
    */
    AbstractRestClient getClient(hash<RestSchemaHostSourceInfo> src, string key, code<string()> resolve) {
        {
            AutoLock al(lck);
            if (*AbstractRestClient rv = cache{src.name}{key}) {
                return rv;
            }
        }
        # resolved outside the lock; see the class-level note on why, and on who wins a race
        string url = RestSchemaHostRouter::checkResolvedHost(src, resolve());
        AbstractRestClient client = parent.copyWithUrl(url);
        AutoLock al(lck);
        if (*AbstractRestClient rv = cache{src.name}{key}) {
            return rv;
        }
        cache{src.name}{key} = client;
        return client;
    }

    #! Turns a resolved host into the URL to address it at, refusing anything the declaration does not allow
    /** @param src the host source declaration
        @param host the host as the lookup action reported it: a bare host, or an absolute URL

        @return the URL to address the host at

        @throw REST-SCHEMA-HOST-ERROR the host is empty, unparseable, carries credentials, uses a scheme
        other than the declared one, or does not match any declared host suffix

        @note Everything here fails closed.  The value came from a response, and the request that follows
        carries the connection's credentials, so a value that is not obviously safe is not used.
    */
    static string checkResolvedHost(hash<RestSchemaHostSourceInfo> src, string host) {
        string raw = trim(host);
        if (!raw.val()) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to an empty host", src.name);
        }
        # a bare host is the common answer; an absolute URL is accepted as well, and its scheme is
        # checked rather than trusted
        string url = raw =~ /^[a-zA-Z][-a-zA-Z0-9+.]*:\/\// ? raw : (src.scheme + "://" + raw);
        hash<UrlInfo> info;
        try {
            info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to %y, which is not a usable "
                "host or URL: %s: %s", src.name, host, ex.err, ex.desc);
        }
        if (info.protocol.lwr() != src.scheme.lwr()) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to %y, which uses scheme %y "
                "where the declaration requires %y; the scheme is never taken from the response, so that a "
                "credentialed request cannot be moved onto cleartext by the service that answers the "
                "lookup", src.name, host, info.protocol, src.scheme);
        }
        if (info.username.val() || info.password.val()) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to a URL carrying credentials, "
                "which is never valid for a host the connection's own credentials are sent to", src.name);
        }
        if (!info.host.val()) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to %y, which names no host",
                src.name, host);
        }
        if (!RestSchemaHostRouter::hostMatchesSuffix(info.host, src.allowed_host_suffixes)) {
            throw "REST-SCHEMA-HOST-ERROR", sprintf("host source %y resolved to host %y, which is not any of "
                "%y or a subdomain of one; the connection's credentials are sent to the resolved host, so a "
                "host outside the declared suffixes is refused rather than tried", src.name, info.host,
                src.allowed_host_suffixes);
        }
        return sprintf("%s://%s%s%s", src.scheme.lwr(), info.host, info.port ? ":" + info.port : "",
            info.path ?? "");
    }

    #! Returns @ref True if the host equals one of the suffixes or is a subdomain of one
    /** Compared label by label rather than as a string, so that \c "evil-pinecone.io" does not match
        \c "pinecone.io" - which is exactly the check a naive string suffix test gets wrong.

        @param host the host to check
        @param suffixes the permitted suffixes, with or without a leading \c "."

        @return @ref True if the host is permitted
    */
    static bool hostMatchesSuffix(string host, softlist<string> suffixes) {
        string h = host.lwr();
        foreach string suffix in (suffixes) {
            string s = trim(suffix).lwr();
            if (s[0] == ".") {
                s = s.substr(1);
            }
            if (!s.val()) {
                continue;
            }
            if (h == s) {
                return True;
            }
            if (h.size() > s.size() && h.substr(h.size() - s.size() - 1) == "." + s) {
                return True;
            }
        }
        return False;
    }
}
}
