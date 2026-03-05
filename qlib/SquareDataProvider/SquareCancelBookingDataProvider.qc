# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCancelBookingDataProvider.qc Square cancel booking action

/*  SquareCancelBookingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for canceling a booking
public class SquareCancelBookingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "booking_id": {
                "type": StringType,
                "display_name": "Booking ID",
                "short_desc": "The booking to cancel",
                "desc": "The unique identifier of the booking to cancel",
                "required": True,
            },
            "booking_version": {
                "type": IntOrNothingType,
                "display_name": "Booking Version",
                "short_desc": "Booking version",
                "desc": "The current version of the booking (for optimistic concurrency)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square cancel booking action data provider
public class SquareCancelBookingDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel",
            "desc": "Cancel a booking (appointment) in Square",
            "type": "SquareCancelBookingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "cancel";
    }

    *string getDesc() {
        return "Cancel a booking (appointment) in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCancelBookingRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareBookingDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
        };
        if (req.booking_version) {
            body.booking_version = req.booking_version;
        }
        return doPost(sprintf("bookings/%s/cancel", req.booking_id), body).booking;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
