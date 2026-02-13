# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCreateBookingDataProvider.qc Square create booking action

/*  SquareCreateBookingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a booking (user-friendly flattened fields)
public class SquareCreateBookingRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "location_id": {
                "type": StringType,
                "display_name": "Location ID",
                "short_desc": "The location ID (required)",
                "desc": "The ID of the location where the booking takes place",
                "required": True,
            },
            "start_at": {
                "type": DateType,
                "display_name": "Start At",
                "short_desc": "Start time (required)",
                "desc": "The start time of the booking",
                "required": True,
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer making the booking",
            },
            "customer_note": {
                "type": StringOrNothingType,
                "display_name": "Customer Note",
                "short_desc": "Note from customer",
                "desc": "A note from the customer about the booking",
            },
            "team_member_id": {
                "type": StringType,
                "display_name": "Team Member ID",
                "short_desc": "Team member (required)",
                "desc": "The ID of the team member providing the service",
                "required": True,
            },
            "service_variation_id": {
                "type": StringType,
                "display_name": "Service Variation ID",
                "short_desc": "Service variation (required)",
                "desc": "The ID of the catalog item variation representing the service",
                "required": True,
            },
            "service_variation_version": {
                "type": IntType,
                "display_name": "Service Variation Version",
                "short_desc": "Service variation version (required)",
                "desc": "The version of the service variation",
                "required": True,
            },
            "duration_minutes": {
                "type": IntOrNothingType,
                "display_name": "Duration (minutes)",
                "short_desc": "Service duration in minutes",
                "desc": "The duration of the service in minutes (overrides the catalog default)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square create booking action data provider
public class SquareCreateBookingDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new booking (appointment) in Square",
            "type": "SquareCreateBookingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new booking (appointment) in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCreateBookingRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareBookingDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Build appointment segment from flattened fields
        hash<auto> segment = {
            "team_member_id": req.team_member_id,
            "service_variation_id": req.service_variation_id,
            "service_variation_version": req.service_variation_version,
        };
        if (req.duration_minutes) {
            segment.duration_minutes = req.duration_minutes;
        }

        hash<auto> booking = {
            "location_id": req.location_id,
            "start_at": formatDateTimeSquare(req.start_at),
            "appointment_segments": (segment,),
        };
        if (req.customer_id) {
            booking.customer_id = req.customer_id;
        }
        if (req.customer_note) {
            booking.customer_note = req.customer_note;
        }

        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "booking": booking,
        };
        return doPost("bookings", body).booking;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
