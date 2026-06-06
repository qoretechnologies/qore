# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OpenSearch app name
const AppName = "OpenSearch";

#! The OpenSearch data provider class
public class OpenSearchDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! current URI path
        string uri_path = "/";

        #! The value to returns as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "OpenSearchDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": NOTHING,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "transaction_management": False,
            "supports_schema": False,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = SearchIndexDataProvider::getRestConstructorOptions("OpenSearch");
    }

    private {
        const ChildMap = {
            "document": Class::forName("OpenSearchDataProvider::OpenSearchDocumentDataProvider"),
            "index": Class::forName("OpenSearchDataProvider::OpenSearchIndexDataProvider"),
            "pipeline": Class::forName("OpenSearchDataProvider::OpenSearchPipelineDataProvider"),
            "indexes": Class::forName("OpenSearchDataProvider::OpenSearchIndexesDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(copts);
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    static AbstractRestClient getRestConnection(*hash<auto> options) {
        return SearchIndexDataProvider::SearchIndexRestUtil::getRestConnection(options, "OpenSearch");
    }

    #! Returns the data provider name
    string getName() {
        return "opensearch";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch data provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
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
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Boolean string type for query parameters
public const SoftBoolStringType = SearchIndexDataProvider::SoftBoolStringType;

#! Boolean data provider string type for query parameters
public const SoftBoolDataProviderStringType = SearchIndexDataProvider::SoftBoolDataProviderStringType;
}
