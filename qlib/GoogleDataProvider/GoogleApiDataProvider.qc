# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDataProvider module definition

/** GoogleApiDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GoogleDataProvider module
public namespace GoogleDataProvider {
#! The Google generic API data provider class
public class GoogleApiDataProvider inherits GoogleDataProviderBase {
    private {
        #! The Google API name (ex: "calendar")
        string api;

        #! resource and action (ex: "events/get")
        string resource_action;

        #! Any path args associated with this data provider
        *hash<string, softstring> path_args;

        #! The action name
        string name;

        #! Data provider description
        string desc;

        #! The resource name
        string category;

        #! Data provider type name
        string type;
    }

    #! Creates the object
    constructor(RestClient::RestClient rest, string api, string resource_action, *hash<auto> path_args)
            : GoogleDataProviderBase(rest) {
        self.api = api;
        self.resource_action = resource_action;
        self.path_args = cast<hash<string, softstring>>(path_args);

        (category, name) = (resource_action =~ x/([^\/]+)\/([^\/]+)/);
        desc = sprintf("Google %s %s provider for `%s`", api, resource_action, rest.getSafeURL());
        type = sprintf("Google%s%s%sDataProvider", cap(api), cap(category), cap(name));
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return desc;
    }

    #! Prepares information for an API request
    private *hash<auto> prepareRequestForSchema(auto req) {
        return prepareRequestForSchema(api, resource_action, req);
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> req_info = prepareRequestForSchema(req);
        hash<auto> info;
        try {
            *hash<auto> rv = rest.doRequest(getMethodForSchema(api, resource_action), req_info.pathstr, req_info.body,
                \info).body;
            return getResponseTypeImpl().acceptsValue(rv);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            *hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return GoogleDataProviderBase::getRequestTypeForSchemaArgs(api, resource_action, keys path_args);
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return GoogleDataProviderBase::getResponseTypeForSchema(api, resource_action);
    }

    #! If the request body should be populated with missing values
    private bool needsBackfillRequestBody() {
        return name == "update";
    }

    #! Used to set attributes in the request body that are not set in the request
    private hash<auto> backfillRequestBody(auto req) {
        string get_action = category + "/get";
        hash<auto> req_info = prepareRequestForSchema(api, get_action, req, True);

        hash<auto> info;
        try {
            return rest.doRequest("GET", req_info.pathstr, NOTHING, \info).body;
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            *hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Prepares information for an API request
    private *hash<auto> prepareRequestForSchema(string api, string resource_action, auto req, *bool in_backfill) {
        hash<auto> requests = getRequestInfo(api);
        *hash<auto> info = requests{resource_action};
        if (!info) {
            throw "SCHEMA-ERROR", sprintf("Google API %y unknown request path %y requested; known request paths: %y",
                api, resource_action, keys requests);
        }

        AbstractDataProviderType request_type = path_args ? getGoogleRequestType() : info.request_type;

        *hash<auto> req_info = prepareRequestIntern(request_type, req, path_args,
            in_backfill ? False : needsBackfillRequestBody());

        string path = info.path;
        foreach string var in (path =~ x/\{([^\}]*)}/g) {
            if (exists (*string value = path_args{var} ?? req_info.path{var})) {
                path = replace(path, "{" + var + "}", value);
            } else {
                throw "GOOGLE-REQUEST-ERROR", sprintf("%s API %s requires missing path arg %y; path args: %y",
                    api, resource_action, var, path_args);
            }
        }

        if (req_info.query) {
            path += req_info.query;
        }

        return req_info + {
            "pathstr": info_map{api}.basePath + path,
        };
    }

    #! Returns the Google request type for the operation
    private AbstractDataProviderType getGoogleRequestType() {
        return getRequestTypeImpl();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": name,
            "desc": desc,
            "type": type,
            "supports_request": True,
        };
    }
}
}