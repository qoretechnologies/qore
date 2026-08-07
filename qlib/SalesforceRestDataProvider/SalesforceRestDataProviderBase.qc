# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! The SalesforceRest data provider class
public class SalesforceRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The Salesforce API version to use",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Salesforce API version to use (`auto` = latest version available)",
            },
            "oauth2_auth_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Auth URL",
                "short_desc": "The OAuth2 auth URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for logging in, overrides the default value",
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the Salesforce consumer key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce consumer key / OAuth2 client ID",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the Salesforce consumer secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com consumer secret / OAuth2 client secret",
                "sensitive": True,
            },
            "oauth2_token_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Token URL",
                "short_desc": "The OAuth2 token URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for token acquisition, overrides the default value, used when logging in",
            },
            "oauth2_logout_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Revoke URL",
                "short_desc": "The OAuth2 token revoke URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for token revocation, overrides the default value, used when logging out",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com account password",
                "sensitive": True,
            },
            "salesforcerestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("SalesforceRestClientIo")),
                "desc": "The `SalesforceRestClientIo` object to use",
            },
            "salesforcerestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options to the `SalesforceRestClientIo` constructor; only used if a "
                    "`SalesforceRestClientIo` object is created for a call",
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com account username",
            },
        };

        #! Environment options
        const EnvOptions = {
            "client_id": "SALESFORCE_CONSUMER_KEY",
            "client_secret": "SALESFORCE_CONSUMER_SECRET",
            "username": "SALESFORCE_USER",
            "password": "SALESFORCE_PASS",
        };

        #! Hash of HTTP Methods
        const HttpMethods = {
            "GET": True,
            "PUT": True,
            "PATCH": True,
            "POST": True,
            "DELETE": True,
        };

        #! I/O errors that are retried
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Maximum number of retries for a retryable error
        const MaxIoRetries = 5;
    }

    private {
        #! The connection object used to create the REST client
        *SalesforceRestClient::SalesforceRestConnection conn;

        #! The REST client object, as supplied; targets the org host
        /** This is the **async** client, so every Salesforce REST action runs on the async socket
            I/O controller rather than blocking a thread per request.

            REST calls are not made on this client directly but on @ref getApiRest(), which is
            rooted at the org's *versioned* API base.
        */
        *RestClientIo::RestClientIo rest;

        #! The client rooted at the org's versioned REST API base; resolved on first use
        /** Resolving the version costs a \c "GET /services/data" round trip, so it is deferred to
            the first REST call rather than performed in the constructor: building a data provider
            tree -- which the UI does merely to list children -- must not perform I/O, and a
            provider built from a client that never reaches a server must still be constructible.

            This is memoization only, and is deliberately **not** lock-protected.  A data provider
            is constructed in the context of whichever module reached it, and a child module runs
            under restricted parse options that forbid constructing thread classes -- a \c Mutex
            member here makes the \c pubsub child provider unresolvable with
            <tt>INVALID-FUNCTION-ACCESS</tt>.  No lock is needed regardless: the deduplication
            that matters happens in
            @ref SalesforceRestClient::SalesforceRestClientIo::getApiClient() "getApiClient()",
            which caches on the client shared by the whole provider tree, so the worst a race here
            can do is call that method twice and store the same object it returns both times.
        */
        *RestClientIo::RestClientIo api_rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object from an async Salesforce REST client
    constructor(SalesforceRestClient::SalesforceRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from a connection
    constructor(SalesforceRestClient::SalesforceRestConnection conn) {
        self.conn = conn;
        self.rest = cast<SalesforceRestClient::SalesforceRestClientIo>(conn.getAsync());
    }

    #! Returns the REST client backing the framework-injected generic API call child
    /** The generic \c make-api-call action addresses paths like \c "sobjects/Account/describe",
        which are relative to the org's *versioned* REST API base, so it must be given
        @ref getApiRest() and not the \c rest member, which targets the org host and would turn
        every such path into a request for the org's web UI.

        @return the versioned API client
    */
    private *object getRestClientForGenericCallImpl() {
        return getApiRest();
    }

    #! Returns the client rooted at the org's versioned REST API base, resolving it on first use
    /** @return the versioned API client

        @see @ref SalesforceRestClient::SalesforceRestClientIo::getApiClient() "getApiClient()"
    */
    private RestClientIo::RestClientIo getApiRest() {
        if (!api_rest) {
            api_rest = cast<SalesforceRestClient::SalesforceRestClientIo>(rest).getApiClient();
        }
        return api_rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Makes a REST call and returns the response; handles rate limit and I/O errors
    /** @param method the HTTP method
        @param path the URI path, relative to the org's versioned REST API base
        @param body the message body, if any
        @param hdr any additional headers

        @return the REST response

        @throw REST-RESPONSE-ERROR an HTTP error response was received
    */
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
                RestClientIo::RestClientIo api = getApiRest();
                switch (method.upr()) {
                    case "GET":
                        return api.restGet(path, hdr);
                    case "POST":
                        return api.restPost(path, body, hdr);
                    case "PUT":
                        return api.restPut(path, body, hdr);
                    case "PATCH":
                        return api.restPatch(path, body, hdr);
                    case "DELETE":
                        return api.restDel(path, body, hdr);
                    default:
                        return api.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, path), ex.arg;
            }
        }
    }

    #! Returns an enriched error description including the Salesforce API error message if available
    /** Salesforce returns errors as a list of \c {errorCode, message, fields} objects, for example
        <tt>[{"message": "Required fields are missing: [Name]", "errorCode":
        "REQUIRED_FIELD_MISSING", "fields": ["Name"]}]</tt>.  Without this the caller sees only
        \c "HTTP 400 response received", which names neither the field nor the reason.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        # a single error object rather than a list is also accepted
        if (body.typeCode() == NT_HASH) {
            body = (body,);
        }
        if (body.typeCode() == NT_LIST) {
            list<string> parts = ();
            foreach auto e in (body) {
                if (e.typeCode() != NT_HASH) {
                    continue;
                }
                *string code = e.errorCode;
                *string message = e.message;
                string p;
                if (message.val() && code.val()) {
                    p = sprintf("%s (code: %s)", message, code);
                } else if (message.val()) {
                    p = message;
                } else if (code.val()) {
                    p = code;
                }
                if (p.val() && e.fields.typeCode() == NT_LIST && e.fields) {
                    p += sprintf(" [fields: %s]", join(", ", e.fields));
                }
                if (p.val()) {
                    push parts, p;
                }
            }
            if (parts) {
                desc += ": " + join("; ", parts);
            }
        }
        return sprintf("%s (request path: %y)", desc, ex.arg.request_path ?? path);
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            warn("Rate limited (429); waiting %d second(s) before retry %d", wait_secs, retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }

    #! Returns options for the SalesforceRestClient
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("salesforcerestclient_options", "salesforcerestclient")
            + copts.salesforcerestclient_options;
    }
}
}
