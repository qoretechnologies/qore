# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCreateCouponDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a coupon
public class WooCommerceCreateCouponRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "code": {
                "display_name": "Code",
                "type": StringType,
                "desc": "The coupon code",
                "required": True,
            },
            "discount_type": {
                "display_name": "Discount Type",
                "type": StringOrNothingType,
                "desc": "The type of discount: `percent`, `fixed_cart`, `fixed_product`",
                "allowed_values": DiscountTypeAllowedValues,
            },
            "amount": {
                "display_name": "Amount",
                "type": StringOrNothingType,
                "desc": "The discount amount",
            },
            "individual_use": {
                "display_name": "Individual Use",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the coupon can only be used individually",
            },
            "product_ids": {
                "display_name": "Product IDs",
                "type": SoftListOrNothingType,
                "desc": "List of product IDs the coupon can be applied to",
            },
            "excluded_product_ids": {
                "display_name": "Excluded Product IDs",
                "type": SoftListOrNothingType,
                "desc": "List of product IDs excluded from the coupon",
            },
            "usage_limit": {
                "display_name": "Usage Limit",
                "type": IntOrNothingType,
                "desc": "Maximum number of times the coupon can be used in total",
            },
            "usage_limit_per_user": {
                "display_name": "Usage Limit Per User",
                "type": IntOrNothingType,
                "desc": "Maximum number of times the coupon can be used per customer",
            },
            "date_expires": {
                "display_name": "Date Expires",
                "type": StringOrNothingType,
                "desc": "The date the coupon expires in ISO8601 format",
            },
            "free_shipping": {
                "display_name": "Free Shipping",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the coupon grants free shipping",
            },
            "minimum_amount": {
                "display_name": "Minimum Amount",
                "type": StringOrNothingType,
                "desc": "The minimum order amount required to use this coupon",
            },
            "maximum_amount": {
                "display_name": "Maximum Amount",
                "type": StringOrNothingType,
                "desc": "The maximum order amount allowed to use this coupon",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs",
            },
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Data provider for creating a new WooCommerce coupon
public class WooCommerceCreateCouponDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Creates a new WooCommerce coupon",
            "type": "WooCommerceCreateCouponDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceCreateCouponRequestDataType RequestType();

        #! Response type
        static WooCommerceCouponDataType ResponseType();
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
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Creates a new WooCommerce coupon";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("coupons", {} + req);
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
