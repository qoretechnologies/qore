# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestApiTableDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! The SalesforceRestApiTableDataProvider data provider class
public class SalesforceRestApiTableDataProvider inherits SalesforceRestDataProviderBase {
    public {
        #! current object name
        string name;

        #! sobject description
        hash<auto> sobject;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SalesforceRestApiTableDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Standard children
        const ChildMap = {
            "create": Class::forName("SalesforceRestDataProvider::SalesforceRestApiTableCreateDataProvider"),
            "search": Class::forName("SalesforceRestDataProvider::SalesforceRestApiTableSearchDataProvider"),
            "update": Class::forName("SalesforceRestDataProvider::SalesforceRestApiTableUpdateDataProvider"),
            "upsert": Class::forName("SalesforceRestDataProvider::SalesforceRestApiTableUpsertDataProvider"),
            "delete": Class::forName("SalesforceRestDataProvider::SalesforceRestApiTableDeleteDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(SalesforceRestClient rest, string name, hash<auto> sobject) : SalesforceRestDataProviderBase(rest) {
        self.name = name;
        self.sobject = sobject;
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Salesforce API table data provider parent for URL `%s` and sobject `%s`", rest.getURL(),
            name);
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        list<hash<DataProviderSummaryInfo>> rv;
        if (sobject.queryable) {
            rv += getSummary("search");
        }
        if (sobject.createable) {
            rv += getSummary("create");
        }
        if (sobject.updateable) {
            rv += getSummary("update");
        }
        if (sobject.mergeable) {
            rv += getSummary("upsert");
        }
        if (sobject.deletable) {
            rv += getSummary("delete");
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        list<string> rv;
        if (sobject.queryable) {
            rv += "search";
        }
        if (sobject.createable) {
            rv += "create";
        }
        if (sobject.updateable) {
            rv += "update";
        }
        if (sobject.mergeable) {
            rv += "upsert";
        }
        if (sobject.deletable) {
            rv += "delete";
        }
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "search") {
            if (sobject.queryable) {
                return new SalesforceRestApiTableSearchDataProvider(rest, name, sobject);
            }
            return;
        }
        if (name == "create") {
            if (sobject.createable) {
                return new SalesforceRestApiTableCreateDataProvider(rest, name, sobject);
            }
            return;
        }
        if (name == "update") {
            if (sobject.updateable) {
                return new SalesforceRestApiTableUpdateDataProvider(rest, name, sobject);
            }
            return;
        }
        if (name == "upsert") {
            if (sobject.mergeable) {
                return new SalesforceRestApiTableUpsertDataProvider(rest, name, sobject);
            }
            return;
        }
        if (name == "delete") {
            if (sobject.deletable) {
                return new SalesforceRestApiTableDeleteDataProvider(rest, name, sobject);
            }
            return;
        }
    }

    #! Returns a summar info object for the given child type
    hash<DataProviderSummaryInfo> getSummary(string type) {
        string ctype = type[0].upr() + type[1..];
        return <DataProviderSummaryInfo>{
            "name": type,
            "desc": sprintf("Salesforce API %s table data provider for URL `%s` and sobject `%s`", type,
                rest.getURL(), name),
            "type": sprintf("SalesforceRestApiTable%sDataProvider", ctype),
            "supports_request": True,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo - "constructor_options" + <DataProviderInfo>{
            "name": name,
        };
    }
}
}
