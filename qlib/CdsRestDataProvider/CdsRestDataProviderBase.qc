# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CdsRestDataProvider module definition

/** CdsRestDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! The CdsRest data provider class
public class CdsRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options (without app-specific options like "api")
        const BaseConstructorOptions = {
            "cdsrestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("RestClient::AbstractRestClient")),
                "desc": "The `CdsRestClientIo` object to use; a synchronous `CdsRestClient` is also accepted, and "
                    "a `CdsRestClientIo` is created with its options",
            },
            "cdsrestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Sets options to the REST client constructor",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Sets options to the REST client constructor",
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Sets the OAuth2 client ID (required if 'client_secret' provided)",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Sets the OAuth2 client secret (required if 'client_id' provided)",
                "sensitive": True,
            },
            "scope": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Scope",
                "short_desc": "The OAuth2 scope string",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 scope string",
            },
            "tenant": <DataProviderOptionInfo>{
                "display_name": "Tenant",
                "short_desc": "The tenant ID of the CDS server instance",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The tenant ID of the CDS server instance",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the CDS server instance",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the CDS server instance",
                "required": True,
            },
        };

        #! Environment options
        const EnvOptions = {
            "url": "CDS_URL",
            "client_id": "CDS_CLIENT_ID",
            "client_secret": "CDS_CLIENT_SECRET",
            "tenant": "CDS_TENANT",
            "scope": "CDS_SCOPE",
        };

        #! Hash of HTTP Methods
        const HttpMethods = {
            "GET": True,
            "PUT": True,
            "PATCH": True,
            "POST": True,
            "DELETE": True,
        };
    }

    private {
        #! The REST client object for API calls
        CdsRestClient::CdsRestClientIo rest;

        #! The connection object used to create the REST client
        *CdsRestClient::CdsRestConnection conn;
    }

    private:internal {
        #! Characters percent-encoded in request paths, as the synchronous REST client does
        /** RestClientIo sends the request target verbatim, so a space in an OData query, for example, would
            otherwise break the request line
        */
        const PathEncodingMap = {
            " ": "%20",
            "<": "%3C",
            ">": "%3E",
            "\"": "%22",
            "#": "%23",
            "%": "%25",
            "{": "%7B",
            "}": "%7D",
            "|": "%7C",
            "\\": "%5C",
            "^": "%5E",
            "~": "%7E",
            "[": "%5B",
            "]": "%5D",
            "`": "%60",
        };
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    /** @param rest the REST client for API calls

        @since CdsRestDataProvider 2.2
    */
    constructor(CdsRestClient::CdsRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object
    /** @param rest a synchronous REST client; API calls are made with a
        @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo" created from it with @ref getAsyncClient()
    */
    constructor(CdsRestClient::CdsRestClient rest) {
        self.rest = CdsRestDataProviderBase::getAsyncClient(rest);
    }

    #! Creates the object from a CDS connection
    /** API calls are made with the asynchronous REST client returned by the connection's \c getAsync() method
    */
    constructor(CdsRestClient::CdsRestConnection conn) {
        self.conn = conn;
        self.rest = conn.getAsync();
    }

    #! Creates the object from a Business Central connection
    /** API calls are made with the asynchronous REST client returned by the connection's \c getAsync() method
    */
    constructor(CdsRestClient::BusinessCentralRestConnection conn) {
        self.conn = conn;
        self.rest = conn.getAsync();
    }

    #! Returns the REST client for API calls
    /** @return the REST client for API calls

        @since CdsRestDataProvider 2.2
    */
    CdsRestClient::CdsRestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns an asynchronous REST client created with the options and the connection path of a synchronous one
    /** @param rest the synchronous REST client

        @return an asynchronous REST client of the class matching the API profile of \a rest
        (@ref CdsRestClient::BusinessCentralRestClientIo "BusinessCentralRestClientIo",
        @ref CdsRestClient::DynamicsRestClientIo "DynamicsRestClientIo", or
        @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo" if no API profile is set), created with the
        options and the connection path of \a rest; if \a rest already has an authentication token, it is used
        by the new client as well

        @throw CDSRESTCLIENT-ERROR missing or invalid required option for REST authentication or communication

        @since CdsRestDataProvider 2.2
    */
    static CdsRestClient::CdsRestClientIo getAsyncClient(CdsRestClient::CdsRestClient rest) {
        hash<auto> opts = rest.getConstructorOptions();
        # use the current token, if any, so that the new client does not have to log in again
        *string token = rest.getToken();
        if (token) {
            opts.token = token;
        }
        CdsRestClient::CdsRestClientIo rv;
        switch (rest.getApiProfile()) {
            case CdsRestClient::BusinessCentralProfile.name:
                rv = new CdsRestClient::BusinessCentralRestClientIo(opts);
                break;
            case CdsRestClient::DynamicsProfile.name:
                rv = new CdsRestClient::DynamicsRestClientIo(opts);
                break;
            default:
                rv = new CdsRestClient::CdsRestClientIo(opts);
                break;
        }
        rv.setConnectionPath(rest.getConnectionPath());
        return rv;
    }

    #! Returns the request path for the given URL
    /** OData services return links to further pages of results (\c \@odata.nextLink) as absolute URLs, but
        @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo" requests take a path; the path of such a link
        includes the connection path, which is therefore not prepended to it again

        @param url an absolute URL or a request path; the query of the URL is kept

        @return the path of \a url followed by its query, if any; characters not valid in a request target are
        percent-encoded, while percent-encoded characters are left unchanged

        @par Example:
        @code{.py}
string path = CdsRestDataProviderBase::getRequestPath(
    "https://org.crm.dynamics.com/api/data/v9.2/accounts?$skiptoken=%3Ccookie%20pagenumber=%222%22%20/%3E");
# path: "/api/data/v9.2/accounts?$skiptoken=%3Ccookie%20pagenumber=%222%22%20/%3E"
        @endcode

        @throw PARSE-URL-ERROR the URL cannot be parsed

        @since CdsRestDataProvider 2.2
    */
    static string getRequestPath(string url) {
        if (url =~ /^[a-z][a-z0-9+.-]*:\/\//i) {
            hash<UrlInfo> info = parse_url(url);
            # the query is not separated from the path by parse_url(); a fragment is not part of a request target
            string path = info.path ?? "/";
            path =~ s/#.*$//;
            if (path !~ /^\//) {
                path = "/" + path;
            }
            url = path;
        }
        return CdsRestDataProviderBase::encodeRequestPathIntern(url, False);
    }

    #! Returns the given request path with characters not valid in a request target percent-encoded
    /** The characters are encoded as the synchronous @ref CdsRestClient::CdsRestClient "CdsRestClient" encodes
        them, because @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo" sends request paths verbatim

        @param path the request path, which may include a query

        @return the path with spaces, control characters, non-ASCII characters, angle brackets, double quotes,
        hash signs, percent signs, braces, vertical bars, backslashes, carets, tildes, square brackets, and grave
        accents percent-encoded

        @par Example:
        @code{.py}
string path = CdsRestDataProviderBase::encodeRequestPath("accounts?$filter=name eq 'x'");
# path: "accounts?$filter=name%20eq%20'x'"
        @endcode

        @since CdsRestDataProvider 2.2
    */
    static string encodeRequestPath(string path) {
        return CdsRestDataProviderBase::encodeRequestPathIntern(path, True);
    }

    #! Returns the CDS REST client for the \c cdsrestclient option
    /** @param rest the value of the option

        @return \a rest if it is a @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo", or a client created
        from it with @ref getAsyncClient() if it is a @ref CdsRestClient::CdsRestClient "CdsRestClient"

        @throw CONSTRUCTOR-ERROR \a rest is neither a \c CdsRestClientIo nor a \c CdsRestClient object
    */
    static private CdsRestClient::CdsRestClientIo getClientFromOption(RestClient::AbstractRestClient rest) {
        if (rest instanceof CdsRestClient::CdsRestClientIo) {
            return cast<CdsRestClient::CdsRestClientIo>(rest);
        }
        if (rest instanceof CdsRestClient::CdsRestClient) {
            return CdsRestDataProviderBase::getAsyncClient(cast<CdsRestClient::CdsRestClient>(rest));
        }
        throw "CONSTRUCTOR-ERROR", sprintf("the 'cdsrestclient' option must be a CdsRestClientIo or a "
            "CdsRestClient object; got class %y", rest.className());
    }

    #! Percent-encodes characters not valid in a request target
    /** @param path the path to encode
        @param encode_pct if @ref True, \c "%" is encoded as well; otherwise the path is assumed to contain
        percent-encoded characters, which are left unchanged
    */
    static private string encodeRequestPathIntern(string path, bool encode_pct) {
        # fast path: nothing to encode
        if (path !~ /[^!$&-;=?-Z_a-z]/) {
            return path;
        }
        # the request target is encoded byte by byte from the UTF-8 representation of the path
        binary b = path.encoding() == "UTF-8" ? path.toBinary() : convert_encoding(path, "UTF-8").toBinary();
        string rv;
        int size = b.size();
        for (int i = 0; i < size; ++i) {
            int code = b[i];
            if (code < 0x20 || code >= 0x7f) {
                rv += sprintf("%%%02X", code);
                continue;
            }
            string c = chr(code);
            *string enc = (c != "%" || encode_pct) ? PathEncodingMap{c} : NOTHING;
            rv += enc ?? c;
        }
        return rv;
    }

    #! Returns options for the CdsRestClientIo
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("cdsrestclient_options", "cdsrestclient")
            + copts.cdsrestclient_options;
    }
}
}
