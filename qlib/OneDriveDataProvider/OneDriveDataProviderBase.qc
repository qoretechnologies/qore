# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! The OneDrive data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class OneDriveDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID for Microsoft Graph API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 client ID from your Azure app registration",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret for Microsoft Graph API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 client secret from your Azure app registration",
                "sensitive": True,
            },
            "tenant": <DataProviderOptionInfo>{
                "display_name": "Tenant ID",
                "short_desc": "Azure AD tenant ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Azure AD tenant ID; use `common` for multi-tenant apps (default), or a specific "
                    "tenant ID",
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
                "desc": "Options to the RestClientIo constructor; only used if a RestClientIo object is "
                    "created for a call",
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
            if (!copts.oauth2_client_id || !copts.oauth2_client_secret) {
                throw "CONSTRUCTOR-ERROR", "missing 'oauth2_client_id' and 'oauth2_client_secret' arguments "
                    "for OneDrive API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "oauth2_client_id": copts.oauth2_client_id,
                "oauth2_client_secret": copts.oauth2_client_secret,
            };
            if (copts.tenant) {
                opts.tenant = copts.tenant;
            }
            rest = new OneDriveRestClient::OneDriveRestClientIo(opts);
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

    #! Returns the reference data types supported by this provider
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "folders": True,
            "drives": True,
            "items": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "folders": return getReferenceFolders();
            case "drives": return getReferenceDrives();
            case "items": return getReferenceItems();
        }
    }

    #! Returns reference data for folders (lists folders from root)
    private *list<hash<AllowedValueInfo>> getReferenceFolders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string path = "me/drive/root/children?$top=200&$filter=folder%20ne%20null"
                "&$select=id,name,parentReference";
            hash<auto> response = doRestCommand("GET", path);
            foreach hash<auto> item in (response.body.value) {
                rv += <AllowedValueInfo>{
                    "display_name": item.name,
                    "short_desc": item.name,
                    "desc": item.name,
                    "value": string(item.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceFolders: error: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for items (files and folders from root)
    private *list<hash<AllowedValueInfo>> getReferenceItems() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string path = "me/drive/root/children?$top=200&$select=id,name,folder";
            hash<auto> response = doRestCommand("GET", path);
            foreach hash<auto> item in (response.body.value) {
                string type_label = item.folder ? "folder" : "file";
                rv += <AllowedValueInfo>{
                    "display_name": item.name,
                    "short_desc": sprintf("%s (%s)", item.name, type_label),
                    "desc": sprintf("%s (%s)", item.name, type_label),
                    "value": string(item.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceItems: error: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for drives
    private *list<hash<AllowedValueInfo>> getReferenceDrives() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doRestCommand("GET", "me/drives?$select=id,name,driveType");
            foreach hash<auto> drive in (response.body.value) {
                string display = drive.name ?? drive.driveType ?? drive.id;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": sprintf("%s (%s)", display, drive.driveType ?? "unknown"),
                    "desc": sprintf("%s (%s)", display, drive.driveType ?? "unknown"),
                    "value": string(drive.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceDrives: error: %s: %s", ex.err, ex.desc);
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

    #! Returns an enriched error description that includes the Microsoft Graph API error message if available
    /** OneDrive is served via Microsoft Graph, which uses the OData-style error envelope:
        \c {"error": {"code": "...", "message": "...", "innerError": {"code": "...", ...},
        "details": [{...}, ...]}}. The \c message is the developer-facing prose and \c code is a
        machine-readable identifier (e.g. \c itemNotFound, \c InvalidAuthenticationToken).
        \c innerError is optional and may carry a more specific nested code plus request metadata
        (documented as lowercase \c innererror in the resource spec but emitted as \c innerError in
        practice — both casings are probed). \c details[] is populated for batch/multi-operation
        failures and contains error objects with the same shape.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        if (body.typeCode() == NT_HASH && body.error.typeCode() == NT_HASH) {
            auto err = body.error;
            *string message = DataProvider::DataProviderErrorUtil::firstStringAt(err, ("message",));
            *string code = DataProvider::DataProviderErrorUtil::firstStringAt(err, ("code",));
            *string inner_code = DataProvider::DataProviderErrorUtil::firstStringAt(
                err, ("innerError.code", "innererror.code"));

            string primary;
            if (message.val() && code.val()) {
                primary = sprintf("%s (code: %s)", message, code);
            } else if (message.val()) {
                primary = message;
            } else if (code.val()) {
                primary = code;
            }
            if (inner_code.val() && inner_code != code) {
                primary = primary.val()
                    ? primary + sprintf(" [inner: %s]", inner_code)
                    : sprintf("[inner: %s]", inner_code);
            }
            if (primary.val()) {
                desc += ": " + primary;
            }

            # details[] is populated for batch operations (each entry same shape as top-level error)
            if (err.details.typeCode() == NT_LIST && err.details) {
                list<string> parts = ();
                foreach auto d in (err.details) {
                    if (d.typeCode() != NT_HASH) {
                        continue;
                    }
                    *string dm = DataProvider::DataProviderErrorUtil::firstStringAt(d, ("message",));
                    *string dc = DataProvider::DataProviderErrorUtil::firstStringAt(d, ("code",));
                    string p;
                    if (dm.val() && dc.val()) {
                        p = sprintf("%s (code: %s)", dm, dc);
                    } else if (dm.val()) {
                        p = dm;
                    } else if (dc.val()) {
                        p = dc;
                    }
                    if (p.val()) {
                        push parts, p;
                    }
                }
                if (parts) {
                    desc += " [" + join("; ", parts) + "]";
                }
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
