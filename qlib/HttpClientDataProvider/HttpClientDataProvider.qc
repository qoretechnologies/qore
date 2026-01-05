# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HttpClientDataProvider class definition

/** HttpClientDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HttpClientDataProvider module
public namespace HttpClientDataProvider {
#! The HTTP data provider class, provides API classes as children
public class HttpClientDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "HttpClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor arguments
        const ConstructorOptions = {
            "assume_encoding": <DataProviderOptionInfo>{
                "display_name": "Assume Character Encoding",
                "short_desc": "Sets the character encoding to assume if 'charset' is missing",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Assumes the given encoding if the server does not send a `charset` value",
            },
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "The connect timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The connection timeout to use in milliseconds (default: 45 seconds)",
                "default_value": 45000,
            },
            "error_passthru": <DataProviderOptionInfo>{
                "display_name": "Error Passthrough",
                "short_desc": "If errors should be returned as normal responses",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then HTTP status codes indicating errors will not cause an "
                    "`HTTP-CLIENT-RECEIVE-ERROR` exception to be raised, rather such responses will be "
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
                "short_desc": "HTTP version to use",
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
            "pre_encoded_urls": <DataProviderOptionInfo>{
                "display_name": "Pre-Encoded URLs",
                "short_desc": "If true then URLs are assumed to be preencoded",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "if `true` then all URI paths in URLs are assumed to be already "
                    "[percent encoded](https://en.wikipedia.org/wiki/Percent-encoding); if this flag is set and "
                    "any unencoded characters are sent in a URL with a new request, an exception is raised",
                "default_value": False,
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
                "desc": "A path or location to an X.509 client certificate file",
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
                "desc": "The password to the private key given with `ssl_key_path` (text PEM format only)",
                "sensitive": True,
            },
            "ssl_verify_cert": <DataProviderOptionInfo>{
                "display_name": "SSL Verify Certificate",
                "short_desc": "If true then server certificates will be verified",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "if `True` then the server's certificate will only be accepted if it's verified",
                "default_value": False,
            },
            "timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "The maximum time an individual I/O operation can take in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Transfer timeout to use in milliseconds (default: 45 seconds)",
                "default_value": 45000,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for an HTTP or HTTPS connection",
                "required": True,
            },
        };

        HTTPClient http;
    }

    private {
        const ChildMap = {
            "call": Class::forName("HttpClientDataProvider::HttpClientCallDataProvider"),
            "delete": Class::forName("HttpClientDataProvider::HttpClientDeleteDataProvider"),
            "get": Class::forName("HttpClientDataProvider::HttpClientGetDataProvider"),
            "head": Class::forName("HttpClientDataProvider::HttpClientHeadDataProvider"),
            "options": Class::forName("HttpClientDataProvider::HttpClientOptionsDataProvider"),
            "patch": Class::forName("HttpClientDataProvider::HttpClientPatchDataProvider"),
            "post": Class::forName("HttpClientDataProvider::HttpClientPostDataProvider"),
            "put": Class::forName("HttpClientDataProvider::HttpClientPutDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        http = getHttpConnection(options);
    }

    #! Creates the object from an HTTP connection
    constructor(HTTPClient http) {
        self.http = http;
    }

    #! Returns the data provider name
    string getName() {
        return "http";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for HTTP servers";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns an HTTP connection
    static HTTPClient getHttpConnection(hash<auto> options) {
        if (*string loc = remove options.ssl_cert_location) {
            if (options.ssl_cert_location =~ /^[a-z]+:\/\//) {
                options.ssl_cert_data = options.ssl_cert_der
                    ? FileLocationHandler::getBinaryFileFromLocation(loc)
                    : FileLocationHandler::getTextFileFromLocation(loc);
            } else {
                options.ssl_cert_path = loc;
            }
        }
        if (*string loc = remove options.ssl_key_location) {
            if (options.ssl_key_location =~ /^[a-z]+:\/\//) {
                options.ssl_key_data = options.ssl_key_der
                    ? FileLocationHandler::getBinaryFileFromLocation(loc)
                    : FileLocationHandler::getTextFileFromLocation(loc);
            } else {
                options.ssl_key_path = loc;
            }
        }
        return new HTTPClient(options);
    }

    #! Makes an HTTP request and returns the response
    static hash<auto> makeRequest(HTTPClient http, string meth, *data body, string path, *hash<auto> hdr) {
        hash<auto> info;
        hash<auto> resp = http.send(body, meth, path, hdr, NOTHING, \info);
        return {
            "request-headers": info.headers,
            "request-uri": info."request-uri",
            "response-body": resp.body,
            "response-headers": (map {$1.key.lwr(): $1.value}, info."response-headers-raw".pairIterator()),
            "response-headers-raw": info."response-headers-raw",
            "response-uri": info."response-uri",
            "status-code": resp.status_code,
            "status-message": info."response-headers".status_message,
        };
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
        return cls.newObject(http);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}