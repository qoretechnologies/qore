# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelContractDataType.qc Deel contract record data type

/*  DeelContractDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Deel contract record type
public class DeelContractDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The contract ID",
                "desc": "The unique identifier of the contract",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The contract title",
                "desc": "The title or name of the contract",
                "example_value": "Senior Software Engineer",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The contract type",
                "desc": "The type of contract (e.g., `pay_as_you_go`, `fixed_rate`, `peo_employee`)",
                "example_value": "fixed_rate",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The contract status",
                "desc": "The current status of the contract (e.g., `pending`, `active`, `ended`)",
                "example_value": "active",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The contract currency",
                "desc": "The ISO 4217 currency code for the contract",
                "example_value": "USD",
            },
            "start_date": {
                "type": StringOrNothingType,
                "display_name": "Start Date",
                "short_desc": "The contract start date",
                "desc": "The date the contract starts (YYYY-MM-DD format)",
                "example_value": "2026-01-01",
            },
            "end_date": {
                "type": StringOrNothingType,
                "display_name": "End Date",
                "short_desc": "The contract end date",
                "desc": "The date the contract ends, if applicable (YYYY-MM-DD format)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the contract was created",
                "desc": "The timestamp when the contract was created (ISO 8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the contract was last updated",
                "desc": "The timestamp when the contract was last updated (ISO 8601 format)",
            },
            "worker": {
                "type": AutoHashOrNothingType,
                "display_name": "Worker",
                "short_desc": "The worker details",
                "desc": "Information about the worker associated with the contract",
            },
            "client": {
                "type": AutoHashOrNothingType,
                "display_name": "Client",
                "short_desc": "The client details",
                "desc": "Information about the client organization",
            },
            "job": {
                "type": AutoHashOrNothingType,
                "display_name": "Job",
                "short_desc": "The job details",
                "desc": "Information about the job title and seniority",
            },
            "payment_cycle": {
                "type": StringOrNothingType,
                "display_name": "Payment Cycle",
                "short_desc": "The payment cycle",
                "desc": "The payment cycle for the contract (e.g., `weekly`, `monthly`)",
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "The contract amount",
                "desc": "The contract rate or salary amount",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The worker's country",
                "desc": "The ISO 3166-1 alpha-2 country code of the worker",
                "example_value": "US",
            },
            "scope_of_work": {
                "type": StringOrNothingType,
                "display_name": "Scope of Work",
                "short_desc": "Description of the work",
                "desc": "A description of the work to be performed",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
