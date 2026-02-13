# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareBookingDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square booking record type
public class SquareBookingDataType inherits HashDataType {
    private {
        #! Field definitions for Square bookings
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The booking ID",
                "desc": "The unique identifier of the booking",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The booking version",
                "desc": "The current version of the booking (for optimistic concurrency)",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The booking status",
                "desc": "The current status of the booking (PENDING, CANCELLED_BY_CUSTOMER, "
                    "CANCELLED_BY_SELLER, DECLINED, ACCEPTED, NO_SHOW)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the booking was created",
                "desc": "The timestamp when the booking was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the booking was last updated",
                "desc": "The timestamp when the booking was last updated (RFC 3339 format)",
            },
            "start_at": {
                "type": StringOrNothingType,
                "display_name": "Start At",
                "short_desc": "When the booking starts",
                "desc": "The start time of the booking (RFC 3339 format)",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the booking takes place",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer who made the booking",
            },
            "customer_note": {
                "type": StringOrNothingType,
                "display_name": "Customer Note",
                "short_desc": "Note from the customer",
                "desc": "A note provided by the customer for the booking",
            },
            "seller_note": {
                "type": StringOrNothingType,
                "display_name": "Seller Note",
                "short_desc": "Note from the seller",
                "desc": "A note provided by the seller for the booking",
            },
            "appointment_segments": {
                "type": AutoListOrNothingType,
                "display_name": "Appointment Segments",
                "short_desc": "Service segments in the booking",
                "desc": "List of appointment segments, each with duration_minutes, "
                    "team_member_id, service_variation_id, service_variation_version, "
                    "intermission_minutes",
            },
            "transition_time_minutes": {
                "type": IntOrNothingType,
                "display_name": "Transition Time",
                "short_desc": "Transition time in minutes",
                "desc": "The time in minutes between this booking and the next",
            },
            "location_type": {
                "type": StringOrNothingType,
                "display_name": "Location Type",
                "short_desc": "The location type",
                "desc": "The type of location for the booking (BUSINESS_LOCATION, "
                    "CUSTOMER_LOCATION, PHONE)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
