# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingLocationsDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! The parent class for Empathic Building locations
public class EmpathicBuildingLocationsDataProvider inherits EmpathicBuildingDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "locations",
            "desc": "Empathic Building locations data provider; parent provider for APIs related to "
                "locations",
            "type": "EmpathicBuildingLocationsDataProvider",
            "constructor_options": EmpathicBuildingDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
            "children_identical": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Org info
        hash<auto> org;

        #! Location map
        hash<string, hash<auto>> locmap;
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger, hash<auto> org)
            : EmpathicBuildingDataProviderBase(rest, logger) {
        self.org = org;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Empathic Building data %s provider for locations for org `%s` (%s), url: `%s`",
            ProviderInfo.name, org.id, org.name, rest.getSafeUrl());
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("Empathic Building org %y location %y", org.id, $1),
            "type": "EmpathicBuildingLocationDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllLocs();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (*hash<auto> loc = getAllLocs(){name}) {
            return new EmpathicBuildingLocationDataProvider(rest, logger, org, loc);
        }
    }

    #! Returns access info for all tables
    private hash<string, hash<auto>> getAllLocs() {
        if (exists locmap) {
            return locmap;
        }

        try {
            locmap = cast<*hash<string, hash<auto>>>(map {$1.id: $1}, doRestCommand("GET", "v1/organizations/"
                + org.id + "/locations", NOTHING).body) ?? {};
        } catch (hash<ExceptionInfo> ex) {
            # translate RestClientIo error arg to the legacy "request-uri"/"response-code"/"response-body" shape
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }

        return locmap;
    }
}
}
