# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveProductDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Wave product record type
public class WaveProductDataType inherits HashDataType {
    private {
        #! Field definitions for Wave products
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the product",
                "desc": "The unique identifier of the product",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The name of the product or service",
                "example_value": "Consulting Services",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description",
                "desc": "The description of the product or service",
                "example_value": "Hourly consulting services",
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "The unit price",
                "desc": "The default unit price of the product or service",
                "example_value": 150.0,
            },
            "isSold": {
                "type": BoolOrNothingType,
                "display_name": "Is Sold",
                "short_desc": "Whether the product is sold",
                "desc": "If `True`, this product/service is used on invoices (income)",
                "example_value": True,
            },
            "isBought": {
                "type": BoolOrNothingType,
                "display_name": "Is Bought",
                "short_desc": "Whether the product is bought",
                "desc": "If `True`, this product/service is used on bills (expense)",
                "example_value": False,
            },
            "isArchived": {
                "type": BoolOrNothingType,
                "display_name": "Is Archived",
                "short_desc": "Whether the product is archived",
                "desc": "If `True`, the product has been archived and is no longer active",
                "example_value": False,
            },
            "defaultSalesTaxes": {
                "type": new ListDataType("SalesTaxRefList", new WaveSalesTaxRefDataType(), True),
                "display_name": "Default Sales Taxes",
                "short_desc": "The default sales taxes",
                "desc": "The default sales taxes applied to this product (array of tax objects with id, "
                    "name, rate)",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the product was created",
                "desc": "The date and time when the product was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the product was last modified",
                "desc": "The date and time when the product was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave product create input type
public class WaveProductCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating products
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the product in",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The product name (required)",
                "desc": "The name of the product or service",
                "example_value": "Consulting Services",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description",
                "desc": "The description of the product or service",
                "example_value": "Hourly consulting services",
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "The unit price",
                "desc": "The default unit price of the product or service",
                "example_value": 150.0,
            },
            "incomeAccountId": {
                "type": StringOrNothingType,
                "display_name": "Income Account ID",
                "short_desc": "The income account for this product",
                "desc": "The ID of the income account to use when this product is sold (at least one of "
                    "incomeAccountId or expenseAccountId is required)",
            },
            "expenseAccountId": {
                "type": StringOrNothingType,
                "display_name": "Expense Account ID",
                "short_desc": "The expense account for this product",
                "desc": "The ID of the expense account to use when this product is purchased (at least one of "
                    "incomeAccountId or expenseAccountId is required)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave product update input type
public class WaveProductUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating products
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The product ID (required)",
                "desc": "The unique identifier of the product to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The new name of the product or service",
                "example_value": "Consulting Services",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description",
                "desc": "The new description of the product or service",
                "example_value": "Hourly consulting services",
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "The unit price",
                "desc": "The new default unit price of the product or service",
                "example_value": 175.0,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
