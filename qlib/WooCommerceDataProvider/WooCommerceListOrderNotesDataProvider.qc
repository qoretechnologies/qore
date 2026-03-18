# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceListOrderNotesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Request type for listing order notes
public class WooCommerceListOrderNotesRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "type": IntType,
                "desc": "The unique identifier for the order",
                "required": True,
                "example_value": 1234,
            },
            "type": {
                "display_name": "Type",
                "type": StringOrNothingType,
                "desc": "Filter by note type: `any`, `customer`, or `internal`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "any", "desc": "All notes"},
                    <AllowedValueInfo>{"value": "customer", "desc": "Customer notes only"},
                    <AllowedValueInfo>{"value": "internal", "desc": "Internal notes only"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for listing order notes
public class WooCommerceListOrderNotesDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-notes",
            "desc": "Lists notes for a WooCommerce order",
            "type": "WooCommerceListOrderNotesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceListOrderNotesRequestDataType RequestType();

        #! Response type - list of notes
        static *AbstractDataProviderType ResponseType = new ListDataType("OrderNoteList", new WooCommerceOrderNoteDataType(), True);
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "list-notes";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lists notes for a WooCommerce order";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int order_id = remove req.order_id;
        *hash<auto> params;
        if (req."type") {
            params."type" = req."type";
        }
        auto result = doGet(sprintf("orders/%d/notes", order_id), params);
        return {"notes": result};
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
