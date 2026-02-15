# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveBusinessDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave business record type (read-only)
public class WaveBusinessDataType inherits HashDataType {
    private {
        #! Field definitions for Wave businesses
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the business",
                "desc": "The unique identifier of the business",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The business name",
                "desc": "The name of the business",
                "example_value": "Acme Inc.",
            },
            "type": {
                "type": new WaveBusinessTypeRefDataType(),
                "display_name": "Type",
                "short_desc": "The business type",
                "desc": "The type of the business (name, value)",
            },
            "subtype": {
                "type": new WaveBusinessTypeRefDataType(),
                "display_name": "Subtype",
                "short_desc": "The business subtype",
                "desc": "The subtype of the business (name, value)",
            },
            "currency": {
                "type": new WaveCurrencyRefDataType(),
                "display_name": "Currency",
                "short_desc": "The business currency",
                "desc": "The default currency of the business (code)",
            },
            "address": {
                "type": new WaveAddressRefDataType(),
                "display_name": "Address",
                "short_desc": "The business address",
                "desc": "The address of the business",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The business phone number",
                "desc": "The phone number of the business",
                "example_value": "+1-555-0100",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the business was created",
                "desc": "The date and time when the business was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the business was last modified",
                "desc": "The date and time when the business was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
