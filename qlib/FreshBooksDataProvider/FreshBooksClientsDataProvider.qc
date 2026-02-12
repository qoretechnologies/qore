# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshBooksClientsDataProvider.qc FreshBooks clients collection data provider

/*  FreshBooksClientsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks clients collection data provider
public class FreshBooksClientsDataProvider inherits FreshBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "clients",
            "desc": "FreshBooks clients data provider for listing and searching clients",
            "type": "FreshBooksClientsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = FreshBooksDataProviderBase::AccountIdSearchOption + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Client ID",
                "short_desc": "Get a specific client by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by client ID for single record lookup",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter clients by email address",
            },
            "organization": <DataProviderOptionInfo>{
                "display_name": "Organization",
                "short_desc": "Filter by organization name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter clients by organization name (partial match)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest) : FreshBooksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "clients";
    }

    #! Returns the provider description
    *string getDesc() {
        return "FreshBooks clients data provider for listing and searching clients";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new FreshBooksClientDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        setAccountIdFromSearchOpts(search_options);

        # If id provided, get single record
        if (search_options.id) {
            hash<auto> result = doGet(accountPath(sprintf("/users/clients/%d", search_options.id)));
            return new DefaultRecordIterator(new ListHashIterator((result.client,)),
                where_cond, search_options);
        }

        # Build query params from search options
        hash<auto> params;
        if (search_options.limit) {
            params.per_page = search_options.limit;
        }
        if (search_options.email) {
            params."search[email]" = search_options.email;
        }
        if (search_options.organization) {
            params."search[organization_like]" = search_options.organization;
        }

        # Fetch all pages
        list<hash<auto>> all_records = ();
        int page = 1;
        while (True) {
            params.page = page;
            hash<auto> result = doGet(accountPath("/users/clients"), params);
            if (result.clients) {
                all_records += result.clients;
            }
            # Check if there are more pages
            if (!result.clients || page >= (result.pages ?? 1)) {
                break;
            }
            ++page;
        }

        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshBooksDataProvider
