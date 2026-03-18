# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceTopSellersReportDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for top sellers report
public class WooCommerceTopSellersReportRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "date_min": {
                "display_name": "Date Min",
                "type": StringOrNothingType,
                "desc": "Start date for the report in YYYY-MM-DD format",
                "example_value": "2026-01-01",
            },
            "date_max": {
                "display_name": "Date Max",
                "type": StringOrNothingType,
                "desc": "End date for the report in YYYY-MM-DD format",
                "example_value": "2026-03-31",
            },
            "period": {
                "display_name": "Period",
                "type": StringOrNothingType,
                "desc": "Predefined period for the report",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "week", "desc": "This week"},
                    <AllowedValueInfo>{"value": "month", "desc": "This month"},
                    <AllowedValueInfo>{"value": "last_month", "desc": "Last month"},
                    <AllowedValueInfo>{"value": "year", "desc": "This year"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for WooCommerce top sellers report
public class WooCommerceTopSellersReportDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "top-sellers-report",
            "desc": "Retrieves a WooCommerce top sellers report",
            "type": "WooCommerceTopSellersReportDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceTopSellersReportRequestDataType RequestType();

        #! Response type - list of top sellers
        static *AbstractDataProviderType ResponseType = new ListDataType("TopSellerList", new WooCommerceTopSellerDataType(), True);
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
        return "top-sellers-report";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Retrieves a WooCommerce top sellers report";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        *hash<auto> params;
        if (req.date_min) { params.date_min = req.date_min; }
        if (req.date_max) { params.date_max = req.date_max; }
        if (req.period) { params.period = req.period; }
        auto result = doGet("reports/top_sellers", params);
        return {"top_sellers": result};
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
