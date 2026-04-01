# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipDetectCourierRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Request type for detecting a courier from a tracking number
public class AftershipDetectCourierRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tracking_number": {
                "display_name": "Tracking Number",
                "short_desc": "Tracking number to identify",
                "desc": "Tracking number to identify the courier for",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "1Z999AA10123456784",
            },
            "tracking_account_number": {
                "display_name": "Tracking Account Number",
                "short_desc": "Carrier account number",
                "desc": "Carrier account number to help narrow courier detection",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_key": {
                "display_name": "Tracking Key",
                "short_desc": "Tracking credential",
                "desc": "Tracking credential to help narrow courier detection",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_ship_date": {
                "display_name": "Ship Date",
                "short_desc": "Ship date",
                "desc": "Ship date to help narrow courier detection",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "origin_country_region": {
                "display_name": "Origin Country",
                "short_desc": "Origin country code (ISO Alpha-3)",
                "desc": "ISO Alpha-3 origin country code to improve detection accuracy",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_country_region": {
                "display_name": "Destination Country",
                "short_desc": "Destination country code (ISO Alpha-3)",
                "desc": "ISO Alpha-3 destination country code to improve detection accuracy",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "slug": {
                "display_name": "Courier Slugs",
                "short_desc": "Filter by courier codes",
                "desc": "List of courier codes to narrow detection (e.g., `fedex`, `ups`)",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
            },
            "destination_postal_code": {
                "display_name": "Destination Postal Code",
                "short_desc": "Destination postal code",
                "desc": "Destination postal code to improve detection accuracy",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_state": {
                "display_name": "Destination State",
                "short_desc": "Destination state or province",
                "desc": "State or province of destination to improve detection accuracy",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "slug_group": {
                "display_name": "Courier Group",
                "short_desc": "Courier group code",
                "desc": "Courier group code for multi-slug detection (e.g., `fedex-group`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipDetectCourierRequestDataType = new AftershipDetectCourierRequestDataType();
}
