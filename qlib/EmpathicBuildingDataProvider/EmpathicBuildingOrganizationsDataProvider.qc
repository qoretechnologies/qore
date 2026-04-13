# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingOrganizationsDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The parent class for Empathic Building organizations
public class EmpathicBuildingOrganizationsDataProvider inherits EmpathicBuildingDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "organizations",
            "desc": "Empathic Building organizations data provider; parent provider for APIs related to "
                "organizations",
            "type": "EmpathicBuildingOrganizationsDataProvider",
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
        #! Org map
        hash<string, hash<auto>> orgmap;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", EmpathicBuildingDataProvider::ConstructorOptions, options);
        rest = EmpathicBuildingDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger)
            : EmpathicBuildingDataProviderBase(rest, logger) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Empathic Building data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("Empathic Building org %y", $1),
            "type": "EmpathicBuildingOrganizationDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllOrgs();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (*hash<auto> org = getAllOrgs(){name}) {
            return new EmpathicBuildingOrganizationDataProvider(rest, logger, org);
        }
    }

    #! Returns access info for all tables
    private hash<string, hash<auto>> getAllOrgs() {
        if (exists orgmap) {
            return orgmap;
        }

        try {
            orgmap = cast<*hash<string, hash<auto>>>(map {$1.id: $1}, doRestCommand("GET", "v1/organizations",
                NOTHING).body) ?? {};
        } catch (hash<ExceptionInfo> ex) {
            # translate RestClientIo error arg to the legacy "request-uri"/"response-code"/"response-body" shape
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }

        return orgmap;
    }
}
}
