# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantPointsDataProvider class definition

/** QdrantPointsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! The Qdrant points API data provider
/** This data provider provides children that are API data providers for Qdrant point operations
*/
public class QdrantPointsDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "points",
            "desc": "Qdrant points data provider; provides access to point CRUD and payload operations",
            "type": "QdrantPointsDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            "upsert": Class::forName("QdrantDataProvider::QdrantPointsUpsertDataProvider"),
            "get": Class::forName("QdrantDataProvider::QdrantPointsGetDataProvider"),
            "delete": Class::forName("QdrantDataProvider::QdrantPointsDeleteDataProvider"),
            "scroll": Class::forName("QdrantDataProvider::QdrantPointsScrollDataProvider"),
            "count": Class::forName("QdrantDataProvider::QdrantPointsCountDataProvider"),
            "set-payload": Class::forName("QdrantDataProvider::QdrantPointsSetPayloadDataProvider"),
            "overwrite-payload": Class::forName("QdrantDataProvider::QdrantPointsOverwritePayloadDataProvider"),
            "delete-payload": Class::forName("QdrantDataProvider::QdrantPointsDeletePayloadDataProvider"),
            "clear-payload": Class::forName("QdrantDataProvider::QdrantPointsClearPayloadDataProvider"),
            "batch-update": Class::forName("QdrantDataProvider::QdrantPointsBatchUpdateDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant %s data provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
