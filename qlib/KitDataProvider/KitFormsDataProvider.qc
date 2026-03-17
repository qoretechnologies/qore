# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitFormsDataProvider.qc Kit forms collection data provider

/*  KitFormsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit forms collection data provider
public class KitFormsDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "forms",
            "desc": "Kit forms data provider for listing forms",
            "type": "KitFormsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "form_id": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "Filter by form ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by form ID",
                "ref_data": "forms",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by form status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter forms by status",
                "allowed_values": (<AllowedValueInfo>{"value": "active"}, <AllowedValueInfo>{"value": "archived"}, <AllowedValueInfo>{"value": "trashed"}, <AllowedValueInfo>{"value": "all"}),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "add-subscriber": Class::forName("KitAddSubscriberToFormDataProvider"),
            "form-subscribers": Class::forName("KitFormSubscribersDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "forms";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit forms data provider for listing forms";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new KitFormDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }

        # Kit API does not support GET /forms/{id}, so we always list all forms
        # and filter client-side
        list<hash<auto>> all_records = fetchAllPages("forms", "forms", params,
            search_options.limit);

        # Filter by form_id if provided
        if (search_options.form_id) {
            list<hash<auto>> filtered = ();
            foreach hash<auto> form in (all_records) {
                if (string(form.id) == search_options.form_id) {
                    filtered += form;
                    break;
                }
            }
            return new DefaultRecordIterator(new ListHashIterator(filtered),
                where_cond, search_options);
        }

        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
