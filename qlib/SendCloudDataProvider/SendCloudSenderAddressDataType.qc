# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudSenderAddressDataType.qc SendCloud sender address response data type

/*  SendCloudSenderAddressDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! SendCloud sender address record (as returned by GET /user/addresses/sender)
public class SendCloudSenderAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Sender address ID",
                "desc": "Unique numeric identifier of the sender address",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Sender company name",
                "desc": "Company name on the sender address",
            },
            "contact_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Name",
                "short_desc": "Primary contact name",
                "desc": "Contact name on the sender address",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Sender email",
                "desc": "Email address of the sender",
            },
            "telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Telephone",
                "short_desc": "Sender phone",
                "desc": "Phone number of the sender",
            },
            "street": {
                "type": SoftStringOrNothingType,
                "display_name": "Street",
                "short_desc": "Street name",
                "desc": "Street name of the sender address",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "Street number",
                "desc": "House number of the sender address",
            },
            "postal_box": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Box",
                "short_desc": "Postal box",
                "desc": "Postal box number (if any)",
            },
            "postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal/ZIP code",
                "desc": "Postal code of the sender address",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City of the sender address",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "ISO 3166-1 alpha-2 country code",
                "desc": "Two-letter ISO country code of the sender address",
            },
            "country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "State / Region",
                "short_desc": "State or region code",
                "desc": "State or region code of the sender address",
            },
            "vat_number": {
                "type": SoftStringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "VAT number",
                "desc": "VAT number associated with the sender",
            },
            "eori": {
                "type": SoftStringOrNothingType,
                "display_name": "EORI",
                "short_desc": "EORI number",
                "desc": "EORI identifier for customs (EU)",
            },
            "label": {
                "type": SoftStringOrNothingType,
                "display_name": "Label",
                "short_desc": "Human-readable label",
                "desc": "Short label/nickname for this sender address",
            },
            "brand_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Brand ID",
                "short_desc": "Associated brand ID",
                "desc": "ID of the brand associated with this sender address, if any",
            },
        };
    }

    constructor() : HashDataType("SendCloudSenderAddressDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
