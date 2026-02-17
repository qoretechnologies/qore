# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeUpdateCouponDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a coupon
public class ChargebeeUpdateCouponRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Coupon ID",
                "short_desc": "The coupon ID to update",
                "desc": "The unique identifier of the coupon to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Display name of the coupon",
                "desc": "The display name of the coupon",
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

#! Data provider for updating an existing coupon
public class ChargebeeUpdateCouponDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-coupon",
            "desc": "Update an existing coupon in Chargebee",
            "type": "ChargebeeUpdateCouponDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeUpdateCouponRequestDataType RequestType();

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
        return "update-coupon";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing coupon in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating coupon %s", id));

        hash<auto> response = doPost("/coupons/" + id + "/update_for_items", body);
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
