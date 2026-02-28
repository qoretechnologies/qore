# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoDataProviderBase.qc base class for PDF.co data providers

/*  PdfCoDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Base class for PDF.co data providers
public class PdfCoDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for PDF.co data providers
        const BaseConstructorOptions = {
            "pdfcorestclient": <DataProviderOptionInfo>{
                "display_name": "PDF.co REST Client",
                "short_desc": "The PdfCoRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("PdfCoRestClient::PdfCoRestClient")),
                "desc": "The PdfCoRestClient object to use for API calls",
            },
            "pdfcorestclient_options": <DataProviderOptionInfo>{
                "display_name": "PDF.co REST Client Options",
                "short_desc": "Options for creating a PdfCoRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a PdfCoRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        PdfCoRestClient::PdfCoRestClient rest;

        #! The connection object, if applicable
        *PdfCoRestClient::PdfCoRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(PdfCoRestClient::PdfCoRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.pdfcorestclient) {
            rest = copts.pdfcorestclient;
        } else {
            rest = new PdfCoRestClient::PdfCoRestClient(copts.pdfcorestclient_options ?? {});
        }
    }

    #! Returns the REST client
    PdfCoRestClient::PdfCoRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *PdfCoRestClient::PdfCoRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a POST request to the PDF.co API
    /** @param path the API path (relative to /v1)
        @param body the request body
        @return the response body

        @throw PDFCO-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a GET request to the PDF.co API
    /** @param path the API path (relative to /v1)
        @return the response body

        @throw PDFCO-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path) {
        return doRestRequest("GET", path);
    }

    #! Makes a REST request with retry handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw PDFCO-API-ERROR if the response indicates an error
    */
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> info;
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.get(path, NOTHING, \info);
                        break;
                    case "POST":
                        response = rest.post(path, body, \info);
                        break;
                    default:
                        throw "PDFCO-ERROR", sprintf("unsupported HTTP method: %s", method);
                }

                checkResponse(response);
                if (!exists response.body || response.body.typeCode() != NT_HASH) {
                    return {};
                }
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Checks if a request should be retried
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : 5;
            if (wait_secs < 1 || wait_secs > 60) {
                wait_secs = 5;
            }
            sleep(wait_secs * 1000ms);
            return True;
        }
        return False;
    }

    #! Checks the PDF.co API response for errors
    /** @param response the API response hash

        PDF.co returns \c "error": \c True when an operation fails, with error details
        in the \c "message" field.

        @throw PDFCO-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.typeCode() == NT_HASH && response.body.error == True) {
            string err_msg = response.body.message ?? "Unknown API error";
            throw "PDFCO-API-ERROR", err_msg, response.body;
        }
    }
}

} # namespace PdfCoDataProvider
