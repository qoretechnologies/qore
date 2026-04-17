# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFulfillmentsDataProvider.qc ClickFunnels fulfillments collection and actions

/*  ClickFunnelsFulfillmentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! ClickFunnels fulfillment line item type for create fulfillment requests
public class ClickFunnelsFulfillmentLineItemDataType inherits HashDataType {
    private {
        const Fields = {
            "invoices_line_item_id": {
                "type": SoftIntType,
                "display_name": "Invoice Line Item ID",
                "short_desc": "The invoice line item to fulfill",
                "desc": "The ID of the orders invoice line item to fulfill",
                "required": True,
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "The quantity to fulfill",
                "desc": "The number of items to fulfill",
                "required": True,
                "example_value": 1,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsFulfillmentsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "fulfillments", "desc": "ClickFunnels fulfillments data provider",
            "type": "ClickFunnelsFulfillmentsDataProvider",
            "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = ClickFunnelsDataProviderBase::WorkspaceIdSearchOption + {
            "fulfillment_id": <DataProviderOptionInfo>{ "display_name": "Fulfillment ID", "short_desc": "Get a specific fulfillment", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Filter by fulfillment ID for single record lookup" },
            "contact_id": <DataProviderOptionInfo>{ "display_name": "Contact ID", "short_desc": "Filter by contact", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Filter fulfillments by contact ID", "ref_data": "contacts" },
            "after": <DataProviderOptionInfo>{ "display_name": "After Cursor", "short_desc": "Pagination cursor", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "The cursor value for fetching the next page of results" },
            "sort_order": <DataProviderOptionInfo>{ "display_name": "Sort Order", "short_desc": "Sort order", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Sort order for results", "allowed_values": (<AllowedValueInfo>{"value": "asc", "display_name": "Ascending"}, <AllowedValueInfo>{"value": "desc", "display_name": "Descending"}) },
            "limit": <DataProviderOptionInfo>{ "display_name": "Limit", "short_desc": "Maximum number of results", "type": AbstractDataProviderType::get(SoftIntOrNothingType), "desc": "Maximum number of results to return" },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateFulfillmentDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateFulfillmentDataProvider"),
            "delete": Class::forName("ClickFunnelsDeleteFulfillmentDataProvider"),
            "cancel": Class::forName("ClickFunnelsCancelFulfillmentDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "fulfillments"; }
    *string getDesc() { return "ClickFunnels fulfillments data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) { *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); } }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsFulfillmentDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        setWorkspaceIdFromSearchOpts(search_options);
        if (search_options.fulfillment_id) {
            auto result = doGet("fulfillments/" + search_options.fulfillment_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)), where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.contact_id) { params.contact_id = search_options.contact_id; }
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(getWorkspacePath("fulfillments"), params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Fulfillment ---
public class ClickFunnelsCreateFulfillmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "contact_id": { "type": StringType, "display_name": "Contact ID", "short_desc": "The contact", "desc": "The ID of the contact for this fulfillment", "required": True, "ref_data": "contacts", "example_value": "12345" },
            "location_id": { "type": StringType, "display_name": "Location ID", "short_desc": "The fulfillment location", "desc": "The ID of the fulfillment location where items are shipped from", "required": True },
            "included_orders_invoices_line_items_attributes": {
                "type": new ListDataType("included_orders_invoices_line_items_attributes",
                    new ClickFunnelsFulfillmentLineItemDataType(), False),
                "display_name": "Line Items to Fulfill",
                "short_desc": "Invoice line items to fulfill",
                "desc": "A list of invoice line items to fulfill. Each item requires "
                    "`invoices_line_item_id` and `quantity`",
                "required": True,
            },
            "tracking_code": { "type": StringOrNothingType, "display_name": "Tracking Code", "short_desc": "The tracking code", "desc": "The shipment tracking code" },
            "tracking_url": { "type": StringOrNothingType, "display_name": "Tracking URL", "short_desc": "The tracking URL", "desc": "The shipment tracking URL" },
            "shipping_provider": { "type": StringOrNothingType, "display_name": "Shipping Provider", "short_desc": "The shipping provider", "desc": "The shipping provider for this fulfillment" },
            "notify_customer": { "type": SoftBoolOrNothingType, "display_name": "Notify Customer", "short_desc": "Send email notification", "desc": "Whether to send an email notification to the customer about the fulfillment" },
            "ignore_out_of_stock": { "type": SoftBoolOrNothingType, "display_name": "Ignore Out of Stock", "short_desc": "Override inventory checks", "desc": "Whether to ignore out-of-stock inventory checks when creating the fulfillment" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateFulfillmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "create", "desc": "Create a fulfillment in ClickFunnels", "type": "ClickFunnelsCreateFulfillmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFulfillmentDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateFulfillmentRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a fulfillment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateFulfillmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFulfillmentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        return doPost(getWorkspacePath("fulfillments"), {"fulfillment": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Fulfillment ---
public class ClickFunnelsUpdateFulfillmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "fulfillment_id": { "type": StringType, "display_name": "Fulfillment ID", "short_desc": "The fulfillment to update", "desc": "The unique identifier of the fulfillment to update", "required": True, "ref_data": "fulfillments" },
            "tracking_code": { "type": StringOrNothingType, "display_name": "Tracking Code", "short_desc": "The tracking code", "desc": "The new tracking code" },
            "tracking_url": { "type": StringOrNothingType, "display_name": "Tracking URL", "short_desc": "The tracking URL", "desc": "The new tracking URL" },
            "shipping_provider": { "type": StringOrNothingType, "display_name": "Shipping Provider", "short_desc": "The shipping provider", "desc": "The new shipping provider" },
            "notify_customer": { "type": SoftBoolOrNothingType, "display_name": "Notify Customer", "short_desc": "Send email notification", "desc": "Whether to send an email notification to the customer about the fulfillment update" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateFulfillmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a fulfillment in ClickFunnels", "type": "ClickFunnelsUpdateFulfillmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFulfillmentDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateFulfillmentRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a fulfillment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateFulfillmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFulfillmentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string id = remove req.fulfillment_id;
        return doPut("fulfillments/" + id, {"fulfillment": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Delete Fulfillment ---
public class ClickFunnelsDeleteFulfillmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "fulfillment_id": { "type": StringType, "display_name": "Fulfillment ID", "short_desc": "The fulfillment to delete", "desc": "The unique identifier of the fulfillment to delete", "required": True, "ref_data": "fulfillments" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsDeleteFulfillmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "delete", "desc": "Delete a fulfillment in ClickFunnels", "type": "ClickFunnelsDeleteFulfillmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "delete"; }
    *string getDesc() { return "Delete a fulfillment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsDeleteFulfillmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        doDelete("fulfillments/" + req.fulfillment_id);
        return {"success": True, "id": req.fulfillment_id};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Cancel Fulfillment ---
public class ClickFunnelsCancelFulfillmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "fulfillment_id": { "type": StringType, "display_name": "Fulfillment ID", "short_desc": "The fulfillment to cancel", "desc": "The unique identifier of the fulfillment to cancel", "required": True, "ref_data": "fulfillments" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCancelFulfillmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "cancel", "desc": "Cancel a fulfillment in ClickFunnels", "type": "ClickFunnelsCancelFulfillmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFulfillmentDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCancelFulfillmentRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "cancel"; }
    *string getDesc() { return "Cancel a fulfillment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCancelFulfillmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFulfillmentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        return doPut("fulfillments/" + req.fulfillment_id + "/cancel", NOTHING);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
