# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeDeleteCouponDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a coupon
public class ChargebeeDeleteCouponRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Coupon ID",
                "short_desc": "The coupon ID to delete",
                "desc": "The unique identifier of the coupon to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a coupon
public class ChargebeeDeleteCouponDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-coupon",
            "desc": "Delete a coupon from Chargebee",
            "type": "ChargebeeDeleteCouponDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeDeleteCouponRequestDataType RequestType();

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
        return "delete-coupon";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a coupon from Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting coupon %s", id));

        hash<auto> response = doPost("/coupons/" + id + "/delete");
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
