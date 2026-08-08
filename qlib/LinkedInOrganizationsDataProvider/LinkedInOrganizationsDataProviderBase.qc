# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInOrganizationsDataProvider module
public namespace LinkedInOrganizationsDataProvider {
#! The LinkedIn Organizations data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class LinkedInOrganizationsDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("LinkedInRestClient::LinkedInRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The LinkedIn REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the LinkedInRestClientIo constructor; only used if a client "
                    "object is created for a call",
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls (async-capable)
        *LinkedInRestClient::LinkedInRestClientIo rest;
    }

    #! Creates the object from a REST client
    constructor(LinkedInRestClient::LinkedInRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            rest = new LinkedInRestClient::LinkedInRestClientIo(copts.restclient_options
                + {"api_profile": "dma"});
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a REST client for the framework's generic \c "make-api-call" action
    /** Every DMA endpoint is versioned, and LinkedIn never defaults to the latest version — a
        missing \c LinkedIn-Version header is an error rather than a fallback — so a client
        discovered reflectively would fail on every call.  A client carrying this connection's API
        version and the Rest.li protocol version as default headers is returned instead, sharing
        this provider's connection manager.
    */
    private *object getRestClientForGenericCallImpl() {
        if (!rest) {
            return;
        }
        *string tok = rest.getToken();
        if (!tok.val()) {
            return rest;
        }
        return new RestClientIo::RestClientIo({
            "url": rest.getUrl(),
            "token": tok,
            "token_type": rest.getTokenType(),
            "data": "json",
            "allow_any_response": True,
            "headers": {
                LinkedInRestClient::LinkedInVersionHeader: rest.getLinkedInVersion(),
                LinkedInRestClient::RestliProtocolVersionHeader:
                    LinkedInRestClient::RestliProtocolVersion,
            },
        }, rest.getConnectionManager());
    }

    #! Returns the reference data types supported by this provider
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "organizations": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "organizations": return getReferenceOrganizations(action_opts);
        }
    }

    #! Returns the organizations the authenticated member administers, as selectable values
    private *list<hash<AllowedValueInfo>> getReferenceOrganizations(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            foreach hash<auto> org in (getAdministeredOrganizations()) {
                string name = org.name ?? org.id.toString();
                rv += <AllowedValueInfo>{
                    "display_name": name,
                    "short_desc": org.tagline ?? name,
                    "desc": org.description ?? org.tagline ?? name,
                    "value": org.id,
                    "image": org.logo_url,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceOrganizations: error: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns the organizations the authenticated member administers
    /** Two calls: the access-control finder says *which* organizations the member administers, and
        it returns only their URNs, so the profiles are then resolved in one batch.

        @return the normalized organizations, in access-control order
    */
    private list<auto> getAdministeredOrganizations() {
        hash<auto> resp = doDmaRequest("GET", OrganizationAclsPath, {
            "q": "roleAssignee",
            "state": "(value:APPROVED)",
            "start": 0,
            "count": 100,
        });
        # an organization can appear more than once when the member holds several roles on it
        list<string> ids = ();
        hash<string, bool> seen = {};
        foreach hash<auto> acl in (resp.body.elements ?? ()) {
            *string org_urn = acl.key.organization;
            if (!org_urn.val()) {
                continue;
            }
            string id = LinkedInOrganizationsUtil::urnId(org_urn);
            if (seen{id}) {
                continue;
            }
            seen{id} = True;
            push ids, id;
        }
        if (!ids) {
            return ();
        }
        return resolveOrganizations(ids);
    }

    #! Resolves organization IDs to normalized organization hashes
    /** @param ids the numeric organization IDs

        @return the normalized organizations, in the order the IDs were given

        @note the Organizations API is a \c BATCH_GET keyed by the **numeric ID**, not by the URN,
        which is why the IDs are passed through unchanged rather than rebuilt into URNs
    */
    private list<auto> resolveOrganizations(list<string> ids) {
        hash<auto> results = {};
        foreach list<auto> chunk in (LinkedInOrganizationsUtil::chunk(ids, MaxBatchSize)) {
            hash<auto> resp = doDmaRequest("GET", OrganizationsPath + "?ids=List("
                + join(",", (map encode_url($1, True), chunk)) + ")");
            results += resp.body.results ?? {};
        }
        list<auto> rv = ();
        foreach string id in (ids) {
            if (*hash<auto> org = results{id}) {
                push rv, LinkedInOrganizationsUtil::normalizeOrganization(org);
            }
        }
        return rv;
    }

    #! Runs a cursor-paged DMA finder and returns the entity URNs it names, with the next cursor
    /** The DMA feed and follows finders return a page of results plus an opaque cursor that
        resumes exactly where the page stopped, which is what makes a listing — and the \c new-post
        trigger — able to continue rather than re-read the head of the list and reconcile against
        IDs it has already seen.

        @param path the finder's path
        @param query the finder's query parameters, without \c paginationCursor
        @param count the maximum number of entries to request
        @param cursor the cursor from a previous call, if any

        @return a hash of \c elements (the raw finder elements) and \c cursor (the next cursor, or
        @ref nothing at the end of the collection)
    */
    private hash<auto> doCursorFinder(string path, hash<auto> query, int count, *string cursor) {
        hash<auto!> q = query + {
            "maxPaginationCount": min(max(count, 1), MaxPaginationCount),
        };
        if (cursor.val()) {
            q.paginationCursor = cursor;
        }
        hash<auto> resp = doDmaRequest("GET", path, q);
        return {
            "elements": resp.body.elements ?? (),
            "cursor": LinkedInOrganizationsUtil::nextCursor(resp.body),
        };
    }

    #! Resolves a list of URNs through a Rest.li \c BATCH_GET and returns the results hash
    /** @param path the resource path
        @param urns the URNs to resolve
        @param query any additional query parameters

        @return the merged \c results hash, keyed by URN

        @note the request is split into batches: a composite comment or reaction URN is long, and a
        URL has a finite length
    */
    private hash<auto> batchGet(string path, list<auto> urns, *hash<auto> query) {
        hash<auto> results = {};
        foreach list<auto> chunk in (LinkedInOrganizationsUtil::chunk(urns, MaxBatchSize)) {
            string full_path = sprintf("%s?ids=%s", path, LinkedInRestClient::linkedin_urn_list(chunk));
            full_path = LinkedInOrganizationsDataProviderBase::addQuery(full_path, query);
            hash<auto> resp = doDmaRequest("GET", full_path);
            results += resp.body.results ?? {};
        }
        return results;
    }

    #! Makes a LinkedIn DMA API call and returns the response
    /** @param method the HTTP method
        @param path the URI path, with any URN already encoded
        @param query optional query parameters, appended url-encoded
        @param body the request body

        @return the REST response
    */
    private hash<auto> doDmaRequest(string method, string path, *hash<auto> query, auto body) {
        string full_path = LinkedInOrganizationsDataProviderBase::addQuery(path, query);
        int retries = 0;
        while (True) {
            try {
                debug("LinkedIn DMA %s %y", method, full_path);
                return rest.restDoRequest(method, full_path, body, rest.getApiHeaders(full_path));
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, full_path), ex.arg;
            }
        }
    }

    #! Appends url-encoded query parameters to a path
    /** Rest.li structural values — \c "(value:APPROVED)", \c "List(a,b)", \c "(end:1,start:0)" —
        must reach LinkedIn with their parentheses, colons and commas **literal**; encoding them
        addresses a different query and LinkedIn rejects it.  Those values are recognized and passed
        through, and everything else is encoded strictly.

        @param path the URI path
        @param query the query parameters; @ref nothing and empty hashes return \a path unchanged

        @return the path with the query string appended
    */
    static string addQuery(string path, *hash<auto> query) {
        if (!query) {
            return path;
        }
        list<string> parts = ();
        foreach hash<auto> i in (query.pairIterator()) {
            if (!exists i.value) {
                continue;
            }
            string value = i.value.toString();
            push parts, sprintf("%s=%s", encode_url(i.key, True),
                LinkedInOrganizationsUtil::isRestliValue(value) ? value : encode_url(value, True));
        }
        if (!parts) {
            return path;
        }
        return path + (path =~ /\?/ ? "&" : "?") + join("&", parts);
    }

    #! Returns an enriched error description including the LinkedIn API error message if available
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        return sprintf("%s (request path: %y)",
            LinkedInRestClient::LinkedInRestClientBase::getErrorDesc(ex.desc, ex.arg.body),
            ex.arg.request_path ?? path);
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        if (ex.err != "REST-RESPONSE-ERROR") {
            return False;
        }
        int status_code = ex.arg.status_code ?? 0;
        # the DMA finders are rate-limited to one request per minute, so 429 is an ordinary outcome
        # rather than an exceptional one; 5xx are the transient server errors LinkedIn asks callers
        # to retry
        if (status_code != 429 && status_code < 500) {
            return False;
        }
        if (++retries == MaxIoRetries) {
            LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
            return False;
        }
        *string retry_after = ex.arg.headers."retry-after";
        int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
        warn("HTTP %d from LinkedIn; waiting %d second(s) before retry %d", status_code, wait_secs,
            retries);
        sleep(wait_secs);
        return True;
    }
}
}
