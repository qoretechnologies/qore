# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryContactPersonsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

#! Zoho Inventory contact persons data provider
public class ZohoInventoryContactPersonsDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contactpersons",
            "desc": "Zoho Inventory contact persons",
            "type": "ZohoInventoryContactPersonsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": BaseConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization ID",
                "short_desc": "Zoho Inventory organization ID",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The organization ID",
                "required": True,
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "The parent contact ID",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The contact ID to list contact persons for",
                "required": True,
            },
            "contact_person_id": <DataProviderOptionInfo>{
                "display_name": "Contact Person ID",
                "short_desc": "Filter by contact person ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The contact person ID for retrieving a single contact person",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contactpersons";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ZohoInventoryContactPersonDataType().getFields();
    }

    #! Searches for contact persons
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }

        string contact_id = search_options.contact_id;

        # Support DPAT_FIND_SINGLE: get a single contact person by ID
        if (search_options.contact_person_id) {
            hash<auto> response = doGet(sprintf("/contacts/%s/contactpersons/%s", contact_id,
                search_options.contact_person_id), params);
            list<auto> records = response.contact_person ? (response.contact_person,) : ();
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
        }

        hash<auto> response = doGet(sprintf("/contacts/%s/contactpersons", contact_id), params);
        return new DefaultRecordIterator(new ListHashIterator(response.contact_persons ?? ()), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
