# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseAddressEntryDataType.qc Close address entry data type

/*  CloseAddressEntryDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close address entry data type for lead addresses
public class CloseAddressEntryDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "label": {
                "type": StringOrNothingType,
                "display_name": "Label",
                "short_desc": "The address label",
                "desc": "The label for the address (e.g., `business`, `mailing`)",
                "example_value": "business",
            },
            "address_1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "The first address line",
                "desc": "The first line of the street address",
                "example_value": "123 Main St",
            },
            "address_2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "The second address line",
                "desc": "The second line of the street address (e.g., suite, floor)",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city name",
                "example_value": "San Francisco",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The state or province",
                "desc": "The state or province name or abbreviation",
                "example_value": "CA",
            },
            "zipcode": {
                "type": StringOrNothingType,
                "display_name": "Zip Code",
                "short_desc": "The zip or postal code",
                "desc": "The zip or postal code",
                "example_value": "94105",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The country code",
                "desc": "The ISO 3166-1 alpha-2 country code",
                "example_value": "US",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
