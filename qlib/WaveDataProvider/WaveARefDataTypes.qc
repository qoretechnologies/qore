# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveARefDataTypes.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains reference data types for embedded objects in Wave responses
public namespace WaveDataProvider {

#! Province reference type (embedded in address responses)
public class WaveProvinceRefDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "type": StringOrNothingType,
                "display_name": "Code",
                "short_desc": "The province/state code",
                "desc": "The code of the province or state (e.g., CA, NY)",
                "example_value": "ON",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The province/state name",
                "desc": "The full name of the province or state",
                "example_value": "Ontario",
            },
        };
    }

    constructor() : HashDataType("ProvinceRef") {
        addQoreFields(Fields);
    }
}

#! Country reference type (embedded in address responses)
public class WaveCountryRefDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "type": StringOrNothingType,
                "display_name": "Code",
                "short_desc": "The country code",
                "desc": "The ISO country code (e.g., US, CA)",
                "example_value": "CA",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The country name",
                "desc": "The full name of the country",
                "example_value": "Canada",
            },
        };
    }

    constructor() : HashDataType("CountryRef") {
        addQoreFields(Fields);
    }
}

#! Address reference type (embedded in customer/business responses)
public class WaveAddressRefDataType inherits HashDataType {
    private {
        const Fields = {
            "addressLine1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "The first line of the address",
                "desc": "The first line of the street address",
                "example_value": "123 Main St",
            },
            "addressLine2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "The second line of the address",
                "desc": "The second line of the street address (suite, unit, etc.)",
                "example_value": "Suite 400",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city name",
                "example_value": "Toronto",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The postal or ZIP code",
                "desc": "The postal code or ZIP code",
                "example_value": "M5V 2T6",
            },
            "province": {
                "type": new WaveProvinceRefDataType(),
                "display_name": "Province",
                "short_desc": "The province or state",
                "desc": "The province or state of the address (code, name)",
            },
            "country": {
                "type": new WaveCountryRefDataType(),
                "display_name": "Country",
                "short_desc": "The country",
                "desc": "The country of the address (code, name)",
            },
        };
    }

    constructor() : HashDataType("AddressRef") {
        addQoreFields(Fields);
    }
}

#! Currency reference type (embedded in monetary responses)
public class WaveCurrencyRefDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "type": StringType,
                "display_name": "Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code",
                "example_value": "USD",
            },
        };
    }

    constructor() : HashDataType("CurrencyRef") {
        addQoreFields(Fields);
    }
}

#! Money reference type (embedded in financial responses)
public class WaveMoneyRefDataType inherits HashDataType {
    private {
        const Fields = {
            "value": {
                "type": StringOrNothingType,
                "display_name": "Value",
                "short_desc": "The monetary value",
                "desc": "The monetary value as a string (Wave returns money as string for precision)",
                "example_value": "100.00",
            },
            "currency": {
                "type": new WaveCurrencyRefDataType(),
                "display_name": "Currency",
                "short_desc": "The currency",
                "desc": "The currency of the monetary value (code)",
            },
        };
    }

    constructor() : HashDataType("MoneyRef") {
        addQoreFields(Fields);
    }
}

#! Customer reference type (embedded in invoice responses)
public class WaveCustomerRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The customer ID",
                "desc": "The unique identifier of the customer",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The customer name",
                "desc": "The name of the customer",
                "example_value": "Acme Corp",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer email",
                "desc": "The email address of the customer",
                "example_value": "billing@acme.com",
            },
        };
    }

    constructor() : HashDataType("CustomerRef") {
        addQoreFields(Fields);
    }
}

#! Product reference type (embedded in invoice item responses)
public class WaveProductRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The product ID",
                "desc": "The unique identifier of the product",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The name of the product or service",
                "example_value": "Consulting Services",
            },
        };
    }

    constructor() : HashDataType("ProductRef") {
        addQoreFields(Fields);
    }
}

#! Account reference type (embedded in invoice item responses)
public class WaveAccountRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The account ID",
                "desc": "The unique identifier of the account",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account name",
                "desc": "The name of the account",
                "example_value": "Sales Revenue",
            },
        };
    }

    constructor() : HashDataType("AccountRef") {
        addQoreFields(Fields);
    }
}

#! Account type reference type (embedded in account responses)
public class WaveAccountTypeRefDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account type name",
                "desc": "The display name of the account type",
                "example_value": "Asset",
            },
            "normalBalanceType": {
                "type": StringOrNothingType,
                "display_name": "Normal Balance Type",
                "short_desc": "The normal balance type",
                "desc": "The normal balance type of the account",
                "example_value": "DEBIT",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DEBIT", "desc": "Debit balance"},
                    <AllowedValueInfo>{"value": "CREDIT", "desc": "Credit balance"},
                ),
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "The account type value",
                "desc": "The enum value of the account type",
                "example_value": "ASSET",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ASSET", "desc": "Asset account"},
                    <AllowedValueInfo>{"value": "LIABILITY", "desc": "Liability account"},
                    <AllowedValueInfo>{"value": "INCOME", "desc": "Income account"},
                    <AllowedValueInfo>{"value": "EXPENSE", "desc": "Expense account"},
                    <AllowedValueInfo>{"value": "EQUITY", "desc": "Equity account"},
                ),
            },
        };
    }

    constructor() : HashDataType("AccountTypeRef") {
        addQoreFields(Fields);
    }
}

