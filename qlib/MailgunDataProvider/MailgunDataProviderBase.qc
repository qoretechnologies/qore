# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Base class for Mailgun data providers
public class MailgunDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Mailgun data providers
        const BaseConstructorOptions = {
            "mailgunrestclient": <DataProviderOptionInfo>{
                "display_name": "Mailgun REST Client",
                "short_desc": "The MailgunRestClient object",
                "type": AbstractDataProviderType::get(new Type("MailgunRestClient::MailgunRestClient")),
                "desc": "The MailgunRestClient object to use for API calls",
            },
            "mailgunrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Mailgun REST Client Options",
                "short_desc": "Options for creating a MailgunRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a MailgunRestClient object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Mailgun API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Mailgun API key for authentication",
                "sensitive": True,
                "required": True,
            },
            "api_region": <DataProviderOptionInfo>{
                "display_name": "API Region",
                "short_desc": "Mailgun API region",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Mailgun API region (US or EU)",
                "required": True,
                "default_value": MailgunRestClient::MailgunRestClient::DefaultApiRegion,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "US",
                        "display_name": "United States",
                        "desc": "US API endpoint - https://api.mailgun.net/v3",
                    },
                    <AllowedValueInfo>{
                        "value": "EU",
                        "display_name": "European Union",
                        "desc": "EU API endpoint - https://api.eu.mailgun.net/v3",
                    },
                ),
            },
            "api_domain": <DataProviderOptionInfo>{
                "display_name": "Sending Domain",
                "short_desc": "Mailgun sending domain",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Mailgun sending domain to use for email operations (e.g., mg.example.com)",
            },
        };
    }

    private {
        #! The REST client object for API calls
        MailgunRestClient::MailgunRestClient rest;

        #! The connection object, if applicable
        *MailgunRestClient::MailgunRestConnection conn;

        #! The default domain for API calls
        *string api_domain;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
        self.api_domain = api_domain;
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
        # Get api_domain from connection options
        api_domain = conn.getConnectionOptions().api_domain;
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.mailgunrestclient) {
            rest = copts.mailgunrestclient;
        } else {
            hash<auto> client_opts = copts.mailgunrestclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            if (copts.api_region) {
                client_opts.api_region = copts.api_region;
            }
            if (copts.api_domain) {
                client_opts.api_domain = copts.api_domain;
            }
            rest = new MailgunRestClient::MailgunRestClient(client_opts);
        }
        api_domain = copts.api_domain;
    }

    #! Returns the REST client
    MailgunRestClient::MailgunRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *MailgunRestClient::MailgunRestConnection getConnection() {
        return conn;
    }

    #! Returns the configured API domain
    *string getApiDomain() {
        return api_domain;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Mailgun API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doGet: GET %s with params: %y", path, params));
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doGet: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Mailgun API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPost: POST %s with body: %y", path, body));
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPost: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a form-encoded POST request to the Mailgun API
    /** Mailgun uses form-encoded POST for most operations.

        @param path the API path
        @param body the request body hash
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doPostFormEncoded(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: POST %s with body: %y", path, body));

        # Build form-encoded body string
        string encoded_body = "";
        if (body) {
            list<string> parts;
            foreach string key in (keys body) {
                auto val = body{key};
                if (exists val) {
                    if (val.typeCode() == NT_LIST) {
                        # Handle list values (e.g., multiple 'to' addresses)
                        foreach auto item in (val) {
                            parts += sprintf("%s=%s", encode_url(key, True), encode_url(string(item), True));
                        }
                    } else {
                        parts += sprintf("%s=%s", encode_url(key, True), encode_url(string(val), True));
                    }
                }
            }
            encoded_body = parts.join("&");
        }

        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: encoded body: %s", encoded_body));

        # Set Content-Type header and send as raw text
        hash<auto> headers = {
            "Content-Type": "application/x-www-form-urlencoded",
        };

        # Save current serialization and switch to "text" to send raw body
        string old_serialization = rest.getSerialization();
        rest.setSerialization("text");
        on_exit rest.setSerialization(old_serialization);

        hash<auto> response = rest.post(path, encoded_body, NOTHING, headers);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a multipart form POST request to the Mailgun API
    /** Used for sending emails with potential attachments.

        @param path the API path
        @param body the request body hash
        @param attachments optional list of attachment hashes with 'filename', 'content', and optional 'content_type'
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doPostMultipart(string path, hash<auto> body, *list<hash<auto>> attachments) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPostMultipart: POST %s with body: %y, attachments: %d",
            path, body, attachments ? attachments.size() : 0));

        # Build multipart message
        MultiPartMessage msg();

        # Add form fields
        foreach string key in (keys body) {
            auto val = body{key};
            if (exists val) {
                if (val.typeCode() == NT_LIST) {
                    # Handle list values (e.g., multiple 'to' addresses)
                    foreach auto item in (val) {
                        msg.addPart(new MultiPartSimple(key, string(item)));
                    }
                } else {
                    msg.addPart(new MultiPartSimple(key, string(val)));
                }
            }
        }

        # Add attachments if provided
        if (attachments) {
            foreach hash<auto> attachment in (attachments) {
                msg.addPart(new MultiPartFile("attachment",
                    attachment.filename,
                    attachment.content,
                    attachment.content_type ?? "application/octet-stream"));
            }
        }

        # Send multipart request
        hash<auto> info;
        hash<auto> response = rest.doRequest("POST", path, msg.getBody(), \info, msg.getContentType());
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPostMultipart: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a PUT request to the Mailgun API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPut: PUT %s with body: %y", path, body));
        hash<auto> response = rest.put(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPut: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a DELETE request to the Mailgun API
    /** @param path the API path
        @return the response data

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doDelete: DELETE %s", path));
        hash<auto> response = rest.del(path);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doDelete: success"));
        return response.body;
    }

    #! Checks the Mailgun API response for errors
    /** @param response the API response hash

        @throw MAILGUN-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Mailgun API returns error in message field on failure
        if (response.status_code >= 400) {
            string err_msg = response.body.message ?? "Unknown Mailgun API error";
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Mailgun API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "MAILGUN-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.apikey) {
            rv.apikey = copts.apikey;
        }
        if (copts.api_region) {
            rv.api_region = copts.api_region;
        }
        if (copts.api_domain) {
            rv.api_domain = copts.api_domain;
        }
        foreach string key in (keys copts.mailgunrestclient_options) {
            rv{key} = copts.mailgunrestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "domains": True,
            "mailing_lists": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "domains": return getReferenceDomains();
            case "mailing_lists": return getReferenceMailingLists();
        }
    }

    #! Returns available domains for reference data
    private *list<hash<AllowedValueInfo>> getReferenceDomains() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("/domains", {"limit": DefaultPageSize});
            if (response.items) {
                foreach hash<auto> domain in (response.items) {
                    string display_name = domain.name;
                    string desc = sprintf("%s (State: %s)", domain.name, domain.state ?? "unknown");
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": domain.state ?? "active",
                        "desc": desc,
                        "value": domain.name,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceDomains: error fetching domains: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available mailing lists for reference data
    private *list<hash<AllowedValueInfo>> getReferenceMailingLists() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("/lists/pages", {"limit": DefaultPageSize});
            if (response.items) {
                foreach hash<auto> list in (response.items) {
                    string display_name = list.name ?? list.address;
                    string desc = sprintf("%s (%s)", display_name, list.address);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": list.description ?? list.address,
                        "desc": desc,
                        "value": list.address,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceMailingLists: error fetching mailing lists: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace MailgunDataProvider
