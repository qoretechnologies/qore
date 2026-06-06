# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchIndexesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch indexes root for providing record-based data providers for each index
public class OpenSearchIndexesDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "indexes",
            "desc": "OpenSearch indexes data provider; provides table-like access to OpenSearch indices",
            "type": "OpenSearchIndexesDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_records": True,
            "children_identical": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", OpenSearchDataProvider::ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("OpenSearch index %y", $1),
            "type": "OpenSearchIndexTableDataProvider",
            "supports_read": OpenSearchIndexTableDataProvider::ProviderInfo.supports_read,
            "supports_create": OpenSearchIndexTableDataProvider::ProviderInfo.supports_create,
            "supports_update": OpenSearchIndexTableDataProvider::ProviderInfo.supports_update,
            "supports_upsert": OpenSearchIndexTableDataProvider::ProviderInfo.supports_upsert,
            "supports_delete": OpenSearchIndexTableDataProvider::ProviderInfo.supports_delete,
            "supports_native_search": OpenSearchIndexTableDataProvider::ProviderInfo.supports_native_search,
            "supports_bulk_read": OpenSearchIndexTableDataProvider::ProviderInfo.supports_bulk_read,
            "supports_bulk_create": OpenSearchIndexTableDataProvider::ProviderInfo.supports_bulk_create,
            "supports_bulk_upsert": OpenSearchIndexTableDataProvider::ProviderInfo.supports_bulk_upsert,
            "supports_request": OpenSearchIndexTableDataProvider::ProviderInfo.supports_request,
            "supports_children": False,
            "transaction_management": OpenSearchIndexTableDataProvider::ProviderInfo.transaction_management,
            "has_record": OpenSearchIndexTableDataProvider::ProviderInfo.has_record,
            "children_can_support_records": False,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllIndices();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *hash<auto> index = getIndex(name);
        if (index) {
            return new OpenSearchIndexTableDataProvider(rest, name, index);
        }
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a hash of indices
    /** @return a hash of indices
    */
    private *hash<auto> getAllIndices() {
        try {
            return rest.restGet("*").body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }
    }

    #! Returns a hash of the given index or @ref nothing
    /** @return a hash of the given index or @ref nothing
    */
    private *hash<auto> getIndex(string name) {
        try {
            return rest.restGet(name).body{name};
        } catch (hash<ExceptionInfo> ex) {
            if (ex.arg.typeCode() == NT_HASH && ex.arg.status_code == 404) {
                return;
            }
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }
    }

    #! Map a @ref RestClient::RestResponseInfo error arg to the legacy ex arg shape
    /** Historically @ref OpenSearchIndexesDataProvider exceptions reported REST error
        context via \c "request-uri" / \c "response-code" / \c "response-body" keys (populated
        from @ref RestClient::RestClient::get() "RestClient::get()"'s \a info reference param).
        The async @ref RestClientIo::RestClientIo "RestClientIo" transport instead throws
        @ref RestClient::REST-RESPONSE-ERROR with a @ref RestClient::RestResponseInfo hashdecl
        in \c ex.arg; translate to the legacy shape so callers do not need to change.
    */
    private static hash<auto> mapRestErrorInfo(hash<ExceptionInfo> ex) {
        if (ex.arg.typeCode() != NT_HASH) {
            return {};
        }
        return {
            "request-uri": ex.arg.request_path,
            "response-code": ex.arg.status_code,
            "response-body": ex.arg.body,
        };
    }
}
}