#! Account subtype reference type (embedded in account responses)
public class WaveAccountSubtypeRefDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account subtype name",
                "desc": "The display name of the account subtype",
                "example_value": "Cash and Bank",
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "The account subtype value",
                "desc": "The enum value of the account subtype",
                "example_value": "CASH_AND_BANK",
            },
        };
    }

    constructor() : HashDataType("AccountSubtypeRef") {
        addQoreFields(Fields);
    }
}

#! Sales tax reference type (embedded in product/invoice responses)
public class WaveSalesTaxRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The sales tax ID",
                "desc": "The unique identifier of the sales tax",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The sales tax name",
                "desc": "The name of the sales tax",
                "example_value": "HST",
            },
            "rate": {
                "type": FloatOrNothingType,
                "display_name": "Rate",
                "short_desc": "The tax rate",
                "desc": "The tax rate as a percentage",
                "example_value": 13.0,
            },
        };
    }

    constructor() : HashDataType("SalesTaxRef") {
        addQoreFields(Fields);
    }
}

#! Business type reference type (embedded in business responses)
public class WaveBusinessTypeRefDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The business type name",
                "desc": "The display name of the business type",
                "example_value": "Company",
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "The business type value",
                "desc": "The enum value of the business type",
                "example_value": "COMPANY",
            },
        };
    }

    constructor() : HashDataType("BusinessTypeRef") {
        addQoreFields(Fields);
    }
}

#! Shipping details reference type (embedded in customer responses)
public class WaveShippingDetailsRefDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The shipping contact name",
                "desc": "The name of the shipping contact",
                "example_value": "John Doe",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The shipping phone number",
                "desc": "The phone number for shipping contact",
                "example_value": "+1-555-0100",
            },
            "instructions": {
                "type": StringOrNothingType,
                "display_name": "Instructions",
                "short_desc": "Shipping instructions",
                "desc": "Special instructions for shipping",
                "example_value": "Leave at front door",
            },
            "address": {
                "type": new WaveAddressRefDataType(),
                "display_name": "Address",
                "short_desc": "The shipping address",
                "desc": "The shipping address details",
            },
        };
    }

    constructor() : HashDataType("ShippingDetailsRef") {
        addQoreFields(Fields);
    }
}

#! Invoice item tax reference type (embedded in invoice item responses)
public class WaveInvoiceItemTaxRefDataType inherits HashDataType {
    private {
        const Fields = {
            "salesTax": {
                "type": new WaveSalesTaxRefDataType(),
                "display_name": "Sales Tax",
                "short_desc": "The sales tax applied",
                "desc": "The sales tax applied to this invoice item (id, name)",
            },
            "amount": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Amount",
                "short_desc": "The tax amount",
                "desc": "The calculated tax amount for this invoice item",
            },
        };
    }

    constructor() : HashDataType("InvoiceItemTaxRef") {
        addQoreFields(Fields);
    }
}

#! Invoice item reference type (embedded in invoice responses)
public class WaveInvoiceItemRefDataType inherits HashDataType {
    private {
        const Fields = {
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The item description",
                "desc": "The description of the invoice line item",
                "example_value": "Web development services",
            },
            "quantity": {
                "type": FloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The item quantity",
                "desc": "The quantity of the item",
                "example_value": 10.0,
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "The unit price",
                "desc": "The price per unit of the item",
                "example_value": 150.0,
            },
            "subtotal": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Subtotal",
                "short_desc": "The line item subtotal",
                "desc": "The subtotal for this line item before taxes (value, currency)",
            },
            "total": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Total",
                "short_desc": "The line item total",
                "desc": "The total amount for this line item including taxes (value, currency)",
            },
            "product": {
                "type": new WaveProductRefDataType(),
                "display_name": "Product",
                "short_desc": "The product or service",
                "desc": "The product or service associated with this line item (id, name)",
            },
            "account": {
                "type": new WaveAccountRefDataType(),
                "display_name": "Account",
                "short_desc": "The account",
                "desc": "The account associated with this line item (id, name)",
            },
            "taxes": {
                "type": new ListDataType("InvoiceItemTaxRefList", new WaveInvoiceItemTaxRefDataType(), True),
                "display_name": "Taxes",
                "short_desc": "The taxes applied to this item",
                "desc": "The taxes applied to this invoice line item (salesTax, amount)",
            },
        };
    }

    constructor() : HashDataType("InvoiceItemRef") {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
