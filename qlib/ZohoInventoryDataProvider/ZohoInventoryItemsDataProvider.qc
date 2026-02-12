# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryItemsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory items data provider
public class ZohoInventoryItemsDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "items",
            "desc": "Zoho Inventory items",
            "type": "ZohoInventoryItemsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
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
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by item name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter items by name",
            },
            "sku": <DataProviderOptionInfo>{
                "display_name": "SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter items by SKU",
            },
            "item_type": <DataProviderOptionInfo>{
                "display_name": "Item Type",
                "short_desc": "Filter by item type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter items by type",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "items";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInventoryItemDataType();
    }

    #! Searches for items
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.name) {
            params.name = search_options.name;
        }
        if (search_options.sku) {
            params.sku = search_options.sku;
        }
        if (search_options.item_type) {
            params.item_type = search_options.item_type;
        }
        hash<auto> response = doGet("/items", params);
        return new DefaultRecordIterator(new ListHashIterator(response.items ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
