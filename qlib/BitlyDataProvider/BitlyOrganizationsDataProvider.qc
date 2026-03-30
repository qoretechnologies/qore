# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyOrganizationsDataProvider.qc Bitly organizations collection data provider

/*  BitlyOrganizationsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly organizations collection data provider
public class BitlyOrganizationsDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "organizations",
            "desc": "Bitly organizations data provider for listing and retrieving organizations",
            "type": "BitlyOrganizationsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "organization_guid": <DataProviderOptionInfo>{
                "display_name": "Organization GUID",
                "short_desc": "Get a specific organization by GUID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The organization GUID to retrieve. When provided, returns a single "
                    "organization",
                "ref_data": "organizations",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "organizations";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly organizations data provider for listing and retrieving organizations";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new BitlyOrganizationDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If organization_guid provided, get single record
        if (search_options.organization_guid) {
            hash<auto> result = doGet("organizations/" + search_options.organization_guid);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> response = doGet("organizations");
        list<hash<auto>> records = response.organizations ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
