# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TavilyDataProviderCommon class definition

/** TavilyDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TavilyDataProvider {
public class TavilyDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultTavilyUrl = "https://api.tavily.com";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Tavily API Key",
                "short_desc": "Tavily API key for Bearer authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Tavily API key.  Get one from the "
                    "[Tavily dashboard](https://app.tavily.com/).",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the Tavily REST API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the Tavily REST API.",
                "default_value": DefaultTavilyUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the RestClientIo constructor.",
            },
        };

        #! The reference data name for the research tasks this account has started
        const RefResearchTask = "tavily-research-tasks";

        #! How many research tasks the dropdown offers
        const ResearchTaskListLimit = 100;

        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };
        const MaxIoRetries = 5;
    }

    private {
        *RestClientIo::RestClientIo rest;
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Tavily API communication";
            }
            hash<auto> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url,
                "apikey": copts.apikey,
            };
            rest = new TavilyRestClient::TavilyRestClientIo(opts);
        }
    }

    private hash<auto> doRestCommand(string method, string path, auto body,
            *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
                switch (method.upr()) {
                    case "GET":
                        return rest.restGet(path, hdr);
                    case "POST":
                        return rest.restPost(path, body, hdr);
                    case "DELETE":
                        return rest.restDel(path, body, hdr);
                    default:
                        return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (request path: %y)", ex.desc,
                    ex.arg.request_path ?? path), ex.arg;
            }
        }
    }

    #! Returns the reference data this application can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            RefResearchTask: True,
        };
    }

    #! Populates a dropdown from the usage log
    /** @param type the reference data name
        @param action_opts the options of the action being filled in

        @return the values for the dropdown, or nothing when there are none
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case RefResearchTask: return getResearchTaskValues();
        }
    }

    #! Returns the research tasks this account has started, most recent first
    /** Tavily publishes no endpoint that lists research tasks, but every request it bills appears in
        the usage log with its \c request_id - which for a research task is the very identifier
        @ref TavilyGetResearchDataProvider "get-research" needs.  Narrowing the log to the research
        endpoint therefore gives the listing the API does not otherwise offer.

        @note the log is populated a few minutes after the request is made, so a task started moments
        ago is not in it yet; the value returned by \c research is authoritative in the meantime
    */
    private *list<hash<AllowedValueInfo>> getResearchTaskValues() {
        hash<auto> body = doRestCommand("POST", "logs", {
            "endpoints": ("research",),
            "limit": ResearchTaskListLimit,
        }).body;
        list<hash<AllowedValueInfo>> rv = ();
        foreach hash<auto> entry in (body.logs ?? ()) {
            if (!entry.request_id.val()) {
                continue;
            }
            rv += <AllowedValueInfo>{
                "value": entry.request_id,
                # the log carries no task text, so the time it ran and the model that ran it are all
                # there is to tell one task from another
                "display_name": entry.depth.val()
                    ? sprintf("%s (%s)", entry.timestamp ?? entry.request_id, entry.depth)
                    : (entry.timestamp ?? entry.request_id),
                "desc": sprintf("Research task `%s`", entry.request_id),
            };
        }
        return rv ?: NOTHING;
    }

    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (DataProviderRetryUtil::isRetriableTransportError(ex, RetrySet)) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying",
                ex.err, ex.desc);
            return True;
        }
        return False;
    }
}
}
