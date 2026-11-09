# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiDataProviderBase.qc ConvertAPI data provider base class

/*  ConvertApiDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConvertApiDataProvider module
public namespace ConvertApiDataProvider {

#! Base class for ConvertAPI data providers
public class ConvertApiDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Supported request methods, mapped to @ref True if the request body is sent
        const RequestMethods = {
            "GET": False,
            "POST": True,
        };

        #! Base constructor options for ConvertAPI data providers
        const BaseConstructorOptions = {
            "convertapirestclient": <DataProviderOptionInfo>{
                "display_name": "ConvertAPI REST Client",
                "short_desc": "The ConvertApiRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("ConvertApiRestClient::ConvertApiRestClientIo")),
                "desc": "The `ConvertApiRestClientIo` object to use for API calls",
            },
            "convertapirestclient_options": <DataProviderOptionInfo>{
                "display_name": "ConvertAPI REST Client Options",
                "short_desc": "Options for creating a ConvertApiRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a `ConvertApiRestClientIo` object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        ConvertApiRestClient::ConvertApiRestClientIo rest;

        #! The connection object, if applicable
        *ConvertApiRestClient::ConvertApiRestConnection conn;

        #! Converter list cache: API URL -> {"timestamp": date, "converters": list}
        static hash<string, hash<auto>> converter_cache = {};

        #! Converter list cache lock
        static Mutex converter_lock();
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ConvertApiRestClient::ConvertApiRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync();
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.convertapirestclient) {
            rest = copts.convertapirestclient;
        } else {
            rest = new ConvertApiRestClient::ConvertApiRestClientIo(copts.convertapirestclient_options ?? {});
        }
    }

    #! Returns the REST client
    ConvertApiRestClient::ConvertApiRestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ConvertApiRestClient::ConvertApiRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Returns the supported reference data types for this provider
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "source_formats": True,
            "destination_formats": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "source_formats":
                return getReferenceSourceFormats();
            case "destination_formats":
                return getReferenceDestinationFormats(action_opts.from);
        }
    }

    #! Returns the converters supported by ConvertAPI
    /** The list is read from the \c info endpoint and cached for @ref ConverterCacheTtl.

        @return a list of converter hashes as returned by the \c info endpoint; each converter has the keys
        \c Name, \c Title, \c Description, \c SourceFileFormats, \c DestinationFileFormats and
        \c ConverterParameterGroups, among others
    */
    list<auto> getConverters() {
        string key = rest.getUrl();
        {
            AutoLock al(converter_lock);
            *hash<auto> entry = converter_cache{key};
            if (entry && (entry.timestamp + ConverterCacheTtl) > now_us()) {
                return entry.converters;
            }
        }
        list<auto> converters = doGetAny("info") ?? ();
        AutoLock al(converter_lock);
        converter_cache{key} = {
            "timestamp": now_us(),
            "converters": converters,
        };
        return converters;
    }

    #! Returns the information about a converter
    /** @param from the source format
        @param to the destination format

        @return the converter hash as returned by the \c info endpoint

        @throw CONVERTAPI-UNKNOWN-CONVERTER no converter exists for the given formats
    */
    hash<auto> getConverterInfo(string from, string to) {
        string path = sprintf("info/%s/to/%s", ConvertApiDataProviderBase::checkFormat(from),
            ConvertApiDataProviderBase::checkFormat(to));
        *list<auto> rv;
        try {
            rv = doGetAny(path);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.arg.status_code == 404) {
                throw "CONVERTAPI-UNKNOWN-CONVERTER", sprintf("no converter from %y to %y", from, to), ex.arg;
            }
            rethrow;
        }
        if (!rv || rv[0].typeCode() != NT_HASH) {
            throw "CONVERTAPI-UNKNOWN-CONVERTER", sprintf("no converter from %y to %y", from, to);
        }
        return rv[0];
    }

    #! Returns the source formats as reference data
    private *list<hash<AllowedValueInfo>> getReferenceSourceFormats() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # source format -> list of destination formats
            hash<auto> formats = {};
            foreach hash<auto> converter in (getConverters()) {
                foreach string from in (converter.SourceFileFormats) {
                    if (!formats{from}) {
                        formats{from} = ();
                    }
                    formats{from} += converter.DestinationFileFormats;
                }
            }
            foreach string from in (sort(keys formats)) {
                rv += <AllowedValueInfo>{
                    "display_name": from.upr(),
                    "short_desc": sprintf("Convert from %s", from.upr()),
                    "desc": sprintf("Convert from `%s` to: %s", from, (foldl $1 + ", " + $2,
                        (map sprintf("`%s`", $1), sort(formats{from})))),
                    "value": from,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns the destination formats for the given source format as reference data
    /** @param from the source format; if not set, no value is returned
    */
    private *list<hash<AllowedValueInfo>> getReferenceDestinationFormats(*string from) {
        if (!from.val()) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        try {
            *list<auto> converters = doGetAny(sprintf("info/%s/to/*",
                ConvertApiDataProviderBase::checkFormat(from)));
            foreach hash<auto> converter in (converters) {
                foreach string to in (converter.DestinationFileFormats) {
                    rv += <AllowedValueInfo>{
                        "display_name": to.upr(),
                        "short_desc": converter.Title ?? sprintf("%s to %s", from.upr(), to.upr()),
                        "desc": trim(converter.Description ?? converter.Title ?? ""),
                        "value": to,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Converts a file with the given converter
    /** @param from the source format (ex: \c "docx")
        @param to the destination format (ex: \c "pdf")
        @param req the common request fields: \c file, \c url, \c files, \c urls, \c store_file, \c file_name,
        \c timeout and \c parameters (see @ref ConvertApiCommonFields)
        @param converter_params other converter parameters with their ConvertAPI names
        @param url_param if set, the \c url request field is sent as a parameter with this name and a URL value
        instead of as the \c File parameter (for example \c "Url" for the \c html to \c pdf converter, which
        renders the web page)

        @return the conversion result: \c ConversionCost and \c Files; the \c FileData of each file is decoded to
        a binary value

        @throw CONVERTAPI-ERROR invalid request
        @throw CONVERTAPI-API-ERROR ConvertAPI returned an error
    */
    hash<auto> convert(string from, string to, hash<auto> req, *hash<auto> converter_params, *string url_param) {
        list<hash<auto>> params = ConvertApiDataProviderBase::makeParameters(req, converter_params, url_param);
        string path = sprintf("convert/%s/to/%s", ConvertApiDataProviderBase::checkFormat(from),
            ConvertApiDataProviderBase::checkFormat(to));
        return ConvertApiDataProviderBase::processResult(doPost(path, {"Parameters": params}));
    }

    #! Returns the ConvertAPI \c Parameters list for a request
    /** @param req the common request fields (see @ref ConvertApiCommonFields)
        @param converter_params other converter parameters with their ConvertAPI names
        @param url_param if set, the \c url field is sent as a parameter with this name and a URL value

        @return the list of ConvertAPI parameter hashes

        @throw CONVERTAPI-ERROR invalid request
    */
    static list<hash<auto>> makeParameters(hash<auto> req, *hash<auto> converter_params, *string url_param) {
        list<hash<auto>> params = ();
        # the main input
        if (req.file) {
            params += {"Name": "File", "FileValue": ConvertApiDataProviderBase::makeFileValue(req.file)};
        }
        if (req.url.val()) {
            if (url_param) {
                params += {"Name": url_param, "Value": req.url};
            } else if (req.file) {
                throw "CONVERTAPI-ERROR", "give either 'file' or 'url', not both";
            } else {
                params += {"Name": "File", "FileValue": {"Url": req.url}};
            }
        }
        list<hash<auto>> file_values = ();
        foreach auto file in (req.files) {
            file_values += ConvertApiDataProviderBase::makeFileValue(file);
        }
        foreach auto url in (req.urls) {
            file_values += {"Url": string(url)};
        }
        if (file_values) {
            params += {"Name": "Files", "FileValues": file_values};
        }
        # the output is always given explicitly, so that the result does not depend on the account defaults
        params += {"Name": "StoreFile", "Value": req.store_file ? True : False};
        if (req.file_name.val()) {
            params += {"Name": "FileName", "Value": req.file_name};
        }
        if (exists req.timeout) {
            params += {"Name": "Timeout", "Value": int(req.timeout)};
        }
        # the converter parameters; parameters given in their own fields take precedence over the same parameters
        # in the "parameters" hash
        # the parameters may be narrowed, so they are combined in an "auto!" local
        hash<auto!> other = {};
        if (req.parameters) {
            other += req.parameters;
        }
        if (converter_params) {
            other += converter_params;
        }
        foreach string name in (keys other) {
            if (!exists other{name}) {
                continue;
            }
            if (name =~ /^(File|Files|StoreFile|FileName|Timeout|Secret|Token)$/i) {
                throw "CONVERTAPI-ERROR", sprintf("parameter %y cannot be given as a converter parameter; use "
                    "the request field for it instead", name);
            }
            params += ConvertApiDataProviderBase::makeParameter(name, other{name});
        }
        return params;
    }

    #! Returns a ConvertAPI parameter hash for a converter parameter value
    /** @param name the ConvertAPI parameter name
        @param val the value; a file hash (with \c name and \c content keys) is sent as a file, and a list of file
        hashes as a list of files

        @return a ConvertAPI parameter hash
    */
    static hash<auto> makeParameter(string name, auto val) {
        if (ConvertApiDataProviderBase::isFile(val)) {
            return {"Name": name, "FileValue": ConvertApiDataProviderBase::makeFileValue(val)};
        }
        if (val.typeCode() == NT_LIST && val && ConvertApiDataProviderBase::isFile(val[0])) {
            return {"Name": name, "FileValues": (map ConvertApiDataProviderBase::makeFileValue($1), val)};
        }
        return {"Name": name, "Value": val};
    }

    #! Returns @ref True if the value is a file hash with \c name and \c content keys
    static bool isFile(auto val) {
        return val.typeCode() == NT_HASH && val.hasKey("name") && val.hasKey("content");
    }

    #! Returns a ConvertAPI file value for a file hash
    /** @param file a file hash with \c name and \c content keys (see @ref DataProvider::FileDataType); the
        content can be binary data, a base64-encoded string, a base64 \c data: URI, or text

        @return a ConvertAPI file value with \c Name and \c Data keys

        @throw CONVERTAPI-ERROR the file has no name or content
    */
    static hash<auto> makeFileValue(auto file) {
        if (!ConvertApiDataProviderBase::isFile(file) || !file.name.val()) {
            throw "CONVERTAPI-ERROR", sprintf("invalid file value: expecting a hash with 'name' and 'content' keys; "
                "got type %y", file.fullType());
        }
        if (!exists file.content || !file.content.size()) {
            throw "CONVERTAPI-ERROR", sprintf("file %y has no content", file.name);
        }
        string data;
        switch (file.content.typeCode()) {
            case NT_BINARY:
                data = make_base64_string(file.content);
                break;
            case NT_STRING: {
                *string b64 = (file.content =~ x/^data:[^;]+;base64,(.*)$/s)[0];
                if (b64.val()) {
                    data = b64;
                } else if (file.content =~ /^[A-Z0-9+\/]+(=*)?$/i) {
                    data = file.content;
                } else {
                    data = make_base64_string(file.content);
                }
                break;
            }
            default:
                throw "CONVERTAPI-ERROR", sprintf("file %y content has type %y; expecting \"binary\" or "
                    "\"string\"", file.name, file.content.type());
        }
        return {
            "Name": file.name,
            "Data": data,
        };
    }

    #! Processes a conversion result: decodes the \c FileData of each output file to binary data
    /** @param body the response body of a conversion request

        @return the processed result
    */
    static hash<auto> processResult(hash<auto> body) {
        if (body.Files.typeCode() != NT_LIST) {
            return body;
        }
        # the response may be narrowed, so the result is built in "auto!" locals
        hash<auto!> rv = body;
        list<auto> files = ();
        foreach hash<auto> file in (body.Files) {
            hash<auto!> f = file;
            if (f.FileData.typeCode() == NT_STRING) {
                f.FileData = parse_base64_string(f.FileData);
            }
            files += f;
        }
        rv.Files = files;
        return rv;
    }

    #! Checks a source or destination format before it is used in a URL path
    /** @param fmt the format, ex: \c "docx", \c "pdf" or \c "delete-pages"

        @return the format in lower case

        @throw CONVERTAPI-ERROR the format contains invalid characters
    */
    static string checkFormat(string fmt) {
        if (fmt !~ /^[a-z0-9][a-z0-9\-_.]*$/i && fmt != "*") {
            throw "CONVERTAPI-ERROR", sprintf("invalid format %y; expecting a format name such as \"docx\" or "
                "\"pdf\"", fmt);
        }
        return fmt.lwr();
    }

    #! Makes a POST request to the ConvertAPI API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw CONVERTAPI-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        auto rv = doRestRequest("POST", path, body);
        return rv.typeCode() == NT_HASH ? rv : {};
    }

    #! Makes a GET request to the ConvertAPI API and returns a hash response
    /** @param path the API path
        @return the response body

        @throw CONVERTAPI-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path) {
        auto rv = doRestRequest("GET", path);
        return rv.typeCode() == NT_HASH ? rv : {};
    }

    #! Makes a GET request to the ConvertAPI API and returns the response body of any type
    /** The \c info endpoints return lists.

        @param path the API path
        @return the response body

        @throw CONVERTAPI-API-ERROR if the response indicates an error
    */
    auto doGetAny(string path) {
        return doRestRequest("GET", path);
    }

    #! Makes a REST request with retry handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body

        @throw CONVERTAPI-API-ERROR if the response indicates an error
    */
    private auto doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                *bool send_body = RequestMethods{method};
                if (!exists send_body) {
                    throw "CONVERTAPI-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                hash<auto> response = rest.restDoRequest(method, path, send_body ? body : NOTHING);
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                ConvertApiDataProviderBase::checkError(ex);
                rethrow;
            }
        }
    }

    #! Checks if a request should be retried
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries || DataProviderRetryUtil::retriesSuppressed()) {
            return False;
        }
        if (DataProviderRetryUtil::isRetriableTransportError(ex, RetrySet)) {
            ++retries;
            DataProviderRetryUtil::waitTransportRetry(pow(2, retries));
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

    #! Raises a ConvertAPI error response as a \c CONVERTAPI-API-ERROR exception
    /** ConvertAPI reports errors with an HTTP error status and a body with \c Code and \c Message keys, for
        example <tt>{"Code": 4011, "Message": "Unauthorized. Invalid or missing API credentials."}</tt>.

        @param ex the exception raised by the REST client

        @throw CONVERTAPI-API-ERROR the exception is a ConvertAPI error response; the exception argument is the
        argument of the original exception (with \c status_code and \c body keys)
    */
    static checkError(hash<ExceptionInfo> ex) {
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.body.typeCode() == NT_HASH && ex.arg.body.Message) {
            throw "CONVERTAPI-API-ERROR", sprintf("%s (code %y, HTTP status %y)", ex.arg.body.Message,
                ex.arg.body.Code, ex.arg.status_code), ex.arg;
        }
    }
}

} # namespace ConvertApiDataProvider
