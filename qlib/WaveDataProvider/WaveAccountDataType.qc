# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveAccountDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Wave account record type
public class WaveAccountDataType inherits HashDataType {
    private {
        #! Field definitions for Wave accounts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the account",
                "desc": "The unique identifier of the account",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account name",
                "desc": "The name of the account",
                "example_value": "Cash on Hand",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The account description",
                "desc": "The description of the account",
                "example_value": "Physical cash available",
            },
            "displayId": {
                "type": StringOrNothingType,
                "display_name": "Display ID",
                "short_desc": "The account display ID",
                "desc": "The display identifier of the account (account number)",
                "example_value": "1000",
            },
            "type": {
                "type": new WaveAccountTypeRefDataType(),
                "display_name": "Type",
                "short_desc": "The account type",
                "desc": "The type of the account (name, normalBalanceType, value)",
            },
            "subtype": {
                "type": new WaveAccountSubtypeRefDataType(),
                "display_name": "Subtype",
                "short_desc": "The account subtype",
                "desc": "The subtype of the account (name, value)",
            },
            "currency": {
                "type": new WaveCurrencyRefDataType(),
                "display_name": "Currency",
                "short_desc": "The account currency",
                "desc": "The currency of the account (code)",
            },
            "isArchived": {
                "type": BoolOrNothingType,
                "display_name": "Is Archived",
                "short_desc": "Whether the account is archived",
                "desc": "If true, the account has been archived and is no longer active",
                "example_value": False,
            },
            "sequence": {
                "type": IntOrNothingType,
                "display_name": "Sequence",
                "short_desc": "The account sequence number",
                "desc": "The sequence number used for ordering accounts",
                "example_value": 1,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave account create input type
public class WaveAccountCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating accounts
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the account in",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account name (required)",
                "desc": "The name of the account",
                "example_value": "Petty Cash",
                "required": True,
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The account type (required)",
                "desc": "The account type enum value (ASSET, LIABILITY, INCOME, EXPENSE, EQUITY)",
                "example_value": "ASSET",
                "required": True,
            },
            "subtype": {
                "type": StringOrNothingType,
                "display_name": "Subtype",
                "short_desc": "The account subtype",
                "desc": "The account subtype enum value (e.g., CASH_AND_BANK, ACCOUNTS_RECEIVABLE)",
                "example_value": "CASH_AND_BANK",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the account (e.g., USD, CAD)",
                "example_value": "USD",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The account description",
                "desc": "The description of the account",
                "example_value": "Petty cash for office expenses",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
