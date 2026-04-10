# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestClientDataProvider class definition

/** RestClientDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Application name
public const AppName = "GenericRest";

#! The REST client data provider class, provides API classes as children
public class RestClientDataProvider inherits RestClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "RestClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor arguments
        const ConstructorOptions = {
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "The connect timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The connection timeout to use in milliseconds (default: 45 seconds)",
                "default_value": 45000,
            },
            "content_encoding": <DataProviderOptionInfo>{
                "display_name": "Compression",
                "short_desc": "Sets the compression to use when sending messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Sets the send encoding (if the `send_encoding` option is not set) and the requested response "
                    "encoding (note that only outgoing message bodies over `1024` bytes in size are compressed)",
                "default_value": "gzip",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "gzip",
                        "desc": "use GNU zip encoding ([RFC 1952](https://tools.ietf.org/html/rfc1952))",
                    }, <AllowedValueInfo>{
                        "value": "bzip2",
                        "desc": "use bzip2 encoding",
                    }, <AllowedValueInfo>{
                        "value": "deflate",
                        "desc": "use the deflate algorithm ([RFC 1951](https://tools.ietf.org/html/rfc1951))",
                    }, <AllowedValueInfo>{
                        "value": "identity",
                        "desc": "use no content encoding",
                    },
                ),
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "Data Serialization",
                "short_desc": "Determines how message bodies are serialized",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Determines how message bodies are serialized",
                "default_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "auto",
                        "desc": "Prefers in this order: `json`, `yaml`, `rawxml`, `xml`, `url`, and `text`",
                    }, <AllowedValueInfo>{
                        "value": "bin",
                        "desc": "For binary message bodies without data serialization",
                    }, <AllowedValueInfo>{
                        "value": "json",
                        "desc": "Use JSON serialization",
                    }, <AllowedValueInfo>{
                        "value": "rawxml",
                        "desc": "Message bodies are encoded with XML without any data type encodings",
                    }, <AllowedValueInfo>{
                        "value": "text",
                        "desc": "Use only plain text; no serialization is used",
                    }, <AllowedValueInfo>{
                        "value": "url",
                        "desc": "For URL-encoded message bodies "
                            "(see [RFC 2738 2.2](https://tools.ietf.org/html/rfc1738))",
                    }, <AllowedValueInfo>{
                        "value": "xml",
                        "desc": "Use only XML-RPC value serialization",
                    }, <AllowedValueInfo>{
                        "value": "yaml",
                        "desc": "Use only YAML serialization",
                    },
                ),
            },
            "error_passthru": <DataProviderOptionInfo>{
                "display_name": "Error Passthrough",
                "short_desc": "If errors should be returned like normal successful responses",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then REST status codes indicating errors will not cause an "
                    "`REST-CLIENT-RECEIVE-ERROR` exception to be raised, rather, such responses will be "
                    "passed through to the caller like any other response",
                "default_value": False,
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "HTTP Headers",
                "short_desc": "Sets HTTP headers to include in each request",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "Sets HTTP headers to include in each outgoing request",
            },
            "http_version": <DataProviderOptionInfo>{
                "display_name": "HTTP Version",
                "short_desc": "The HTTP version to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The HTTP version to use: `auto` (HTTP/2 if available, default), `1.0`, `1.1`, "
                    "or `2.0`. HTTP/2 requires TLS and is negotiated via ALPN",
                "default_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "auto",
                        "desc": "Use HTTP/2 if available via ALPN, fall back to HTTP/1.1 (default)",
                    },
                    <AllowedValueInfo>{
                        "value": "1.0",
                        "desc": "HTTP/1.0 only",
                    },
                    <AllowedValueInfo>{
                        "value": "1.1",
                        "desc": "HTTP/1.1 only",
                    },
                    <AllowedValueInfo>{
                        "value": "2.0",
                        "desc": "Require HTTP/2, fail if the server doesn't support it (TLS required)",
                    },
                ),
            },
            "max_redirects": <DataProviderOptionInfo>{
                "display_name": "Max Redirects",
                "short_desc": "Sets the maximum HTTP redirects to support",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Sets the maximum HTTP redirects to support in a single request",
            },
            "no_charset": <DataProviderOptionInfo>{
                "display_name": "Supress Charset",
                "short_desc": "If true no charset will be added to the 'Content-Type' header",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` no charset will be added to the `Content-Type` header",
            },
            "oauth2_auth_args": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Authorization Args",
                "short_desc": "Optional arguments for authentication requests for the authorization_code grant type",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "Optional arguments for authentication requests for the `authorization_code` grant type; "
                    "ignored for logins if the `token` option is set.  Note that the `authorization_code` grant type "
                    "requires external user authorization to acquire an access token",
            },
            "oauth2_auth_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Auth URL",
                "short_desc": "Sets the OAuth2 authorization URL for the authorization_code grant type",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 authorization URL for the `authorization_code` grant type; ignored for logins "
                    "if the `token` option is set.  Note that the `authorization_code` grant type requires "
                    "external user authorization to acquire an access token",
            },
            "oauth2_auto_refresh": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Auto Refresh",
                "short_desc": "Sets if OAuth2 tokens should be automatically refreshed",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If OAuth2 tokens should be automatically refreshed",
                "default_value": True,
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 client ID; ignored for logins if the `token` option is set (but possibly "
                    "used when refreshing tokens)",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Sets the OAuth2 client secret; ignored for logins if the `token` option is set (but "
                    "possibly used when refreshing tokens)",
                "sensitive": True,
            },
            "oauth2_grant_type": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Grant Type",
                "short_desc": "Sets the OAuth2 grant type",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 grant type; ignored for logins if the `token` option is set",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "authorization_code",
                        "desc": "OAuth2 2.0 authorization code grant "
                            "([RFC 6749 authorization code grant]"
                            "(https://datatracker.ietf.org/doc/html/rfc6749#section-4.1)); this grant type "
                            "requires external user authorization to acquire an access token; the "
                            "`oauth2_auth_url` must be set to inform external code where to request "
                            "authorization from the external authorization server",
                    }, <AllowedValueInfo>{
                        "value": "client_credentials",
                        "desc": "OAuth2 2.0 client credentials grant "
                            "([RFC 6749 client credentials grant]"
                            "(https://datatracker.ietf.org/doc/html/rfc6749#section-1.3.4))",
                    }, <AllowedValueInfo>{
                        "value": "password",
                        "desc": "OAuth2 2.0 legacy password grant "
                            "([RFC 6749 password grant]"
                            "(https://datatracker.ietf.org/doc/html/rfc6749#section-1.3.3))",
                    },
                ),
            },
            "oauth2_redirect_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Redirect URL",
                "short_desc": "The OAuth2 redirect URL for the authorization code grant type",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 redirect URL for the `authorization_code` grant type; ignored for logins "
                    "if the `token` option is set.  Note that the `authorization_code` grant type requires "
                    "external user authorization to acquire an access token; the special value `auto` (the "
                    "default) is meant to be interpreted by servers that implement OAuth2 authorization code "
                    "client handling",
                "default_value": "auto",
            },
            "oauth2_refresh_token": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Refresh Token",
                "short_desc": "Sets the OAuth2 refresh token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The OAuth2 refresh token, if any (complements option `token`)",
                "sensitive": True,
            },
            "oauth2_scopes": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Scopes",
                "short_desc": "List of OAuth2 scopes to request",
                "type": AbstractDataProviderTypeMap."list",
                "desc": "Space-separated string of OAuth2 scopes to request; ignored if the `token` option is set",
            },
            "oauth2_token_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Token URL",
                "short_desc": "The token URL for OAuth2 flows",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The token URL for OAuth2 flows; ignored for logins if the `token` option is set",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The password for authentication; only used if no username or password is set in the URL "
                    "and if the `username` option is also used; conflicts with the `token` option",
                "sensitive": True,
            },
            "proxy": <DataProviderOptionInfo>{
                "display_name": "HTTP Proxy",
                "short_desc": "The HTTP proxy to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The proxy URL to use",
            },
            "redirect_passthru": <DataProviderOptionInfo>{
                "display_name": "Redirect Passthrough",
                "short_desc": "If true then redirect responses are returned to the caller",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "if `True` then redirect responses will be passed to the caller instead of processed",
                "default_value": False,
            },
            "send_encoding": <DataProviderOptionInfo>{
                "display_name": "Send Compression",
                "short_desc": "Sets the compression algorithm to use when sending messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Sets the compression / send encoding algorithm to use when sending messages",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "gzip",
                        "desc": "use GNU zip encoding ([RFC 1952](https://tools.ietf.org/html/rfc1952))",
                    }, <AllowedValueInfo>{
                        "value": "bzip2",
                        "desc": "use bzip2 encoding",
                    }, <AllowedValueInfo>{
                        "value": "deflate",
                        "desc": "use the deflate algorithm ([RFC 1951](https://tools.ietf.org/html/rfc1951))",
                    }, <AllowedValueInfo>{
                        "value": "identity",
                        "desc": "use no content encoding",
                    },
                ),
            },
            "ssl_cert_der": <DataProviderOptionInfo>{
                "display_name": "SSL Certificate DER",
                "short_desc": "Indicates if X.509 certificate data is in DER format",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then the data represented by `ssl_cert_location` will be assumed to be in binary "
                    "DER format",
                "default_value": False,
            },
            "ssl_cert_location": <DataProviderOptionInfo>{
                "display_name": "SSL Certificate Location",
                "short_desc": "The location of the X.509 client certificate to use",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_FromFile: True,
                    DTT_FromLocation: True,
                }),
                "desc": "A path or location to an X.509 client certificate file for mutual TLS (mTLS)",
            },
            "ssl_key_der": <DataProviderOptionInfo>{
                "display_name": "SSL Key DER",
                "short_desc": "Indicates if X.509 private key data is in DER format",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then the data represented by `ssl_key_location` will be assumed to be in binary "
                    "DER format",
                "default_value": False,
            },
            "ssl_key_location": <DataProviderOptionInfo>{
                "display_name": "SSL Key Location",
                "short_desc": "The location of the private key file",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_FromFile: True,
                    DTT_FromLocation: True,
                }),
                "desc": "A path or location to a private key file for the X.509 client certificate",
            },
            "ssl_key_password": <DataProviderOptionInfo>{
                "display_name": "SSL Key Password",
                "short_desc": "The password to the private key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The password to the private key given with `ssl_key_location` (text PEM format only)",
                "sensitive": True,
            },
            "ssl_verify_cert": <DataProviderOptionInfo>{
                "display_name": "SSL Verify Certificate",
                "short_desc": "If true then server certificates will be verified",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "if `True` then the server's certificate will only be accepted if it's verified",
                "default_value": False,
            },
            "swagger": <DataProviderOptionInfo>{
                "display_name": "Swagger Schema",
                "short_desc": "Sets the location of a Swagger schema to use for the connection",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_FromFile: True,
                    DTT_FromLocation: True,
                }),
                "desc": "A Swagger 2.0 schema location or file data for the REST connection",
            },
            "swagger_lax_parsing": <DataProviderOptionInfo>{
                "display_name": "Swagger Lax Parsing",
                "short_desc": "Sets lax parsing for Swagger schemas",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Try to parse invalid Swagger schemas; this option is not recommended; it will not parse "
                    "all bad Swagger schemas; schemas should be corrected instead of setting this option",
            },
            "timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "The maximum time an individual I/O operation can take in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Transfer timeout to use in milliseconds (default: 45 seconds)",
                "default_value": 45000,
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Any bearer token to use for the connection; will be passed as "
                    "`Authorization: Bearer ...` (assuming that `token_type` is `Bearer`) in request headers; this "
                    "option cannot be used with the `username` and `password` options or authentication information "
                    "in the URL; if this option is set then OAuth2 options are ignored",
                "sensitive": True,
            },
            "token_type": <DataProviderOptionInfo>{
                "display_name": "Authorization Token Type",
                "short_desc": "The type of authorization token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of `token` to use for the `Authentication` header; ignored if no `token` "
                    "option is set",
                "default_value": "Bearer",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for a REST / HTTP/S connection",
                "required": True,
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The username for authentication; only used if no username or password is set in the URL "
                    "and if the `password` option is also used; conflicts with the `token` option",
            },
            "validator_base_path": <DataProviderOptionInfo>{
                "display_name": "Override Schema Base Path",
                "short_desc": "Override  the base path in any REST schema",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "In case a REST validator is used, the base path in the schema can be overridden with this "
                    "option",
            },
        };
    }

    private {
        const ChildMap = {
            "call": Class::forName("RestClientDataProvider::RestClientCallDataProvider"),
            "delete": Class::forName("RestClientDataProvider::RestClientDeleteDataProvider"),
            "get": Class::forName("RestClientDataProvider::RestClientGetDataProvider"),
            "head": Class::forName("RestClientDataProvider::RestClientHeadDataProvider"),
            "options": Class::forName("RestClientDataProvider::RestClientOptionsDataProvider"),
            "patch": Class::forName("RestClientDataProvider::RestClientPatchDataProvider"),
            "post": Class::forName("RestClientDataProvider::RestClientPostDataProvider"),
            "put": Class::forName("RestClientDataProvider::RestClientPutDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = getRestConnection(copts);
    }

    #! Creates the object from a REST connection
    constructor(RestClientIo rest) : RestClientDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "rest";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for REST servers";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a REST connection
    static RestClientIo getRestConnection(hash<auto> options) {
        options = RestClientDataProvider::processRestOptions(options);
        return new RestClientIo(options);
    }

    #! Makes a REST request and returns the response
    static hash<auto> makeRequest(RestClient::AbstractRestClient rest, string meth, auto body, string path,
            *hash<auto> hdr) {
        hash<RestClient::RestResponseInfo> resp = rest.restDoRequest(meth, path, body, hdr);
        return RestClientDataProviderBase::buildResponseHash(resp);
    }

    #! Processes common REST options
    static hash<auto> processRestOptions(hash<auto> options) {
        if (*string loc = remove options.ssl_cert_location) {
            if (loc =~ /^[a-z]+:\/\//) {
                options.ssl_cert_data = options.ssl_cert_der
                    ? FileLocationHandler::getBinaryFileFromLocation(loc)
                    : FileLocationHandler::getTextFileFromLocation(loc);
            } else {
                options.ssl_cert_path = loc;
            }
        }
        if (*string loc = remove options.ssl_key_location) {
            if (loc =~ /^[a-z]+:\/\//) {
                options.ssl_key_data = options.ssl_key_der
                    ? FileLocationHandler::getBinaryFileFromLocation(loc)
                    : FileLocationHandler::getTextFileFromLocation(loc);
            } else {
                options.ssl_key_path = loc;
            }
        }
        if (*string base_path = remove options.validator_base_path) {
            options.swagger_base_path = base_path;
        }
        return options;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}