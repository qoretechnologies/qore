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

    #! Uploads file content if provided, replacing it with a temporary URL
    /** If the request contains a \c file field (FileDataType hash with \c name, \c content,
        \c mime_type), uploads the file via the \c /file/upload/base64 endpoint and replaces
        it with the returned temporary URL in the \c url field.

        @param req the request hash
        @return the modified request hash with \c file replaced by \c url
    */
    hash<auto> uploadFileIfNeeded(hash<auto> req) {
        if (req.file) {
            hash<auto> upload_resp = doPost("file/upload/base64", {
                "file": req.file.content,
                "name": req.file.name,
            });
            req.url = upload_resp.url;
            remove req.file;
        }
        return req;
    }

    #! Parses a JSON string field into a Qore data structure if needed
    /** The PDF.co API expects \c annotations, \c images, and \c fields as JSON arrays
        in the request body. When these fields are typed as strings, the REST client
        sends them as JSON string values (double-serialized). This method parses the
        JSON string into native Qore data so it serializes correctly.

        @param val the field value (string or already-parsed data)
        @return the parsed value
    */
    static auto parseJsonField(auto val) {
        if (val.typeCode() == NT_STRING) {
            return parse_json(val);
        }
        return val;
    }

    #! Removes keys with NOTHING values from each hash in a list
    /** The PDF.co API rejects \c null values for optional string fields like \c pages.
        This method strips any keys with NOTHING values from each element.

        @param items list of hash items (annotations, images, fields, etc.)
        @return the list with nil-valued keys removed from each hash
    */
    static list<auto> stripNilFields(list<auto> items) {
        list<auto> result = ();
        foreach hash<auto> item in (items) {
            hash<auto> clean;
            foreach string key in (keys item) {
                if (exists item{key}) {
                    clean{key} = item{key};
                }
            }
            result += clean;
        }
        return result;
    }

    #! Makes a POST request to the PDF.co API and waits for async jobs to complete
    /** If the response indicates the job is still processing (\c status \c "working"),
        automatically polls \c job/check until the job completes or fails, unless the
        caller explicitly requested async mode.

        @param path the API path (relative to /v1)
        @param body the request body
        @return the response body

        @throw PDFCO-API-ERROR if the response indicates an error
        @throw PDFCO-JOB-TIMEOUT if the job does not complete within the polling limit
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        hash<auto> resp = doRestRequest("POST", path, body);
        # If async was explicitly requested, return the jobId immediately
        if (body."async") {
            return resp;
        }
        return waitForJobIfNeeded(resp);
    }

    #! Makes a GET request to the PDF.co API
    /** @param path the API path (relative to /v1)
        @return the response body

        @throw PDFCO-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path) {
        return doRestRequest("GET", path);
    }

    #! Polls job/check if the response has a jobId but no result data
    /** PDF.co may return a \c jobId with \c status \c 200 but no result data when
        processing takes too long. This method detects incomplete responses and polls
        \c job/check until the job completes.

        @param resp the initial API response
        @return the final response with completed job data

        @throw PDFCO-JOB-TIMEOUT if the job does not complete within the polling limit
        @throw PDFCO-JOB-FAILED if the job fails or is aborted
    */
    private hash<auto> waitForJobIfNeeded(hash<auto> resp) {
        if (!resp.jobId) {
            return resp;
        }
        # If the response already contains result data, no polling needed
        if (exists resp.body || exists resp.url || exists resp.urls
                || exists resp.info || exists resp.barcodes) {
            return resp;
        }
        int attempts = 0;
        while (attempts < MaxJobPollAttempts) {
            ++attempts;
            sleep(JobPollIntervalMs);
            hash<auto> check = doRestRequest("POST", "job/check", {"jobId": resp.jobId});
            switch (check.status) {
                case "success":
                    return check;
                case "working":
                    continue;
                case "failed":
                case "aborted":
                    throw "PDFCO-JOB-FAILED", check.message ?? sprintf("Job %s %s",
                        resp.jobId, check.status), check;
                default:
                    throw "PDFCO-JOB-FAILED", sprintf("unexpected job status: %y for job %s",
                        check.status, resp.jobId), check;
            }
        }
        throw "PDFCO-JOB-TIMEOUT", sprintf("job %s did not complete after %d polling attempts",
            resp.jobId, MaxJobPollAttempts);
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
