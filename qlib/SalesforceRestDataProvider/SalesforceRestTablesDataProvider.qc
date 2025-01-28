# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestTablesDataProvider module definition

/** SalesforceRestTablesDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The SalesforceRest data provider class
public class SalesforceRestTablesDataProvider inherits SalesforceRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SalesforceResTablestDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
            "children_identical": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from the arguments
    constructor(SalesforceRestClient rest) : SalesforceRestDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        bool auto_config = DataProvider::getAutoConfig();
        if (auto_config) {
            map options{$1.key} = ENV{$1.value}, EnvOptions.pairIterator(),
                exists ENV{$1.value} && !exists options{$1.key};
        }
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.salesforcerestclient) {
            copts.salesforcerestclient.checkClientOptions(copts);
            rest = copts.salesforcerestclient;
        }

        if (!rest) {
            rest = new SalesforceRestClient(getClientOptions(copts));
        }
    }

    #! Returns the data provider name
    string getName() {
        return "/";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Salesforce tables data provider for URL `%s`; supports child data provider objects with a "
            "record-based interface", rest.getURL());
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<auto> v = rest.get("sobjects").body.sobjects;
        if (!v) {
            return;
        }
        v = sort(v, int sub(hash<auto> l, hash<auto> r) {
            return l.name <=> r.name;
        });

        return map <DataProviderSummaryInfo>{
            "name": $1.name,
            "desc": sprintf("Salesforce data provider for URL `%s` and sobject `%s`; supports a table-like record-based "
                "interface", rest.getURL(), $1.name),
            "type": "SalesforceRestObjectDataProvider",
            "supports_read": $1.queryable,
            "supports_create": $1.createable,
            "supports_update": $1.updateable,
            "supports_upsert": $1.mergeable,
            "supports_delete": $1.deletable,
            "supports_native_search": $1.queryable,
            "supports_request": False,
            "supports_children": False,
            "has_record": $1.queryable || $1.mergeable || $1.deletable || $1.createable,
            "children_can_support_records": False,
        }, v;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        hash<auto> v = rest.get("sobjects");
        *list<string> rv = map $1.name, v.body.sobjects, $1.queryable;
        return rv ? sort(rv) : NOTHING;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        hash<auto> info;
        try {
            hash<auto> v = rest.get("sobjects/" + name, NOTHING, \info).body.objectDescribe;
            if (!v.queryable) {
                return;
            }
            return new SalesforceRestObjectDataProvider(rest, name, v);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "HTTP-CLIENT-RECEIVE-ERROR" && info."response-headers".status_code == 404) {
                return;
            }
            rethrow;
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns options for the SalesforceRestClient
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("salesforcerestclient_options", "salesforcerestclient")
            + copts.salesforcerestclient_options;
    }
}
}
