# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsOrdersDataProvider.qc ClickFunnels orders collection and actions

/*  ClickFunnelsOrdersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels order line item type for create order requests
public class ClickFunnelsOrderLineItemDataType inherits HashDataType {
    private {
        const Fields = {
            "variant_id": {
                "type": SoftIntType,
                "display_name": "Variant ID",
                "short_desc": "The product variant ID",
                "desc": "The ID of the product variant to order",
                "required": True,
            },
            "products_price_id": {
                "type": SoftIntType,
                "display_name": "Products Price ID",
                "short_desc": "The product price ID",
                "desc": "The ID of the product price to use",
                "required": True,
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "The quantity to order",
                "desc": "The number of items to order",
                "required": True,
                "example_value": 1,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsOrdersDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "ClickFunnels orders data provider",
            "type": "ClickFunnelsOrdersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Get a specific order by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by order ID for single record lookup",
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter orders by contact ID",
                "ref_data": "contacts",
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The cursor value for fetching the next page of results",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateOrderDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateOrderDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "orders"; }
    *string getDesc() { return "ClickFunnels orders data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsOrderDataType().getFields(); }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.order_id) {
            auto result = doGet("orders/" + search_options.order_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.contact_id) { params.contact_id = search_options.contact_id; }
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(getWorkspacePath("orders"), params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Order ---
public class ClickFunnelsCreateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "example_value": "12345",
                "desc": "The ID of the contact for this order",
                "required": True,
                "ref_data": "contacts",
            },
            "order_type": {
                "type": StringType,
                "display_name": "Order Type",
                "short_desc": "The type of order",
                "desc": "The type of the order to create",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "one-time-order", "display_name": "One-Time Order"},
                    <AllowedValueInfo>{"value": "subscription-order", "display_name": "Subscription Order"},
                ),
            },
            "live_mode": {
                "type": SoftBoolType,
                "display_name": "Live Mode",
                "short_desc": "Whether this is a live order",
                "desc": "Whether this is a live order (`True`) or a test order (`False`)",
                "required": True,
            },
            "line_items_attributes": {
                "type": new ListDataType("line_items_attributes", new ClickFunnelsOrderLineItemDataType(), False),
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "A list of line items for the order. Each item requires `variant_id`, "
                    "`products_price_id`, and `quantity`.\n\n"
                    "**Note**: for one-time orders, line items must reference one-time products. "
                    "For subscription orders, line items can include both one-time and subscription "
                    "products",
                "required": True,
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The order currency",
                "desc": "The currency code for the order (e.g., `USD`)",
            },
            "origination_channel_id": {
                "type": StringOrNothingType,
                "display_name": "Origination Channel ID",
                "short_desc": "The origination channel",
                "desc": "The ID of the origination channel",
            },
            "origination_channel_type": {
                "type": StringOrNothingType,
                "display_name": "Origination Channel Type",
                "short_desc": "The origination channel type",
                "desc": "The type of the origination channel",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateOrderDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new order in ClickFunnels",
            "type": "ClickFunnelsCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsOrderDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateOrderRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a new order in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateOrderRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsOrderDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(getWorkspacePath("orders"), {"order": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Order ---
public class ClickFunnelsUpdateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "type": StringType,
                "display_name": "Order ID",
                "short_desc": "The order to update",
                "desc": "The unique identifier of the order to update",
                "required": True,
                "ref_data": "orders",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The order currency",
                "desc": "The new currency code for the order",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The order status",
                "desc": "The new status for the order",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateOrderDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing order in ClickFunnels",
            "type": "ClickFunnelsUpdateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsOrderDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateOrderRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update an existing order in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateOrderRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsOrderDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.order_id;
        return doPut("orders/" + id, {"order": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
