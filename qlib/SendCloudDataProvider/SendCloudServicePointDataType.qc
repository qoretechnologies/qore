# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudServicePointDataType.qc SendCloud service point data type

/*  SendCloudServicePointDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Service-point opening hours entry
public class SendCloudServicePointOpeningHoursDataType inherits HashDataType {
    public {
        const Fields = {
            "day": {
                "type": SoftStringOrNothingType,
                "display_name": "Day",
                "short_desc": "Day of the week",
                "desc": "Day of the week (e.g., monday, tuesday)",
            },
            "opening_times": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Opening Times",
                "short_desc": "Time ranges",
                "desc": "List of time-range hashes with from/to fields (opening hours for this day)",
            },
        };
    }

    constructor() : HashDataType("SendCloudServicePointOpeningHoursDataType") {
        addQoreFields(Fields);
    }
}

#! SendCloud service-point record
public class SendCloudServicePointDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Service point ID",
                "desc": "Unique SendCloud service-point ID",
            },
            "code": {
                "type": SoftStringOrNothingType,
                "display_name": "Code",
                "short_desc": "Service point code",
                "desc": "Carrier-specific service-point code",
            },
            "is_active": {
                "type": SoftBoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Whether the service point is currently active",
                "desc": "`True` if the service point is currently accepting drop-offs or pickups",
            },
            "shop_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Shop Type",
                "short_desc": "Type of shop / location",
                "desc": "Type of physical location (e.g., parcel shop, locker)",
            },
            "extra_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Extra Data",
                "short_desc": "Additional carrier-specific data",
                "desc": "Additional carrier-specific fields for this service point",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Display name",
                "desc": "Name of the service point",
            },
            "street": {
                "type": SoftStringOrNothingType,
                "display_name": "Street",
                "short_desc": "Street name",
                "desc": "Street name of the service point address",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "Street number",
                "desc": "House number of the service point address",
            },
            "postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal code",
                "desc": "Postal code of the service point",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City of the service point",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "ISO country code",
                "desc": "Two-letter ISO country code of the service point",
            },
            "postal_box": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Box",
                "short_desc": "Postal box",
                "desc": "Postal box (if any)",
            },
            "latitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Latitude",
                "short_desc": "Geographic latitude",
                "desc": "Latitude of the service point (string-encoded decimal)",
            },
            "longitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Longitude",
                "short_desc": "Geographic longitude",
                "desc": "Longitude of the service point (string-encoded decimal)",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Contact email",
                "desc": "Email address of the service point",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Contact phone",
                "desc": "Phone number of the service point",
            },
            "homepage": {
                "type": SoftStringOrNothingType,
                "display_name": "Homepage",
                "short_desc": "Homepage URL",
                "desc": "Homepage URL of the service point",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Carrier code",
                "desc": "Carrier code for the service point",
            },
            "distance": {
                "type": SoftIntOrNothingType,
                "display_name": "Distance (m)",
                "short_desc": "Distance from search origin in meters",
                "desc": "Distance in meters from the search center (when searching by near_* params)",
            },
            "opening_hours": {
                "type": new ListDataType("ServicePointOpeningHours",
                    new SendCloudServicePointOpeningHoursDataType(), True),
                "display_name": "Opening Hours",
                "short_desc": "Weekly opening hours",
                "desc": "List of opening-hours entries per day of the week",
            },
        };
    }

    constructor() : HashDataType("SendCloudServicePointDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
