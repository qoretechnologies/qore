# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelLegalEntityDataType.qc Deel legal entity record data type

/*  DeelLegalEntityDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel legal entity record type
public class DeelLegalEntityDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The legal entity ID",
                "desc": "The unique identifier of the legal entity",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The legal entity name",
                "desc": "The registered name of the legal entity",
                "example_value": "Acme Corporation Inc.",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The entity type",
                "desc": "The type of legal entity (e.g., `llc`, `corporation`, `sole_proprietor`)",
                "example_value": "corporation",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The entity's country",
                "desc": "The ISO 3166-1 alpha-2 country code where the entity is registered",
                "example_value": "US",
            },
            "registration_number": {
                "type": StringOrNothingType,
                "display_name": "Registration Number",
                "short_desc": "The registration number",
                "desc": "The official registration number of the legal entity",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The entity status",
                "desc": "The current status of the legal entity (e.g., `active`, `inactive`)",
                "example_value": "active",
            },
            "address": {
                "type": AutoHashOrNothingType,
                "display_name": "Address",
                "short_desc": "The entity's address",
                "desc": "The registered address of the legal entity",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the entity was created",
                "desc": "The timestamp when the legal entity was added to Deel (ISO 8601 format)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
