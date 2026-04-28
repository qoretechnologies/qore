# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! The Tally data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class TallyDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Tally API Key",
                "short_desc": "The API key required to communicate with Tally servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Tally API key used for authentication; sent as the `Authorization: Bearer` header",
                "sensitive": True,
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
                "desc": "Options to the RestClientIo constructor; only used if a RestClientIo object is created for "
                    "a call",
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
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }

        if (!rest) {
            if (!copts.apikey) {
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Tally API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "apikey": copts.apikey,
            };
            rest = new TallyRestClient::TallyRestClientIo(opts);
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

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "forms": True,
            "workspaces": True,
            "webhooks": True,
            "submissions": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "forms": return getReferenceForms();
            case "workspaces": return getReferenceWorkspaces();
            case "webhooks": return getReferenceWebhooks();
            case "submissions": return getReferenceSubmissions(action_opts.form_id);
        }
    }

    #! Returns available forms for reference data (dropdown selections)
    private *list<hash<AllowedValueInfo>> getReferenceForms() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doRestCommand("GET", "forms?limit=500");
        if (response.body.items) {
            foreach hash<auto> form in (response.body.items) {
                string display_name = form.name ?? sprintf("Form %s", form.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": display_name,
                    "desc": sprintf("%s (ID: %s, Status: %s)", display_name, form.id,
                        form.status ?? "unknown"),
                    "value": string(form.id),
                };
            }
        }
        return rv;
    }

    #! Returns available workspaces for reference data (dropdown selections)
    private *list<hash<AllowedValueInfo>> getReferenceWorkspaces() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doRestCommand("GET", "workspaces");
        if (response.body.items) {
            foreach hash<auto> ws in (response.body.items) {
                string display_name = ws.name ?? sprintf("Workspace %s", ws.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": display_name,
                    "desc": sprintf("%s (ID: %s)", display_name, ws.id),
                    "value": string(ws.id),
                };
            }
        }
        return rv;
    }

    #! Returns available webhooks for reference data (dropdown selections)
    private *list<hash<AllowedValueInfo>> getReferenceWebhooks() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doRestCommand("GET", "webhooks?limit=500");
        if (response.body.webhooks) {
            foreach hash<auto> wh in (response.body.webhooks) {
                string display_name = wh.url ?? sprintf("Webhook %s", wh.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": display_name,
                    "desc": sprintf("%s (ID: %s, Form: %s, Enabled: %y)", display_name, wh.id,
                        wh.formId ?? "unknown", wh.isEnabled),
                    "value": string(wh.id),
                };
            }
        }
        return rv;
    }

    #! Returns available submissions for the given form (cascading reference data)
    /** Returns NOTHING when no form_id is set so the dropdown stays empty until the user
        selects a form.
    */
    private *list<hash<AllowedValueInfo>> getReferenceSubmissions(*string form_id) {
        if (!form_id.val()) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doRestCommand("GET", "forms/" + form_id + "/submissions?limit=500");
        if (response.body.submissions) {
            foreach hash<auto> submission in (response.body.submissions) {
                string display_name = submission.submittedAt
                    ? sprintf("Submission %s (%s)", submission.id, submission.submittedAt)
                    : sprintf("Submission %s", submission.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": display_name,
                    "desc": sprintf("%s (ID: %s, Completed: %y)", display_name, submission.id,
                        submission.isCompleted),
                    "value": string(submission.id),
                };
            }
        }
        return rv;
    }

    #! Makes a REST call and returns the response; handles rate limit and I/O errors
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
                switch (method.upr()) {
                    case "GET":
                        return rest.restGet(path, hdr);
                    case "POST":
                        return rest.restPost(path, body, hdr);
                    case "PUT":
                        return rest.restPut(path, body, hdr);
                    case "PATCH":
                        return rest.restPatch(path, body, hdr);
                    case "DELETE":
                        return rest.restDel(path, body, hdr);
                    default:
                        return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, path), ex.arg;
            }
        }
    }

    #! Returns an enriched error description that includes the Tally API error message if available
    /** Tally runs on NestJS and uses its standard exception envelope:
        \c {"statusCode": <int>, "message": "<string>" | [<strings or {property, message} hashes>],
        "error": "Bad Request" | "Unauthorized" | ...}. The \c message field is polymorphic:
        scalar on auth/simple errors, list of strings from \c ValidationPipe, or list of
        \c {property, message} hashes from custom class-validator configurations.

        Tally also returns bare plain-text bodies (e.g. \c "Unauthorized") on auth failures; those
        surface as-is.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        switch (body.typeCode()) {
            case NT_STRING:
                if (body.val()) {
                    desc += ": " + body;
                }
                break;
            case NT_HASH: {
                *string err_type = DataProvider::DataProviderErrorUtil::firstStringAt(body, ("error",));
                auto message = body.message;
                string msg_str;
                if (message.typeCode() == NT_STRING && message.val()) {
                    msg_str = message;
                } else if (message.typeCode() == NT_LIST && message) {
                    list<string> parts = ();
                    foreach auto m in (message) {
                        if (m.typeCode() == NT_STRING) {
                            push parts, m;
                        } else if (m.typeCode() == NT_HASH) {
                            *string prop = DataProvider::DataProviderErrorUtil::firstStringAt(m, ("property",));
                            *string mmsg = DataProvider::DataProviderErrorUtil::firstStringAt(m, ("message",));
                            string p = prop ?? "";
                            if (mmsg.val()) {
                                p = p.val() ? p + ": " + mmsg : mmsg;
                            }
                            if (p.val()) {
                                push parts, p;
                            }
                        } else {
                            push parts, sprintf("%y", m);
                        }
                    }
                    if (parts) {
                        msg_str = parts.size() == 1 ? parts[0] : "[" + join("; ", parts) + "]";
                    }
                }
                if (msg_str.val()) {
                    desc += ": " + msg_str;
                } else if (err_type.val()) {
                    desc += ": " + err_type;
                }
                break;
            }
        }
        return sprintf("%s (request path: %y)", desc, ex.arg.request_path ?? path);
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            # use retry-after header if available
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            warn("Rate limited (429); waiting %d second(s) before retry %d", wait_secs, retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }
}
}
