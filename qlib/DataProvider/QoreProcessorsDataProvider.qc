# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreProcessorsDataProvider class definition

/** QoreProcessorsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Root data provider for built-in Qore pipeline processors
public class QoreProcessorsDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreProcessorsDataProvider",
            "supports_children": True,
            "children_can_support_pipeline_processors": True,
        };

        #! Child processor names
        const Children = {
            "filter": "QoreFilterRecordsProcessor",
            "select-fields": "QoreSelectFieldsProcessor",
            "set-fields": "QoreSetFieldsProcessor",
            "remove-fields": "QoreRemoveFieldsProcessor",
            "search-replace": "QoreSearchReplaceProcessor",
            "record-limit": "QoreRecordLimitProcessor",
        };
    }

    #! Creates the object
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", NOTHING, options);
    }

    #! Returns the name of this data provider
    string getName() {
        return "qore-processors";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Root data provider for built-in Qore data pipeline processors providing filtering, "
            "transformation, and record manipulation capabilities";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys Children;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "filter":
                return new QoreFilterRecordsProcessor();
            case "select-fields":
                return new QoreSelectFieldsProcessor();
            case "set-fields":
                return new QoreSetFieldsProcessor();
            case "remove-fields":
                return new QoreRemoveFieldsProcessor();
            case "search-replace":
                return new QoreSearchReplaceProcessor();
            case "record-limit":
                return new QoreRecordLimitProcessor();
        }
    }
}
}
