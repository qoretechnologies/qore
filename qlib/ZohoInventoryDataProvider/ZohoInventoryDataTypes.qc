# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryDataTypes.qc Zoho Inventory structured data types for all resources

/*  ZohoInventoryDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

# ========== COMMON TYPES ==========

#! Common delete response type
public class ZohoInventoryDeleteResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "type": SoftIntType,
                "display_name": "Code",
                "short_desc": "Response code",
                "desc": "Response code (0 for success)",
            },
            "message": {
                "type": SoftStringType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "Response message from the API",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryDeleteResponse") {
        addQoreFields(Fields);
    }
}

# ========== CONTACT TYPES ==========

#! Zoho Inventory contact record type
public class ZohoInventoryContactDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "Unique contact identifier",
                "desc": "The unique identifier of the contact",
            },
            "contact_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Name",
                "short_desc": "Name of the contact",
                "desc": "The display name of the contact",
                "example_value": "John Smith",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name of the contact",
                "example_value": "Acme Corporation",
            },
            "contact_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Type",
                "short_desc": "Type of contact",
                "desc": "The type of the contact",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "customer", "desc": "Customer contact"},
                    <AllowedValueInfo>{"value": "vendor", "desc": "Vendor contact"},
                ),
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Contact status",
                "desc": "The status of the contact (active or inactive)",
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms",
                "desc": "Net payment terms in days",
            },
            "currency_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency identifier for the contact",
            },
            "currency_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "example_value": "USD",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "Notes about the contact",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact",
                "example_value": "john.smith@acme.com",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website of the contact",
            },
            "billing_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address of the contact (hash with address, city, state, zip, country, fax)",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address of the contact (hash with address, city, state, zip, country, fax)",
            },
            "has_transaction": {
                "type": SoftBoolOrNothingType,
                "display_name": "Has Transaction",
                "short_desc": "Has transactions",
                "desc": "Whether the contact has any transactions",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the contact was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the contact was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContact") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory contact create request type
public class ZohoInventoryContactCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_name": {
                "type": SoftStringType,
                "display_name": "Contact Name",
                "short_desc": "Name of the contact",
                "desc": "The display name of the contact",
                "example_value": "John Smith",
                "required": True,
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name of the contact",
            },
            "contact_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Type",
                "short_desc": "Type of contact",
                "desc": "The type of the contact",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "customer", "desc": "Customer contact"},
                    <AllowedValueInfo>{"value": "vendor", "desc": "Vendor contact"},
                ),
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms in days",
                "desc": "Net payment terms in days",
            },
            "currency_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency identifier for the contact",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "Notes about the contact",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website of the contact",
            },
            "billing_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address (hash with address, city, state, zip, country, fax)",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address (hash with address, city, state, zip, country, fax)",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContactCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory contact update request type
public class ZohoInventoryContactUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_id": {
                "type": SoftStringType,
                "display_name": "Contact ID",
                "short_desc": "The contact to update",
                "desc": "The ID of the contact to update",
                "required": True,
            },
            "contact_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Name",
                "short_desc": "Name of the contact",
                "desc": "The new display name of the contact",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The new company name of the contact",
            },
            "contact_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Type",
                "short_desc": "Type of contact",
                "desc": "The type of the contact",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "customer", "desc": "Customer contact"},
                    <AllowedValueInfo>{"value": "vendor", "desc": "Vendor contact"},
                ),
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms in days",
                "desc": "Net payment terms in days",
            },
            "currency_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency identifier for the contact",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "Notes about the contact",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website of the contact",
            },
            "billing_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address (hash with address, city, state, zip, country, fax)",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address (hash with address, city, state, zip, country, fax)",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContactUpdate") {
        addQoreFields(Fields);
    }
}

# ========== CONTACT PERSON TYPES ==========

#! Zoho Inventory contact person record type
public class ZohoInventoryContactPersonDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_person_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Person ID",
                "short_desc": "Unique contact person identifier",
                "desc": "The unique identifier of the contact person",
            },
            "contact_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "Parent contact ID",
                "desc": "The ID of the parent contact",
            },
            "salutation": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Salutation",
                "desc": "The salutation for the contact person (e.g., Mr., Mrs.)",
            },
            "first_name": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the contact person",
                "example_value": "John",
            },
            "last_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the contact person",
                "example_value": "Smith",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact person",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact person",
            },
            "mobile": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile number",
                "desc": "The mobile number of the contact person",
            },
            "designation": {
                "type": SoftStringOrNothingType,
                "display_name": "Designation",
                "short_desc": "Job title",
                "desc": "The designation or job title of the contact person",
            },
            "department": {
                "type": SoftStringOrNothingType,
                "display_name": "Department",
                "short_desc": "Department",
                "desc": "The department of the contact person",
            },
            "is_primary_contact": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Primary",
                "short_desc": "Whether this is the primary contact",
                "desc": "Whether this is the primary contact person",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContactPerson") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory contact person create request type
public class ZohoInventoryContactPersonCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_id": {
                "type": SoftStringType,
                "display_name": "Contact ID",
                "short_desc": "Parent contact ID",
                "desc": "The ID of the parent contact",
                "required": True,
            },
            "first_name": {
                "type": SoftStringType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the contact person",
                "required": True,
            },
            "last_name": {
                "type": SoftStringType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the contact person",
                "required": True,
            },
            "salutation": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Salutation",
                "desc": "The salutation for the contact person (e.g., Mr., Mrs.)",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact person",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact person",
            },
            "mobile": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile number",
                "desc": "The mobile number of the contact person",
            },
            "designation": {
                "type": SoftStringOrNothingType,
                "display_name": "Designation",
                "short_desc": "Job title",
                "desc": "The designation or job title of the contact person",
            },
            "department": {
                "type": SoftStringOrNothingType,
                "display_name": "Department",
                "short_desc": "Department",
                "desc": "The department of the contact person",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContactPersonCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory contact person update request type
public class ZohoInventoryContactPersonUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_id": {
                "type": SoftStringType,
                "display_name": "Contact ID",
                "short_desc": "Parent contact ID",
                "desc": "The ID of the parent contact",
                "required": True,
            },
            "contact_person_id": {
                "type": SoftStringType,
                "display_name": "Contact Person ID",
                "short_desc": "Contact person to update",
                "desc": "The ID of the contact person to update",
                "required": True,
            },
            "first_name": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The new first name of the contact person",
            },
            "last_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The new last name of the contact person",
            },
            "salutation": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Salutation",
                "desc": "The new salutation for the contact person",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The new email address of the contact person",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The new phone number of the contact person",
            },
            "mobile": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile number",
                "desc": "The new mobile number of the contact person",
            },
            "designation": {
                "type": SoftStringOrNothingType,
                "display_name": "Designation",
                "short_desc": "Job title",
                "desc": "The new designation or job title",
            },
            "department": {
                "type": SoftStringOrNothingType,
                "display_name": "Department",
                "short_desc": "Department",
                "desc": "The new department of the contact person",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryContactPersonUpdate") {
        addQoreFields(Fields);
    }
}

# ========== ITEM TYPES ==========

#! Zoho Inventory item record type
public class ZohoInventoryItemDataType inherits HashDataType {
    private {
        const Fields = {
            "item_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Item ID",
                "short_desc": "Unique item identifier",
                "desc": "The unique identifier of the item",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The name of the item",
                "example_value": "Widget Pro",
            },
            "rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Rate",
                "short_desc": "Selling price",
                "desc": "The selling price of the item",
            },
            "purchase_rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Purchase Rate",
                "short_desc": "Purchase price",
                "desc": "The purchase price of the item",
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit of the item",
                "example_value": "WDG-001",
            },
            "unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement for the item",
            },
            "item_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Item Type",
                "short_desc": "Type of item",
                "desc": "The type of the item",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "inventory", "desc": "Inventory item"},
                    <AllowedValueInfo>{"value": "sales", "desc": "Sales item"},
                    <AllowedValueInfo>{"value": "purchases", "desc": "Purchases item"},
                    <AllowedValueInfo>{"value": "sales_and_purchases", "desc": "Sales and purchases item"},
                    <AllowedValueInfo>{"value": "service", "desc": "Service item"},
                ),
            },
            "product_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Product Type",
                "short_desc": "Product type",
                "desc": "The product type of the item (goods or service)",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Sales description",
                "desc": "The sales description of the item",
            },
            "purchase_description": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Description",
                "short_desc": "Purchase description",
                "desc": "The purchase description of the item",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Item status",
                "desc": "The status of the item (active or inactive)",
            },
            "reorder_level": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Level",
                "short_desc": "Reorder level",
                "desc": "The reorder level/point for the item",
            },
            "stock_on_hand": {
                "type": SoftFloatOrNothingType,
                "display_name": "Stock On Hand",
                "short_desc": "Current stock",
                "desc": "The current stock on hand",
            },
            "available_stock": {
                "type": SoftFloatOrNothingType,
                "display_name": "Available Stock",
                "short_desc": "Available stock",
                "desc": "The available stock quantity",
            },
            "vendor_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor ID",
                "short_desc": "Preferred vendor ID",
                "desc": "The ID of the preferred vendor for the item",
            },
            "vendor_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor Name",
                "short_desc": "Preferred vendor name",
                "desc": "The name of the preferred vendor",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the item was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the item was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItem") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory item create request type
public class ZohoInventoryItemCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "name": {
                "type": SoftStringType,
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The name of the item",
                "example_value": "Widget Pro",
                "required": True,
            },
            "rate": {
                "type": SoftFloatType,
                "display_name": "Rate",
                "short_desc": "Selling price",
                "desc": "The selling price of the item",
                "required": True,
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit of the item",
            },
            "unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement for the item",
            },
            "item_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Item Type",
                "short_desc": "Type of item",
                "desc": "The type of the item",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "inventory", "desc": "Inventory item"},
                    <AllowedValueInfo>{"value": "sales", "desc": "Sales item"},
                    <AllowedValueInfo>{"value": "purchases", "desc": "Purchases item"},
                    <AllowedValueInfo>{"value": "sales_and_purchases", "desc": "Sales and purchases item"},
                    <AllowedValueInfo>{"value": "service", "desc": "Service item"},
                ),
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Sales description",
                "desc": "The sales description of the item",
            },
            "purchase_rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Purchase Rate",
                "short_desc": "Purchase price",
                "desc": "The purchase price of the item",
            },
            "purchase_description": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Description",
                "short_desc": "Purchase description",
                "desc": "The purchase description of the item",
            },
            "reorder_level": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Level",
                "short_desc": "Reorder level",
                "desc": "The reorder level/point for the item",
            },
            "vendor_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor ID",
                "short_desc": "Preferred vendor ID",
                "desc": "The ID of the preferred vendor for the item",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory item update request type
public class ZohoInventoryItemUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "item_id": {
                "type": SoftStringType,
                "display_name": "Item ID",
                "short_desc": "The item to update",
                "desc": "The ID of the item to update",
                "required": True,
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The new name of the item",
            },
            "rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Rate",
                "short_desc": "Selling price",
                "desc": "The new selling price of the item",
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The new stock keeping unit",
            },
            "unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The new unit of measurement",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Sales description",
                "desc": "The new sales description of the item",
            },
            "purchase_rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Purchase Rate",
                "short_desc": "Purchase price",
                "desc": "The new purchase price of the item",
            },
            "purchase_description": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Description",
                "short_desc": "Purchase description",
                "desc": "The new purchase description of the item",
            },
            "reorder_level": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Level",
                "short_desc": "Reorder level",
                "desc": "The new reorder level/point for the item",
            },
            "vendor_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor ID",
                "short_desc": "Preferred vendor ID",
                "desc": "The new preferred vendor ID for the item",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemUpdate") {
        addQoreFields(Fields);
    }
}

# ========== ITEM GROUP TYPES ==========

#! Zoho Inventory item group record type
public class ZohoInventoryItemGroupDataType inherits HashDataType {
    private {
        const Fields = {
            "group_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Group ID",
                "short_desc": "Unique group identifier",
                "desc": "The unique identifier of the item group",
            },
            "group_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Group Name",
                "short_desc": "Group name",
                "desc": "The name of the item group",
                "example_value": "Electronics",
            },
            "unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement for items in the group",
            },
            "brand": {
                "type": SoftStringOrNothingType,
                "display_name": "Brand",
                "short_desc": "Brand name",
                "desc": "The brand of the item group",
            },
            "manufacturer": {
                "type": SoftStringOrNothingType,
                "display_name": "Manufacturer",
                "short_desc": "Manufacturer name",
                "desc": "The manufacturer of the item group",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Group description",
                "desc": "The description of the item group",
            },
            "product_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Product Type",
                "short_desc": "Product type",
                "desc": "The product type (goods or service)",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Group status",
                "desc": "The status of the item group (active or inactive)",
            },
            "items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Items",
                "short_desc": "Items in the group",
                "desc": "The list of items belonging to this group",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the item group was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the item group was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemGroup") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory item group create request type
public class ZohoInventoryItemGroupCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "group_name": {
                "type": SoftStringType,
                "display_name": "Group Name",
                "short_desc": "Group name",
                "desc": "The name of the item group",
                "example_value": "Electronics",
                "required": True,
            },
            "unit": {
                "type": SoftStringType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement for items in the group",
                "required": True,
            },
            "brand": {
                "type": SoftStringOrNothingType,
                "display_name": "Brand",
                "short_desc": "Brand name",
                "desc": "The brand of the item group",
            },
            "manufacturer": {
                "type": SoftStringOrNothingType,
                "display_name": "Manufacturer",
                "short_desc": "Manufacturer name",
                "desc": "The manufacturer of the item group",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Group description",
                "desc": "The description of the item group",
            },
            "items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Items",
                "short_desc": "Items in the group",
                "desc": "The list of items to add to the group",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemGroupCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory item group update request type
public class ZohoInventoryItemGroupUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "group_id": {
                "type": SoftStringType,
                "display_name": "Group ID",
                "short_desc": "The item group to update",
                "desc": "The ID of the item group to update",
                "required": True,
            },
            "group_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Group Name",
                "short_desc": "Group name",
                "desc": "The new name of the item group",
            },
            "unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The new unit of measurement",
            },
            "brand": {
                "type": SoftStringOrNothingType,
                "display_name": "Brand",
                "short_desc": "Brand name",
                "desc": "The new brand of the item group",
            },
            "manufacturer": {
                "type": SoftStringOrNothingType,
                "display_name": "Manufacturer",
                "short_desc": "Manufacturer name",
                "desc": "The new manufacturer of the item group",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Group description",
                "desc": "The new description of the item group",
            },
            "items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Items",
                "short_desc": "Items in the group",
                "desc": "The new list of items in the group",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemGroupUpdate") {
        addQoreFields(Fields);
    }
}

# ========== ITEM ADJUSTMENT TYPES ==========

#! Zoho Inventory item adjustment record type
public class ZohoInventoryItemAdjustmentDataType inherits HashDataType {
    private {
        const Fields = {
            "inventory_adjustment_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Adjustment ID",
                "short_desc": "Unique adjustment identifier",
                "desc": "The unique identifier of the inventory adjustment",
            },
            "adjustment_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Adjustment Type",
                "short_desc": "Type of adjustment",
                "desc": "The type of inventory adjustment",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "quantity", "desc": "Quantity adjustment"},
                    <AllowedValueInfo>{"value": "value", "desc": "Value adjustment"},
                ),
            },
            "reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Adjustment reason",
                "desc": "The reason for the inventory adjustment",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Adjustment date",
                "desc": "The date of the inventory adjustment (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the adjustment",
            },
            "total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Total value",
                "desc": "The total value of the adjustment",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Adjustment line items",
                "desc": "The list of line items in the adjustment",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Adjustment description",
                "desc": "The description of the inventory adjustment",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the adjustment was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the adjustment was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemAdjustment") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory item adjustment create request type
public class ZohoInventoryItemAdjustmentCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "adjustment_type": {
                "type": SoftStringType,
                "display_name": "Adjustment Type",
                "short_desc": "Type of adjustment",
                "desc": "The type of inventory adjustment",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "quantity", "desc": "Quantity adjustment"},
                    <AllowedValueInfo>{"value": "value", "desc": "Value adjustment"},
                ),
            },
            "date": {
                "type": SoftStringType,
                "display_name": "Date",
                "short_desc": "Adjustment date",
                "desc": "The date of the inventory adjustment (YYYY-MM-DD)",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListType,
                "display_name": "Line Items",
                "short_desc": "Adjustment line items",
                "desc": "The list of line items in the adjustment",
                "required": True,
            },
            "reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Adjustment reason",
                "desc": "The reason for the inventory adjustment",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the adjustment",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Adjustment description",
                "desc": "The description of the inventory adjustment",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryItemAdjustmentCreate") {
        addQoreFields(Fields);
    }
}

# ========== INVOICE TYPES ==========

#! Zoho Inventory invoice record type
public class ZohoInventoryInvoiceDataType inherits HashDataType {
    private {
        const Fields = {
            "invoice_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Invoice ID",
                "short_desc": "Unique invoice identifier",
                "desc": "The unique identifier of the invoice",
            },
            "invoice_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "The invoice number",
                "example_value": "INV-001",
            },
            "customer_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer",
            },
            "customer_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Invoice date",
                "desc": "The date of the invoice (YYYY-MM-DD)",
            },
            "due_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The payment due date (YYYY-MM-DD)",
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms in days",
                "desc": "Net payment terms in days",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Invoice status",
                "desc": "The status of the invoice",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft invoice"},
                    <AllowedValueInfo>{"value": "sent", "desc": "Sent invoice"},
                    <AllowedValueInfo>{"value": "overdue", "desc": "Overdue invoice"},
                    <AllowedValueInfo>{"value": "paid", "desc": "Paid invoice"},
                    <AllowedValueInfo>{"value": "void", "desc": "Void invoice"},
                    <AllowedValueInfo>{"value": "unpaid", "desc": "Unpaid invoice"},
                    <AllowedValueInfo>{"value": "partially_paid", "desc": "Partially paid invoice"},
                ),
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The list of line items on the invoice",
            },
            "total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Invoice total",
                "desc": "The total amount of the invoice",
            },
            "balance": {
                "type": SoftFloatOrNothingType,
                "display_name": "Balance",
                "short_desc": "Outstanding balance",
                "desc": "The outstanding balance on the invoice",
            },
            "sub_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sub Total",
                "short_desc": "Sub total",
                "desc": "The sub total before tax and discounts",
            },
            "tax_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount applied to the invoice",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge on the invoice",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "Notes on the invoice (visible to customer)",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the invoice",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the invoice",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the invoice was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the invoice was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryInvoice") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory invoice create request type
public class ZohoInventoryInvoiceCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "customer_id": {
                "type": SoftStringType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for the invoice",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The list of line items on the invoice",
                "required": True,
            },
            "invoice_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "The invoice number (auto-generated if not provided)",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Invoice date",
                "desc": "The date of the invoice (YYYY-MM-DD)",
            },
            "due_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The payment due date (YYYY-MM-DD)",
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms in days",
                "desc": "Net payment terms in days",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount to apply to the invoice",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge on the invoice",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "Notes on the invoice (visible to customer)",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the invoice",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the invoice",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryInvoiceCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory invoice update request type
public class ZohoInventoryInvoiceUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "invoice_id": {
                "type": SoftStringType,
                "display_name": "Invoice ID",
                "short_desc": "The invoice to update",
                "desc": "The ID of the invoice to update",
                "required": True,
            },
            "customer_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The new customer ID for the invoice",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The new list of line items on the invoice",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Invoice date",
                "desc": "The new date of the invoice (YYYY-MM-DD)",
            },
            "due_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The new payment due date (YYYY-MM-DD)",
            },
            "payment_terms": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Net payment terms in days",
                "desc": "Net payment terms in days",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The new discount to apply to the invoice",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The new shipping charge on the invoice",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "New notes on the invoice",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "New terms and conditions on the invoice",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The new reference number for the invoice",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryInvoiceUpdate") {
        addQoreFields(Fields);
    }
}

# ========== SALES ORDER TYPES ==========

#! Zoho Inventory sales order record type
public class ZohoInventorySalesOrderDataType inherits HashDataType {
    private {
        const Fields = {
            "salesorder_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Unique sales order identifier",
                "desc": "The unique identifier of the sales order",
            },
            "salesorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order Number",
                "short_desc": "Sales order number",
                "desc": "The sales order number",
                "example_value": "SO-001",
            },
            "customer_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer",
            },
            "customer_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Sales order date",
                "desc": "The date of the sales order (YYYY-MM-DD)",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Sales order status",
                "desc": "The status of the sales order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft order"},
                    <AllowedValueInfo>{"value": "open", "desc": "Open order"},
                    <AllowedValueInfo>{"value": "confirmed", "desc": "Confirmed order"},
                    <AllowedValueInfo>{"value": "fulfilled", "desc": "Fulfilled order"},
                    <AllowedValueInfo>{"value": "void", "desc": "Void order"},
                ),
            },
            "shipment_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The expected shipment date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the sales order",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The list of line items in the sales order",
            },
            "total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Order total",
                "desc": "The total amount of the sales order",
            },
            "sub_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sub Total",
                "short_desc": "Sub total",
                "desc": "The sub total before tax and discounts",
            },
            "tax_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge on the sales order",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount applied to the sales order",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Notes on the sales order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the sales order",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the sales order was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the sales order was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventorySalesOrder") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory sales order create request type
public class ZohoInventorySalesOrderCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "customer_id": {
                "type": SoftStringType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for the sales order",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The list of line items in the sales order",
                "required": True,
            },
            "salesorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order Number",
                "short_desc": "Sales order number",
                "desc": "The sales order number (auto-generated if not provided)",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Sales order date",
                "desc": "The date of the sales order (YYYY-MM-DD)",
            },
            "shipment_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The expected shipment date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the sales order",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount to apply to the sales order",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge on the sales order",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Notes on the sales order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the sales order",
            },
        };
    }

    constructor() : HashDataType("ZohoInventorySalesOrderCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory sales order update request type
public class ZohoInventorySalesOrderUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "salesorder_id": {
                "type": SoftStringType,
                "display_name": "Sales Order ID",
                "short_desc": "The sales order to update",
                "desc": "The ID of the sales order to update",
                "required": True,
            },
            "customer_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The new customer ID for the sales order",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The new list of line items",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Sales order date",
                "desc": "The new date of the sales order (YYYY-MM-DD)",
            },
            "shipment_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The new expected shipment date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The new reference number for the sales order",
            },
            "discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The new discount to apply",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The new shipping charge",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "New notes on the sales order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "New terms and conditions",
            },
        };
    }

    constructor() : HashDataType("ZohoInventorySalesOrderUpdate") {
        addQoreFields(Fields);
    }
}

# ========== PURCHASE ORDER TYPES ==========

#! Zoho Inventory purchase order record type
public class ZohoInventoryPurchaseOrderDataType inherits HashDataType {
    private {
        const Fields = {
            "purchaseorder_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Order ID",
                "short_desc": "Unique purchase order identifier",
                "desc": "The unique identifier of the purchase order",
            },
            "purchaseorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Order Number",
                "short_desc": "Purchase order number",
                "desc": "The purchase order number",
                "example_value": "PO-001",
            },
            "vendor_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The ID of the vendor",
            },
            "vendor_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor Name",
                "short_desc": "Vendor name",
                "desc": "The name of the vendor",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Purchase order date",
                "desc": "The date of the purchase order (YYYY-MM-DD)",
            },
            "expected_delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Expected Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "The expected delivery date (YYYY-MM-DD)",
            },
            "delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Date",
                "short_desc": "Actual delivery date",
                "desc": "The actual delivery date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the purchase order",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Purchase order status",
                "desc": "The status of the purchase order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft order"},
                    <AllowedValueInfo>{"value": "open", "desc": "Open order"},
                    <AllowedValueInfo>{"value": "billed", "desc": "Billed order"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled order"},
                ),
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The list of line items in the purchase order",
            },
            "total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Order total",
                "desc": "The total amount of the purchase order",
            },
            "sub_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sub Total",
                "short_desc": "Sub total",
                "desc": "The sub total before tax",
            },
            "tax_total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Notes on the purchase order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the purchase order",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the purchase order was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the purchase order was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPurchaseOrder") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory purchase order create request type
public class ZohoInventoryPurchaseOrderCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "vendor_id": {
                "type": SoftStringType,
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The ID of the vendor for the purchase order",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The list of line items in the purchase order",
                "required": True,
            },
            "purchaseorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase Order Number",
                "short_desc": "Purchase order number",
                "desc": "The purchase order number (auto-generated if not provided)",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Purchase order date",
                "desc": "The date of the purchase order (YYYY-MM-DD)",
            },
            "expected_delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Expected Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "The expected delivery date (YYYY-MM-DD)",
            },
            "delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Date",
                "short_desc": "Actual delivery date",
                "desc": "The actual delivery date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The reference number for the purchase order",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Notes on the purchase order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions on the purchase order",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPurchaseOrderCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory purchase order update request type
public class ZohoInventoryPurchaseOrderUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "purchaseorder_id": {
                "type": SoftStringType,
                "display_name": "Purchase Order ID",
                "short_desc": "The purchase order to update",
                "desc": "The ID of the purchase order to update",
                "required": True,
            },
            "vendor_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The new vendor ID for the purchase order",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The new list of line items",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Purchase order date",
                "desc": "The new date of the purchase order (YYYY-MM-DD)",
            },
            "expected_delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Expected Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "The new expected delivery date (YYYY-MM-DD)",
            },
            "delivery_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Date",
                "short_desc": "Actual delivery date",
                "desc": "The new actual delivery date (YYYY-MM-DD)",
            },
            "reference_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "The new reference number",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "New notes on the purchase order",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Terms and conditions",
                "desc": "New terms and conditions",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPurchaseOrderUpdate") {
        addQoreFields(Fields);
    }
}

# ========== PACKAGE TYPES ==========

#! Zoho Inventory package record type
public class ZohoInventoryPackageDataType inherits HashDataType {
    private {
        const Fields = {
            "package_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Package ID",
                "short_desc": "Unique package identifier",
                "desc": "The unique identifier of the package",
            },
            "package_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Package Number",
                "short_desc": "Package number",
                "desc": "The package number",
                "example_value": "PKG-001",
            },
            "salesorder_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Associated sales order",
                "desc": "The ID of the associated sales order",
            },
            "salesorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order Number",
                "short_desc": "Associated sales order number",
                "desc": "The number of the associated sales order",
            },
            "customer_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer",
            },
            "customer_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Package date",
                "desc": "The date of the package (YYYY-MM-DD)",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Package line items",
                "desc": "The list of line items in the package",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Package notes",
                "desc": "Notes on the package",
            },
            "total_quantity": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Quantity",
                "short_desc": "Total quantity",
                "desc": "The total quantity of items in the package",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the package was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the package was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPackage") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory package create request type
public class ZohoInventoryPackageCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "salesorder_id": {
                "type": SoftStringType,
                "display_name": "Sales Order ID",
                "short_desc": "Associated sales order",
                "desc": "The ID of the sales order to create a package for",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListType,
                "display_name": "Line Items",
                "short_desc": "Package line items",
                "desc": "The list of line items in the package",
                "required": True,
            },
            "package_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Package Number",
                "short_desc": "Package number",
                "desc": "The package number (auto-generated if not provided)",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Package date",
                "desc": "The date of the package (YYYY-MM-DD)",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Package notes",
                "desc": "Notes on the package",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPackageCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory package update request type
public class ZohoInventoryPackageUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "package_id": {
                "type": SoftStringType,
                "display_name": "Package ID",
                "short_desc": "The package to update",
                "desc": "The ID of the package to update",
                "required": True,
            },
            "salesorder_id": {
                "type": SoftStringType,
                "display_name": "Sales Order ID",
                "short_desc": "Associated sales order",
                "desc": "The ID of the associated sales order",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Package line items",
                "desc": "The new list of line items",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Package date",
                "desc": "The new date of the package (YYYY-MM-DD)",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Package notes",
                "desc": "New notes on the package",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryPackageUpdate") {
        addQoreFields(Fields);
    }
}

# ========== SHIPMENT ORDER TYPES ==========

#! Zoho Inventory shipment order record type
public class ZohoInventoryShipmentOrderDataType inherits HashDataType {
    private {
        const Fields = {
            "shipment_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment ID",
                "short_desc": "Unique shipment identifier",
                "desc": "The unique identifier of the shipment order",
            },
            "shipment_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Number",
                "short_desc": "Shipment number",
                "desc": "The shipment number",
            },
            "salesorder_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Associated sales order",
                "desc": "The ID of the associated sales order",
            },
            "salesorder_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sales Order Number",
                "short_desc": "Associated sales order number",
                "desc": "The number of the associated sales order",
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Shipment date",
                "desc": "The date of the shipment (YYYY-MM-DD)",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Shipment status",
                "desc": "The status of the shipment",
            },
            "delivery_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Method",
                "short_desc": "Delivery method",
                "desc": "The delivery method for the shipment",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The tracking number for the shipment",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge for the shipment",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Shipment line items",
                "desc": "The list of line items in the shipment",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Shipment notes",
                "desc": "Notes on the shipment",
            },
            "total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Shipment total",
                "desc": "The total amount of the shipment",
            },
            "created_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the shipment was created",
            },
            "last_modified_time": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified Time",
                "short_desc": "Last modification timestamp",
                "desc": "The date and time when the shipment was last modified",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryShipmentOrder") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory shipment order create request type
public class ZohoInventoryShipmentOrderCreateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "salesorder_id": {
                "type": SoftStringType,
                "display_name": "Sales Order ID",
                "short_desc": "Associated sales order",
                "desc": "The ID of the sales order to create a shipment for",
                "required": True,
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Shipment date",
                "desc": "The date of the shipment (YYYY-MM-DD)",
            },
            "delivery_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Method",
                "short_desc": "Delivery method",
                "desc": "The delivery method for the shipment",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The tracking number for the shipment",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The shipping charge for the shipment",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Shipment line items",
                "desc": "The list of line items in the shipment",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Shipment notes",
                "desc": "Notes on the shipment",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryShipmentOrderCreate") {
        addQoreFields(Fields);
    }
}

#! Zoho Inventory shipment order update request type
public class ZohoInventoryShipmentOrderUpdateDataType inherits HashDataType {
    private {
        const Fields = ZohoInventoryDataProviderBase::OrganizationIdField + {
            "shipment_id": {
                "type": SoftStringType,
                "display_name": "Shipment ID",
                "short_desc": "The shipment to update",
                "desc": "The ID of the shipment to update",
                "required": True,
            },
            "date": {
                "type": SoftStringOrNothingType,
                "display_name": "Date",
                "short_desc": "Shipment date",
                "desc": "The new date of the shipment (YYYY-MM-DD)",
            },
            "delivery_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Method",
                "short_desc": "Delivery method",
                "desc": "The new delivery method",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The new tracking number",
            },
            "shipping_charge": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charge",
                "desc": "The new shipping charge",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The new shipping carrier",
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Shipment line items",
                "desc": "The new list of line items",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Shipment notes",
                "desc": "New notes on the shipment",
            },
        };
    }

    constructor() : HashDataType("ZohoInventoryShipmentOrderUpdate") {
        addQoreFields(Fields);
    }
}

} # namespace ZohoInventoryDataProvider
