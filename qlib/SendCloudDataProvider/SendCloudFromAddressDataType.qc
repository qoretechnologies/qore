# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudFromAddressDataType.qc SendCloud sender override address data type

/*  SendCloudFromAddressDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! From-address override (per-parcel sender) — uses SendCloud's from_* fields
public class SendCloudFromAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "from_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Name",
                "short_desc": "Sender full name",
                "desc": "Name of the sender. When omitted, SendCloud uses the default sender address",
            },
            "from_company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Company",
                "short_desc": "Sender company name",
                "desc": "Company name of the sender",
            },
            "from_address_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Street",
                "short_desc": "Sender primary address line",
                "desc": "Street address of the sender",
            },
            "from_address_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Address Line 2",
                "short_desc": "Sender secondary address line",
                "desc": "Apartment/suite line for the sender",
            },
            "from_house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender House Number",
                "short_desc": "Sender house number",
                "desc": "Street number of the sender",
            },
            "from_city": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender City",
                "short_desc": "Sender city",
                "desc": "City of the sender",
            },
            "from_postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Postal Code",
                "short_desc": "Sender postal/ZIP code",
                "desc": "Postal code of the sender",
            },
            "from_country": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Country",
                "short_desc": "Sender ISO country code",
                "desc": "Two-letter ISO country code of the sender",
                "ref_data": "countries",
            },
            "from_country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender State / Region",
                "short_desc": "Sender state or region",
                "desc": "State or region code of the sender",
            },
            "from_telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Phone",
                "short_desc": "Sender phone number",
                "desc": "Phone number of the sender",
            },
            "from_email": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Email",
                "short_desc": "Sender email address",
                "desc": "Email address of the sender",
            },
        };
    }

    constructor() : HashDataType("SendCloudFromAddressDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
