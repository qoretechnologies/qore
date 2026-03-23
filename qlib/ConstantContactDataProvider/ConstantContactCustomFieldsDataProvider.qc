# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactCustomFieldsDataProvider.qc Constant Contact custom fields collection data provider

/*  ConstantContactCustomFieldsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Constant Contact custom fields collection data provider
public class ConstantContactCustomFieldsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "custom-fields",
            "desc": "Constant Contact custom fields data provider for listing and searching custom fields",
            "type": "ConstantContactCustomFieldsDataProvider",
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
            "custom_field_id": <DataProviderOptionInfo>{
                "display_name": "Custom Field ID",
                "short_desc": "Get a specific custom field by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by custom field ID for single record lookup",
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
            "create": Class::forName("ConstantContactCreateCustomFieldDataProvider"),
            "update": Class::forName("ConstantContactUpdateCustomFieldDataProvider"),
            "delete": Class::forName("ConstantContactDeleteCustomFieldDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest) : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "custom-fields";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Constant Contact custom fields data provider for listing and searching custom fields";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ConstantContactCustomFieldDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If custom_field_id provided, get single record
        if (search_options.custom_field_id) {
            hash<auto> result = doGet("contact_custom_fields/" + search_options.custom_field_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # No pagination for custom fields endpoint - return all results directly
        hash<auto> response = doGet("contact_custom_fields");
        list<hash<auto>> records = response.custom_fields ?? ();
        if (search_options.limit && records.size() > search_options.limit) {
            records = records[0..search_options.limit-1];
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
