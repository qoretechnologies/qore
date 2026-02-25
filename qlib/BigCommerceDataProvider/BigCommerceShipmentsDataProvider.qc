# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceShipmentsDataProvider.qc BigCommerce shipments collection data provider

/*  BigCommerceShipmentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! BigCommerce shipments collection data provider (V2 API, sub-resource of orders)
public class BigCommerceShipmentsDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "shipments",
            "desc": "BigCommerce shipments data provider for listing and searching shipments "
                "(requires order_id)",
            "type": "BigCommerceShipmentsDataProvider",
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
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Order ID (required)",
                "type": AbstractDataProviderType::get(SoftIntType),
                "required": True,
                "desc": "The order ID to list shipments for (required - shipments are a "
                    "sub-resource of orders)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number for pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for paginated results",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BigCommerceCreateShipmentDataProvider"),
            "get": Class::forName("BigCommerceGetShipmentDataProvider"),
            "update": Class::forName("BigCommerceUpdateShipmentDataProvider"),
            "delete": Class::forName("BigCommerceDeleteShipmentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "shipments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce shipments data provider for listing and searching shipments "
            "(requires order_id)";
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
        return new BigCommerceShipmentDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # order_id is required for shipments (V2 sub-resource)
        if (!search_options.order_id) {
            throw "BIGCOMMERCE-ERROR", "order_id is required to list shipments";
        }

        hash<auto> params;
        if (search_options.page) {
            params.page = search_options.page;
        }

        list<hash<auto>> all_records = fetchAllPagesV2(
            sprintf("orders/%d/shipments", search_options.order_id), params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
