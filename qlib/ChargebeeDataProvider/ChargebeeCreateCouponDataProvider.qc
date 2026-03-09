# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCreateCouponDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Request type for creating a coupon
public class ChargebeeCreateCouponRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Coupon ID",
                "short_desc": "User-provided coupon code",
                "desc": "A unique identifier for the coupon (the coupon code)",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Display name of the coupon",
                "desc": "The display name of the coupon",
                "required": True,
            },
            "discount_type": {
                "type": StringType,
                "display_name": "Discount Type",
                "short_desc": "Type of discount",
                "desc": "The type of discount the coupon provides",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "fixed_amount", "desc": "Fixed amount discount"},
                    <AllowedValueInfo>{"value": "percentage", "desc": "Percentage discount"},
                    <AllowedValueInfo>{"value": "offer_quantity", "desc": "Offer quantity discount"},
                ),
            },
            "discount_percentage": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Percentage",
                "short_desc": "Percentage discount value",
                "desc": "The discount percentage (applicable when discount_type is `percentage`)",
            },
            "discount_amount": {
                "type": IntOrNothingType,
                "display_name": "Discount Amount",
                "short_desc": "Fixed discount amount in cents",
                "desc": "The discount amount in the smallest currency unit (applicable when "
                    "discount_type is `fixed_amount`)",
            },
            "duration_type": {
                "type": StringType,
                "display_name": "Duration Type",
                "short_desc": "How long the coupon applies",
                "desc": "Specifies the duration for which the coupon discount applies",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "one_time", "desc": "Applies only once"},
                    <AllowedValueInfo>{"value": "forever", "desc": "Applies forever"},
                    <AllowedValueInfo>{"value": "limited_period", "desc": "Applies for a limited period"},
                ),
            },
            "apply_on": {
                "type": StringOrNothingType,
                "display_name": "Apply On",
                "short_desc": "What the coupon applies to",
                "desc": "Specifies what the coupon discount is applied on",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "invoice_amount", "desc": "Apply on invoice amount"},
                    <AllowedValueInfo>{"value": "each_specified_item", "desc": "Apply on each specified item"},
                ),
            },
            "valid_till": {
                "type": IntOrNothingType,
                "display_name": "Valid Till",
                "short_desc": "Coupon expiry timestamp",
                "desc": "The date until which the coupon is valid (Unix timestamp in seconds)",
            },
            "max_redemptions": {
                "type": IntOrNothingType,
                "display_name": "Max Redemptions",
                "short_desc": "Maximum number of redemptions",
                "desc": "The maximum number of times the coupon can be redeemed",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Currency for the coupon",
                "desc": "The currency code for the coupon (e.g., `USD`, `EUR`). Required for "
                    "fixed_amount discount type",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs for storing additional information about the coupon",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new coupon
public class ChargebeeCreateCouponDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-coupon",
            "desc": "Create a new coupon in Chargebee",
            "type": "ChargebeeCreateCouponDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeCreateCouponRequestDataType RequestType();

        #! Response type
        static ChargebeeCouponDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-coupon";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new coupon in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating coupon %s", req.id));

        hash<auto> response = doPost("/coupons/create_for_items", req);
        return response.coupon;
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

} # namespace ChargebeeDataProvider
