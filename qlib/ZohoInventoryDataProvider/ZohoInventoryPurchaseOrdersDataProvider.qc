# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryPurchaseOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory purchase orders data provider
public class ZohoInventoryPurchaseOrdersDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "purchaseorders",
            "desc": "Zoho Inventory purchase orders",
            "type": "ZohoInventoryPurchaseOrdersDataProvider",
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
            "vendor_id": <DataProviderOptionInfo>{
                "display_name": "Vendor ID",
                "short_desc": "Filter by vendor ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter purchase orders by vendor ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter purchase orders by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "issued",
                        "desc": "Issued purchase orders",
                    },
                    <AllowedValueInfo>{
                        "value": "cancelled",
                        "desc": "Cancelled purchase orders",
                    },
                    <AllowedValueInfo>{
                        "value": "open",
                        "desc": "Open purchase orders",
                    },
                    <AllowedValueInfo>{
                        "value": "closed",
                        "desc": "Closed purchase orders",
                    },
                    <AllowedValueInfo>{
                        "value": "billed",
                        "desc": "Billed purchase orders",
                    },
                ),
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "purchaseorders";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInventoryPurchaseOrderDataType();
    }

    #! Searches for purchase orders
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.vendor_id) {
            params.vendor_id = search_options.vendor_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        hash<auto> response = doGet("/purchaseorders", params);
        return new DefaultRecordIterator(new ListHashIterator(response.purchaseorders ?? ()), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
