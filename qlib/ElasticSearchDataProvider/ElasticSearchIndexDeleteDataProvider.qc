# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchIndexDeleteDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! The ElasticSearch index delete API data provider
/** This API allows the caller to delete an ElasticSearch index
*/
public class ElasticSearchIndexDeleteDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "ElasticSearch index delete API data provider",
            "type": "ElasticSearchIndexDeleteDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchIndexDeleteRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchAcknowledgedDataType();

        #! Query args
        const QueryArgs = ("allow_no_indices", "expand_wildcards", "ignore_unavailable", "master_timeout", "timeout");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data index %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string uri_path = sprintf("%s", remove req.index);
        bool q;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!q) {
                uri_path += "?";
                q = True;
            } else {
                uri_path += "&";
            }
            uri_path += sprintf("%s=%s", arg, remove req{arg});
        }
        return rest.del(uri_path).body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Index delete API request data type
class ElasticSearchIndexDeleteRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name to delete",
                "desc": "The name of the index to delete",
                "example_value": "my-index",
            },
            "allow_no_indices": {
                "type": SoftBoolStringType,
                "display_name": "Allow No Indices?",
                "short_desc": "Allow no matching indices",
                "desc": "If `false` (default `true`), the request returns an error if any wildcard expression, index "
                    "alias, or `_all` value targets only missing or closed indices. This behavior applies even if "
                    "the request targets other open indices. For example, a request targeting `foo*,bar*` returns an "
                    "error if an index starts with `foo` but no index starts with `bar`",
                "example_value": True,
            },
            "expand_wildcards": {
                "type": StringOrNothingType,
                "display_name": "Expand Wildcards",
                "short_desc": "Wildcard expansion mode",
                "desc": "Type of index that wildcard patterns can match. If the request can target data streams, "
                    "this argument determines whether wildcard expressions match hidden data streams. Supports "
                    "comma-separated values, such as `open,hidden` (default `open,closed`). Valid values are:\n"
                    "- `all`: Match any data stream or index, including hidden ones\n"
                    "- `open`: Match open, non-hidden indices. Also matches any non-hidden data stream.\n"
                    "- `closed`: Match closed, non-hidden indices. Also matches any non-hidden data stream. Data "
                        "streams cannot be closed\n"
                    "- `hidden`: Match hidden data streams and hidden indices. Must be combined with `open`, "
                        "`closed`, or both\n"
                    "- `none`: Wildcard patterns are not accepted",
                "example_value": "open,closed",
            },
            "ignore_unavailable": {
                "type": SoftBoolStringType,
                "display_name": "Ignore Unavailable?",
                "short_desc": "Ignore missing indices",
                "desc": "If `false, the request returns an error if it targets a missing or closed "
                    "index. Defaults to `false`",
                "example_value": False,
            },
            "master_timeout": {
                "type": StringOrNothingType,
                "display_name": "Master Timeout",
                "short_desc": "Master node timeout",
                "desc": "Period to wait for a connection to the master node (default `30s`). If no response is "
                    "received before the timeout expires, the request fails and returns an error",
                "example_value": "30s",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Response timeout",
                "desc": "Period to wait for a response (default `30s`). If no response is received before the "
                    "timeout expires, the request fails and returns an error",
                "example_value": "30s",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
