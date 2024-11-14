# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SwaggerRequestDataProvider module definition

/** SwaggerRequestDataProvider.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SwaggerRequestDataProvider module
public namespace SwaggerDataProvider {
#! The Swagger data provider class
public class SwaggerRequestDataProvider inherits SwaggerDataProviderBase {
    public {
        #! The current URI path
        string uri_path;

        #! The URI needs dynamic resolution
        bool resolve_uri;

        #! The path item object for the operation
        PathItemObject pio;

        #! The operation object
        OperationObject op;

        #! The request type
        AbstractDataProviderType request_type;

        #! The response type
        AbstractDataProviderType response_type;

        #! The success response code, if any
        string success_response;

        #! Any headers to return in the response
        *list<string> response_headers;

        #! If the request supports a list of hashes in the response and therefore the record API
        *hash<string, AbstractDataField> record_type;
    }

    private {
        #! Disable checking the request type
        bool disable_request_type_check;
    }

    #! Creates the object from the arguments
    constructor(SwaggerSchema schema, *RestClient rest, string uri_path, PathItemObject pio, OperationObject op)
            : SwaggerDataProviderBase(rest) {
        self.schema = schema;
        self.uri_path = uri_path;
        self.pio := pio;
        self.op := op;

        if (cb_value_needs_resolution) {
            resolve_uri = cb_value_needs_resolution(uri_path);
        }

        # check for any successful response data structure
        foreach hash<auto> i in (op.responses.responses.pairIterator()) {
            if (i.key =~ /^2/ && i.value) {
                success_response = i.key;
                break;
            }
        }
    }

    #! Returns the data provider name
    string getName() {
        return op.method.upr();
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Swagger/OpenAPI data provider for URL `%s`, URI path `%s`, and HTTP method `%s`.\n\n"
            "## Summary\n\n%s\n\n## Description\n\n%s",
            rest.getURL(), uri_path, op.method.upr(), op.summary, op.desc);
    }

    #! Returns data provider info
    hash<DataProviderInfo> getInfo() {
        hash<DataProviderInfo> rv({
            "name": getName(),
        });
        rv += getStaticInfoImpl();
        return rv;
    }

    #! Disable the request type check
    disableRequestTypeCheck() {
        disable_request_type_check = True;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw INVALID-REQUEST the request argument provided do not match the expected request type
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (!disable_request_type_check) {
            return AbstractDataProvider::doRequest(req, request_options);
        }
        return doRequestImpl(req, validateRequestOptions(request_options));
    }

    #! Returns the description of the record type, if any
    /** Returns the record type description for any successful response
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        if (!record_type) {
            record_type = getRecordTypeIntern();
        }
        return record_type;
    }

    private *hash<string, AbstractDataField> getRecordTypeIntern() {
        *ResponseObject response = getSuccessResponse();
        if (response.schema) {
            AbstractDataField response_field = schema.getFieldFromSchema("body", "REST response body argument",
                response.schema);
            AbstractDataProviderType type = response_field.getType();
            *hash<string, AbstractDataField> rv = type.getFields();
            # if we have a list of hashes, then return the list element type
            if (type.getBaseTypeCode() == NT_LIST) {
                rv = type.getElementType().getFields();
            }
            return rv;
        }
    }

    #! Returns the description of a successful request message, if any
    private *AbstractDataProviderType getRequestTypeImpl() {
        if (request_type) {
            return request_type;
        }

        HashDataType rv(getName() + ": request");
        bool required = False;
        *hash<string, AbstractParameterObject> params = pio.parameters + op.parameters;
        if (params) {
            schema.addFieldsFromParameters(rv, params, \required);
        }
        *AbstractParameterObject body = op.body ?? pio.body;
        if (body) {
            AbstractDataField body_field = schema.getFieldFromSchema("body", "REST request body argument",
                cast<BodyParameter>(body).schema, body.required ?? False, rest);
            rv.addField(body_field);
            if (!required && body.required) {
                required = True;
            }
        }
        if (op.formData) {
            HashDataType formData(getName() + ": request formData");
            foreach hash<auto> i in (op.formData.pairIterator()) {
                TypedParameter formData = cast<TypedParameter>(i.value);
                rv.addField(new QoreDataField(i.key, "formData." + i.key, schema.getType("/request/" + i.key,
                    i.value.type, i.value.format, i.value.items, i.value.required)));
                if (!required && i.value.required) {
                    required = True;
                }
            }
            rv.addField(new QoreDataField("formData", "REST request form data", formData));
        }
        return request_type = required ? rv : rv.getOrNothingType();
    }

    #! Returns the description of a successful response message, if any
    private *AbstractDataProviderType getResponseTypeImpl() {
        if (response_type) {
            return response_type;
        }

        *ResponseObject response = getSuccessResponse();
        if (response) {
            return response_type = getResponseTypeIntern(response, "success");
        }
    }

    #! Returns a hash of error responses, if any
    private *hash<string, AbstractDataProviderType> getErrorResponseTypesImpl() {
        *hash<string, AbstractDataProviderType> rv = map {$1.key: getResponseTypeIntern($1.value,
            ($1.key =~ /^2/ ? "success" : "error") + sprintf(" (%s)", $1.key))},
            op.responses.responses.pairIterator(), $1.key != success_response;
        if (op.responses.defaultResp) {
            rv."default" = getResponseTypeIntern(op.responses.defaultResp, "default");
        }
        return rv;
    }

    #! Returns the type for the given error code
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return the type for the given error code

        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known
    */
    private AbstractDataProviderType getErrorResponseTypeImpl(string error_code) {
        *ResponseObject resp = op.responses.responses{error_code};
        if (!resp && error_code == "default" && op.responses.defaultResp) {
            resp = op.responses.defaultResp;
        }
        if (!resp) {
            throw "UNKNOWN-ERROR-RESPONSE", sprintf("error code %y is unknown; known error codes: %y", error_code,
                keys op.responses.responses + (op.responses.defaultResp ? ("default",) : ()));
        }
        return getResponseTypeIntern(resp, error_code);
    }

    #! Returns a data provider type object for a Swagger response
    private AbstractDataProviderType getResponseTypeIntern(ResponseObject response, string label) {
        label += " response";
        HashDataType rv();
        if (response.schema) {
            rv.addField(schema.getFieldFromSchema("body", "REST " + label + " body argument", response.schema));
        }
        if (response.headers) {
            rv.addField(schema.getFieldForHeaders("headers", response.headers));
            response_headers = keys response.headers;
        }
        return rv;
    }

    #! Returns the schema for the first successful response message found
    private *ResponseObject getSuccessResponse() {
        return success_response
            ? op.responses.responses{success_response}
            : op.responses.defaultResp;
    }

    #! Makes a request and returned the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRequestIntern(req, request_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param req the request to serialize and make according to the request type
        @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned

        @throw SWAGGER-SEARCH-ERROR the response type is not list or hash and does not support searching
    */
    private AbstractDataProviderRecordIterator requestSearchRecordsImpl(auto req,
            *hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        hash<auto> resp = doRequestIntern(req, search_options);
        # return an iterator for the result
        AbstractIterator i;
        switch (resp.body.typeCode()) {
            case NT_HASH:
                i = new SingleValueIterator(resp.body);
                break;
            case NT_LIST:
                i = new ListIterator(resp.body);
                break;
            default:
                throw "SWAGGER-SEARCH-ERROR", sprintf("the response has type %y, which does not support searching",
                    resp.body.type());
                break;
        }
        return new DefaultRecordIterator(i, where_cond, NOTHING, record_type);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param req the request to serialize and make according to the request type
        @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned

        @throw SWAGGER-SEARCH-ERROR the response type is not list or hash and does not support searching
    */
    private AbstractDataProviderRecordIterator requestSearchRecordsImpl(auto req, *hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> resp = doRequestIntern(req, search_options);
        # return an iterator for the result
        AbstractIterator i;
        switch (resp.body.typeCode()) {
            case NT_HASH:
                i = new SingleValueIterator(resp.body);
                break;
            case NT_LIST:
                i = new ListIterator(resp.body);
                break;
            default:
                throw "SWAGGER-SEARCH-ERROR", sprintf("the response has type %y, which does not support searching",
                    resp.body.type());
                break;
        }
        return new DefaultRecordIterator(i, where_cond, NOTHING, record_type);
    }

    #! Makes a REST request and returns the response
    private *hash<auto> doRequestIntern(auto req, *hash<auto> options) {
        RestClient rest = getRestClient();
        # make URI path with any query args
        string uri_path = getUriPath();
        map uri_path = replace(uri_path, "{" + $1.key + "}", $1.value.toString()), options.path_vars.pairIterator();
        if (req.query) {
            uri_path += "?" +
                (foldl $1 + "&" + $2,
                    (map sprintf("%s=%s", $1.key, getUriValue($1.value)), req.query.pairIterator()));
            remove req.query;
        }

        # setup headers
        *hash<auto> hdr = req.header + options.hdr;

        # set up Accept header
        *softlist<string> content_types = MimeContentTypes;
        PathItemObject pio = schema.paths.match(uri_path);

        # perform validation
        hash<string, bool> mime_types;
        hash<UriQueryInfo> uri_info = parse_uri_query(uri_path);
        op.validateRequest(True, pio, \uri_info, \req.body, \hdr, \mime_types, disable_request_type_check);

        # make the call
        hash<auto> info;
        try {
            *data body = op.getRequestBody(pio, req.body, \hdr, disable_request_type_check);

            # ensure we specify JSON for the response if possible
            if (!hdr.Accept && op.produces{MimeTypeJson}) {
                hdr.Accept = MimeTypeJson;
            }

            # prepend base path to request, if any
            string base_path = options.base_path ?? schema.getBasePath();
            if (base_path.val() && base_path != "/") {
                if (uri_path !~ /^\//) {
                    base_path += "/";
                }
                uri_path = base_path + uri_path;
            }
            hash<auto> resp = rest.doValidatedRequest(op.method, uri_path, body, \info, NOTHING, hdr);
            *hash<auto> rv;
            if (resp.body.val()) {
                rv.body = resp.body;
            }
            if (response_headers) {
                rv.headers = map {$1: info."response-headers"{$1.lwr()}}, response_headers;
            }
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            error("SWAGGER-REQUEST-ERROR", "%s info: %y", get_exception_string(ex), info);
            rethrow ex.err, ex.desc, info;
        }
    }

    #! Returns the value for the given query argument
    string getUriValue(auto v) {
        switch (v.typeCode()) {
            case NT_BOOLEAN:
                return v ? "true" : "false";
            case NT_LIST:
                return foldl $1 + "," + $2, (map getUriValue($1), v);
            default:
                return v.toString();
        }
    }

    #! Returns the URI path to use in requests
    private string getUriPath() {
        string uri_path = self.uri_path;
        if (resolve_uri) {
            uri_path = cb_resolve_value(uri_path);
        }
        return uri_path;
    }

    #! Returns a REST client for HTTP operations
    private RestClient getRestClient() {
        if (!rest) {
            rest = new RestClient({
                "url": schema.getTargetUrl(),
                "validator": schema,
            });
        }
        return rest;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = SwaggerDataProvider::ProviderInfo - "constructor_options";
        rv += {
            "supports_read": exists record_type,
            "supports_request": True,
            "has_record": exists record_type,
            "request_options": {
                "base_path": hash<DataProviderOptionInfo>{
                    "display_name": "Swagger Base Path",
                    "short_desc": "Override the base URI path for the REST request",
                    "type": AbstractDataProviderTypeMap."string",
                    "required": False,
                    "desc": "Override the base URI path for the REST request",
                },
                "hdr": hash<DataProviderOptionInfo>{
                    "display_name": "HTTP Headers",
                    "short_desc": "HTTP headers to send in each request",
                    "type": AbstractDataProviderTypeMap."hash",
                    "required": False,
                    "desc": "HTTP headers to send in each request",
                },
                "path_vars": hash<DataProviderOptionInfo>{
                    "display_name": "URI Path Variables",
                    "short_desc": "URI path variables for generic paths",
                    "type": AbstractDataProviderTypeMap."hash",
                    "required": False,
                    "desc": "URI path variables for generic paths",
                },
            },
            "search_options": {
                "limit": <DataProviderOptionInfo>{
                    "display_name": "Limit",
                    "short_desc": "The maximum number of records to return",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "The maximum number of records to return",
                },
            },
        };
        return rv;
    }
}
}
