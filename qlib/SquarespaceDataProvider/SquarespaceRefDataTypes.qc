# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceRefDataTypes.qc defines embedded/reference data types for the SquarespaceDataProvider module

/*  SquarespaceRefDataTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains reference data types for embedded objects in Squarespace responses
public namespace SquarespaceDataProvider {

#! Money type (currency + value)
public class SquarespaceMoneyDataType inherits HashDataType {
    public {
        const Fields = {
            "currency": {
                "type": StringType,
                "display_name": "Currency",
                "short_desc": "ISO currency code",
                "desc": "The ISO 4217 currency code (e.g., USD, EUR)",
                "required": True,
                "example_value": "USD",
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "Monetary amount",
                "desc": "The monetary amount as a string (e.g., \"9.99\")",
                "required": True,
                "example_value": "9.99",
            },
        };
    }

    constructor() : HashDataType("Money") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "Money") {
        addQoreFields(Fields);
    }
}

#! Pricing type for product variants (all product types)
public class SquarespaceBasePricingDataType inherits HashDataType {
    public {
        const Fields = {
            "basePrice": {
                "type": new SquarespaceMoneyDataType(),
                "display_name": "Base Price",
                "short_desc": "The base price of the variant",
                "desc": "The base price of the variant (currency + value)",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("BasePricing") {
        addQoreFields(Fields);
    }
}

#! Pricing type for PHYSICAL and SERVICE variants (includes sale pricing)
public class SquarespaceFullPricingDataType inherits HashDataType {
    public {
        const Fields = {
            "basePrice": {
                "type": new SquarespaceMoneyDataType(),
                "display_name": "Base Price",
                "short_desc": "The base price of the variant",
                "desc": "The base price of the variant (currency + value)",
                "required": True,
            },
            "onSale": {
                "type": SoftBoolOrNothingType,
                "display_name": "On Sale",
                "short_desc": "Whether the variant is on sale",
                "desc": "Whether the variant is on sale; if `true`, `salePrice` is required",
            },
            "salePrice": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Sale Price",
                "short_desc": "The sale price of the variant",
                "desc": "The sale price of the variant (currency + value); "
                    "required if `onSale` is `true`",
            },
        };
    }

    constructor() : HashDataType("FullPricing") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "FullPricing") {
        addQoreFields(Fields);
    }
}

#! Stock type for PHYSICAL and SERVICE variants
public class SquarespaceStockDataType inherits HashDataType {
    public {
        const Fields = {
            "quantity": {
                "type": SoftIntOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The stock quantity",
                "desc": "The available stock quantity for the variant",
                "example_value": 10,
            },
            "unlimited": {
                "type": SoftBoolOrNothingType,
                "display_name": "Unlimited",
                "short_desc": "Whether stock is unlimited",
                "desc": "Whether the variant has unlimited stock",
            },
        };
    }

    constructor() : HashDataType("Stock") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "Stock") {
        addQoreFields(Fields);
    }
}

#! Weight type for PHYSICAL variant shipping measurements
public class SquarespaceWeightDataType inherits HashDataType {
    public {
        const Fields = {
            "unit": {
                "type": StringType,
                "display_name": "Unit",
                "short_desc": "Weight unit (POUND or KILOGRAM)",
                "desc": "The weight unit: POUND or KILOGRAM",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "POUND", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "KILOGRAM", "desc": "Kilograms"},
                ),
            },
            "value": {
                "type": SoftFloatType,
                "display_name": "Value",
                "short_desc": "The weight value",
                "desc": "The weight value",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("Weight") {
        addQoreFields(Fields);
    }
}

#! Dimensions type for PHYSICAL variant shipping measurements
public class SquarespaceDimensionsDataType inherits HashDataType {
    public {
        const Fields = {
            "unit": {
                "type": StringType,
                "display_name": "Unit",
                "short_desc": "Dimension unit (INCH or CENTIMETER)",
                "desc": "The dimension unit: INCH or CENTIMETER",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "INCH", "desc": "Inches"},
                    <AllowedValueInfo>{"value": "CENTIMETER", "desc": "Centimeters"},
                ),
            },
            "length": {
                "type": SoftFloatType,
                "display_name": "Length",
                "short_desc": "The length",
                "desc": "The length dimension",
                "required": True,
            },
            "width": {
                "type": SoftFloatType,
                "display_name": "Width",
                "short_desc": "The width",
                "desc": "The width dimension",
                "required": True,
            },
            "height": {
                "type": SoftFloatType,
                "display_name": "Height",
                "short_desc": "The height",
                "desc": "The height dimension",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("Dimensions") {
        addQoreFields(Fields);
    }
}

#! Shipping measurements type for PHYSICAL variants
public class SquarespaceShippingMeasurementsDataType inherits HashDataType {
    public {
        const Fields = {
            "weight": {
                "type": new SquarespaceWeightDataType(),
                "display_name": "Weight",
                "short_desc": "The shipping weight",
                "desc": "The shipping weight (unit + value)",
            },
            "dimensions": {
                "type": new SquarespaceDimensionsDataType(),
                "display_name": "Dimensions",
                "short_desc": "The shipping dimensions",
                "desc": "The shipping dimensions (unit, length, width, height)",
            },
        };
    }

    constructor() : HashDataType("ShippingMeasurements") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "ShippingMeasurements") {
        addQoreFields(Fields);
    }
}

#! Variant type for GIFT_CARD products (minimal: sku + basePrice only)
public class SquarespaceGiftCardVariantDataType inherits HashDataType {
    public {
        const Fields = {
            "sku": {
                "type": StringType,
                "display_name": "SKU",
                "short_desc": "The variant SKU",
                "desc": "The stock keeping unit identifier (max 60 chars, must be unique)",
                "required": True,
            },
            "pricing": {
                "type": new SquarespaceBasePricingDataType(),
                "display_name": "Pricing",
                "short_desc": "The variant pricing",
                "desc": "The variant pricing (basePrice only for gift cards)",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("GiftCardVariant") {
        addQoreFields(Fields);
    }
}

#! Variant type for SERVICE products (sku, pricing with sale, stock, attributes)
public class SquarespaceServiceVariantDataType inherits HashDataType {
    public {
        const Fields = {
            "sku": {
                "type": StringType,
                "display_name": "SKU",
                "short_desc": "The variant SKU",
                "desc": "The stock keeping unit identifier (max 60 chars, must be unique)",
                "required": True,
            },
            "pricing": {
                "type": new SquarespaceFullPricingDataType(),
                "display_name": "Pricing",
                "short_desc": "The variant pricing",
                "desc": "The variant pricing (basePrice, onSale, salePrice)",
                "required": True,
            },
            "stock": {
                "type": new SquarespaceStockDataType(AutoHashOrNothingType),
                "display_name": "Stock",
                "short_desc": "The stock information",
                "desc": "The stock information (quantity, unlimited)",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "The variant attributes",
                "desc": "Key-value pairs matching the product's variantAttributes "
                    "(e.g., {\"Size\": \"Large\", \"Color\": \"Red\"})",
            },
        };
    }

    constructor() : HashDataType("ServiceVariant") {
        addQoreFields(Fields);
    }
}

#! Variant type for PHYSICAL products (full: sku, pricing, stock, attributes, shipping)
public class SquarespacePhysicalVariantDataType inherits HashDataType {
    public {
        const Fields = {
            "sku": {
                "type": StringType,
                "display_name": "SKU",
                "short_desc": "The variant SKU",
                "desc": "The stock keeping unit identifier (max 60 chars, must be unique)",
                "required": True,
            },
            "pricing": {
                "type": new SquarespaceFullPricingDataType(),
                "display_name": "Pricing",
                "short_desc": "The variant pricing",
                "desc": "The variant pricing (basePrice, onSale, salePrice)",
                "required": True,
            },
            "stock": {
                "type": new SquarespaceStockDataType(AutoHashOrNothingType),
                "display_name": "Stock",
                "short_desc": "The stock information",
                "desc": "The stock information (quantity, unlimited)",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "The variant attributes",
                "desc": "Key-value pairs matching the product's variantAttributes "
                    "(e.g., {\"Size\": \"Large\", \"Color\": \"Red\"})",
            },
            "shippingMeasurements": {
                "type": new SquarespaceShippingMeasurementsDataType(AutoHashOrNothingType),
                "display_name": "Shipping Measurements",
                "short_desc": "The shipping measurements",
                "desc": "The shipping measurements (weight + dimensions)",
            },
        };
    }

    constructor() : HashDataType("PhysicalVariant") {
        addQoreFields(Fields);
    }
}

#! Address type for billing and shipping addresses
public class SquarespaceAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the recipient",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the recipient",
            },
            "address1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "Street address line 1",
                "desc": "The first line of the street address",
            },
            "address2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Street address line 2",
                "desc": "The second line of the street address",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province",
            },
            "countryCode": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "ISO country code",
                "desc": "The ISO country code",
                "example_value": "US",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal or ZIP code",
                "desc": "The postal or ZIP code",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number",
            },
        };
    }

    constructor() : HashDataType("Address") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "Address") {
        addQoreFields(Fields);
    }
}

#! SEO options type for product SEO configuration
public class SquarespaceSeoOptionsDataType inherits HashDataType {
    public {
        const Fields = {
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "SEO title",
                "desc": "The SEO title for the product",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "SEO description",
                "desc": "The SEO meta description for the product",
            },
        };
    }

    constructor() : HashDataType("SeoOptions") {
        addQoreFields(Fields);
    }

    #! Creates an or-nothing variant for use in optional field contexts
    constructor(Type base_type) : HashDataType(base_type, "SeoOptions") {
        addQoreFields(Fields);
    }
}

#! Line item type for order line items
public class SquarespaceLineItemDataType inherits HashDataType {
    public {
        const Fields = {
            "lineItemType": {
                "type": StringType,
                "display_name": "Line Item Type",
                "short_desc": "The type of line item",
                "desc": "The type of line item (e.g., `PHYSICAL`, `DIGITAL`, `SERVICE`, `GIFT_CARD`)",
                "required": True,
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "The quantity ordered",
                "desc": "The quantity of this line item",
                "required": True,
                "example_value": 1,
            },
            "unitPricePaid": {
                "type": new SquarespaceMoneyDataType(),
                "display_name": "Unit Price Paid",
                "short_desc": "The unit price paid",
                "desc": "The unit price paid for this line item",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("LineItem") {
        addQoreFields(Fields);
    }
}

#! Shipment type for order fulfillment shipments
public class SquarespaceShipmentDataType inherits HashDataType {
    public {
        const Fields = {
            "shipDate": {
                "type": SoftDateType,
                "display_name": "Ship Date",
                "short_desc": "The shipment date",
                "desc": "The date the shipment was sent",
                "required": True,
            },
            "carrierName": {
                "type": StringOrNothingType,
                "display_name": "Carrier Name",
                "short_desc": "The shipping carrier name",
                "desc": "The name of the shipping carrier",
            },
            "service": {
                "type": StringOrNothingType,
                "display_name": "Service",
                "short_desc": "The shipping service",
                "desc": "The shipping service used",
            },
            "trackingNumber": {
                "type": StringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "The tracking number",
                "desc": "The shipment tracking number",
            },
            "trackingUrl": {
                "type": StringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "The tracking URL",
                "desc": "The URL for tracking the shipment",
            },
        };
    }

    constructor() : HashDataType("Shipment") {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
