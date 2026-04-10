# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MewsRestDataProvider module definition

/** MewsRestDataProvider.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MewsRestDataProvider module
public namespace MewsRestDataProvider {
#! MewsRest app name
const AppName = "Mews";

#! The MewsRest data provider class
public class MewsRestDataProvider inherits MewsRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MewsRestDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            #"children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "Mews REST Client",
                "short_desc": "The REST client for accessing the Mews server",
                "type": AbstractDataProviderType::get(new Type("AbstractRestClient")),
                "desc": "The REST client for accessing the Mews server; must be a "
                    "`MewsRestClient` or `MewsRestClientIo` instance (both carry the Mews auth "
                    "token state via the shared `MewsRestClientBase` mixin)",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the Mews server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the Mews REST server; overrides any URL in any REST client object passed "
                    "as an option",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options to any REST client created",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the `MewsRestClientIo` constructor; only used if a `MewsRestClientIo` "
                    "object is created for a call to the Mews server",
            },
        };
    }

    private {
        const ChildMap = {
            "tables": Class::forName("MewsRestDataProvider::MewsRestTablesDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = MewsRestDataProvider::getRestConnection(copts);
    }

    #! Creates the object from the REST client
    constructor(AbstractRestClient mews) : MewsRestDataProviderBase(mews) {
    }

    #! Returns the data provider name
    string getName() {
        return "mewsrest";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Mews REST data provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return ("api",) + keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "api") {
            return sdp.getChildProviderPath("api/connector/v1");
        }
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        AbstractDataProvider rv = cls.newObject(rest);
        if (logger) {
            rv.setLogger(logger);
        }
        return rv;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
