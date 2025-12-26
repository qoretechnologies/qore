# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcRestDataProvider class definition

/** BcRestDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! The Business Central REST data provider class
public class BcRestDataProvider inherits BcRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BcRestDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };
    }

    private {
        static hash<string, code> dynamic_children;

        const ChildMap = {
            "companies": Class::forName("CdsRestDataProvider::BcCompaniesDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::BusinessCentralRestClient rest) : BcRestDataProviderBase(rest) {
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::BusinessCentralRestConnection conn) : BcRestDataProviderBase(conn) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        bool auto_config = DataProvider::getAutoConfig();
        if (auto_config) {
            map options{$1.key} = ENV{$1.value}, EnvOptions.pairIterator(),
                exists ENV{$1.value} && !exists options{$1.key};
        }
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.bcrestclient) {
            rest = copts.bcrestclient;
        }

        if (!rest) {
            rest = new CdsRestClient::BusinessCentralRestClient(getClientOptions(copts));
        }
    }

    #! Returns the data provider name
    string getName() {
        return "/";
    }

    #! Registers a child data provider
    /** @param name the child's name
        @param generator code to generate the child; prototype: sub (string name, AbstractConnection conn) {}
    */
    static registerChild(string name, code generator) {
        dynamic_children{name} = generator;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<hash<DataProvider::DataProviderSummaryInfo>> rv;
        foreach Class cls in (ChildMap.iterator()) {
            try {
                rv += cls.getConstant("ProviderSummaryInfo").getValue();
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "UNKNOWN-CONSTANT") {
                    rethrow;
                }
            }
            rv += cls.getStaticMember("ProviderSummaryInfo").getValue();
        }
        if (conn) {
            map rv += $1.value($1.key, conn).getSummaryInfo(), dynamic_children.pairIterator();
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        list<string> rv = keys ChildMap;
        if (conn && dynamic_children) {
            rv += keys dynamic_children;
        }
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
        if (conn && (*code gen = dynamic_children{name})) {
            return gen(name, conn);
        }
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
