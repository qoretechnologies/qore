# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelTimeOffTypeDataType.qc Deel time-off type record data type

/*  DeelTimeOffTypeDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Deel time-off type (entitlement) record type
public class DeelTimeOffTypeDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The time-off type ID",
                "desc": "The unique identifier of the time-off type",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The time-off type name",
                "desc": "The name of the time-off type (e.g., `Vacation`, `Sick Leave`)",
                "example_value": "Vacation",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The category of time-off",
                "desc": "The category of this time-off type "
                    "(e.g., `annual_leave`, `sick_leave`, `unpaid_leave`)",
                "example_value": "annual_leave",
            },
            "balance": {
                "type": NumberOrNothingType,
                "display_name": "Balance",
                "short_desc": "Available balance",
                "desc": "The available balance for this time-off type (in days)",
            },
            "accrued": {
                "type": NumberOrNothingType,
                "display_name": "Accrued",
                "short_desc": "Total accrued",
                "desc": "Total accrued time-off for this type (in days)",
            },
            "used": {
                "type": NumberOrNothingType,
                "display_name": "Used",
                "short_desc": "Amount used",
                "desc": "Amount of time-off already used (in days)",
            },
            "contract_id": {
                "type": StringOrNothingType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The ID of the contract this time-off entitlement belongs to",
            },
            "policy": {
                "type": AutoHashOrNothingType,
                "display_name": "Policy",
                "short_desc": "The time-off policy details",
                "desc": "Details about the accrual policy for this time-off type",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
