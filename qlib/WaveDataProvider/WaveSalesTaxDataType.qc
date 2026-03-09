# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveSalesTaxDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Wave sales tax record type
public class WaveSalesTaxDataType inherits HashDataType {
    private {
        #! Field definitions for Wave sales taxes
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the sales tax",
                "desc": "The unique identifier of the sales tax",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The sales tax name",
                "desc": "The name of the sales tax",
                "example_value": "HST",
            },
            "abbreviation": {
                "type": StringOrNothingType,
                "display_name": "Abbreviation",
                "short_desc": "The tax abbreviation",
                "desc": "The short abbreviation for the sales tax",
                "example_value": "HST",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The tax description",
                "desc": "The description of the sales tax",
                "example_value": "Harmonized Sales Tax",
            },
            "taxNumber": {
                "type": StringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "The tax registration number",
                "desc": "The tax registration number associated with this sales tax",
                "example_value": "123456789RT0001",
            },
            "rate": {
                "type": FloatOrNothingType,
                "display_name": "Rate",
                "short_desc": "The tax rate",
                "desc": "The tax rate as a percentage",
                "example_value": 13.0,
            },
            "isCompound": {
                "type": BoolOrNothingType,
                "display_name": "Is Compound",
                "short_desc": "Whether this is a compound tax",
                "desc": "If `True`, this tax is calculated on top of other taxes (compound tax)",
                "example_value": False,
            },
            "isRecoverable": {
                "type": BoolOrNothingType,
                "display_name": "Is Recoverable",
                "short_desc": "Whether the tax is recoverable",
                "desc": "If `True`, this tax can be recovered (e.g., as an input tax credit)",
                "example_value": True,
            },
            "isArchived": {
                "type": BoolOrNothingType,
                "display_name": "Is Archived",
                "short_desc": "Whether the tax is archived",
                "desc": "If `True`, the sales tax has been archived and is no longer active",
                "example_value": False,
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the sales tax was created",
                "desc": "The date and time when the sales tax was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the sales tax was last modified",
                "desc": "The date and time when the sales tax was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave sales tax create input type
public class WaveSalesTaxCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating sales taxes
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the sales tax in",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The sales tax name (required)",
                "desc": "The name of the sales tax",
                "example_value": "HST",
                "required": True,
            },
            "rate": {
                "type": FloatType,
                "display_name": "Rate",
                "short_desc": "The tax rate (required)",
                "desc": "The tax rate as a percentage",
                "example_value": 13.0,
                "required": True,
            },
            "abbreviation": {
                "type": StringOrNothingType,
                "display_name": "Abbreviation",
                "short_desc": "The tax abbreviation",
                "desc": "The short abbreviation for the sales tax",
                "example_value": "HST",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The tax description",
                "desc": "The description of the sales tax",
                "example_value": "Harmonized Sales Tax",
            },
            "taxNumber": {
                "type": StringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "The tax registration number",
                "desc": "The tax registration number associated with this sales tax",
                "example_value": "123456789RT0001",
            },
            "isCompound": {
                "type": BoolOrNothingType,
                "display_name": "Is Compound",
                "short_desc": "Whether this is a compound tax",
                "desc": "If `True`, this tax is calculated on top of other taxes (compound tax)",
                "example_value": False,
            },
            "isRecoverable": {
                "type": BoolOrNothingType,
                "display_name": "Is Recoverable",
                "short_desc": "Whether the tax is recoverable",
                "desc": "If `True`, this tax can be recovered (e.g., as an input tax credit)",
                "example_value": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
