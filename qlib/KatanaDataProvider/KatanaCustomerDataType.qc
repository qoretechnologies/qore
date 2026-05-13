# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCustomerDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Customer ID", "desc": "Unique customer identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Customer name", "desc": "Name of the customer"},
            "first_name": {"type": SoftStringOrNothingType, "display_name": "First Name",
                "short_desc": "First name", "desc": "Customer first name"},
            "last_name": {"type": SoftStringOrNothingType, "display_name": "Last Name",
                "short_desc": "Last name", "desc": "Customer last name"},
            "company": {"type": SoftStringOrNothingType, "display_name": "Company",
                "short_desc": "Company", "desc": "Company name"},
            "email": {"type": SoftStringOrNothingType, "display_name": "Email",
                "short_desc": "Email", "desc": "Customer email address"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Customer phone number"},
            "comment": {"type": SoftStringOrNothingType, "display_name": "Comment",
                "short_desc": "Comment", "desc": "Free-form comment about the customer"},
            "category": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category", "desc": "Customer category"},
            "reference_id": {"type": SoftStringOrNothingType, "display_name": "Reference ID",
                "short_desc": "External reference",
                "desc": "External reference identifier"},
            "discount_rate": {"type": SoftFloatOrNothingType, "display_name": "Discount Rate",
                "short_desc": "Discount rate", "desc": "Default discount rate (percent)"},
            "default_billing_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Billing Address ID",
                "short_desc": "Default billing address ID",
                "desc": "ID of the default billing address"},
            "default_shipping_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Shipping Address ID",
                "short_desc": "Default shipping address ID",
                "desc": "ID of the default shipping address"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "Default currency"},
            "addresses": {"type": new ListDataType("KatanaCustomerAddrList",
                    new KatanaCustomerAddressDataType(), True),
                "display_name": "Addresses", "short_desc": "Addresses",
                "desc": "Customer addresses"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Created", "desc": "UTC creation timestamp"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Updated", "desc": "UTC update timestamp"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deleted", "desc": "UTC soft-deletion timestamp"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {"type": SoftStringType, "required": True, "display_name": "Name",
                "short_desc": "Customer name", "desc": "Name of the customer (required)",
                "example_value": "Acme Corp"},
            "first_name": {"type": SoftStringOrNothingType, "display_name": "First Name",
                "short_desc": "First name", "desc": "Customer first name"},
            "last_name": {"type": SoftStringOrNothingType, "display_name": "Last Name",
                "short_desc": "Last name", "desc": "Customer last name"},
            "company": {"type": SoftStringOrNothingType, "display_name": "Company",
                "short_desc": "Company", "desc": "Company name"},
            "email": {"type": SoftStringOrNothingType, "display_name": "Email",
                "short_desc": "Email", "desc": "Customer email address"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Customer phone number"},
            "comment": {"type": SoftStringOrNothingType, "display_name": "Comment",
                "short_desc": "Comment", "desc": "Free-form comment about the customer"},
            "category": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category", "desc": "Customer category"},
            "reference_id": {"type": SoftStringOrNothingType, "display_name": "Reference ID",
                "short_desc": "External reference",
                "desc": "External reference identifier"},
            "discount_rate": {"type": SoftFloatOrNothingType, "display_name": "Discount Rate",
                "short_desc": "Discount rate", "desc": "Default discount rate (percent)"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "Default currency"},
            "addresses": {"type": new ListDataType("KatanaCreateCustomerAddrList",
                    new KatanaCustomerAddressDataType(), True),
                "display_name": "Addresses", "short_desc": "Addresses",
                "desc": "Customer addresses"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Customer ID",
                "short_desc": "Customer ID", "desc": "ID of the customer to update",
                "ref_data": "customers"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateCustomerRequestDataType::Fields)
            - ("addresses",)) + {
            "default_shipping_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Shipping Address ID",
                "short_desc": "Default shipping address ID",
                "desc": "ID of the default shipping address",
                "ref_data": "customer_addresses"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Customer ID",
                "short_desc": "Customer ID", "desc": "ID of the customer to delete",
                "ref_data": "customers"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

# --- Customer address standalone request types ---

public class KatanaCreateCustomerAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {"type": SoftIntType, "required": True,
                "display_name": "Customer ID",
                "short_desc": "Parent customer", "desc": "ID of the parent customer",
                "ref_data": "customers"},
            "entity_type": {"type": SoftStringType, "required": True,
                "display_name": "Entity Type",
                "short_desc": "Address type",
                "desc": "Whether this is a `billing` or `shipping` address",
                "allowed_values": AddressEntityTypeAllowed},
            "first_name": {"type": SoftStringOrNothingType, "display_name": "First Name",
                "short_desc": "First name", "desc": "Recipient first name"},
            "last_name": {"type": SoftStringOrNothingType, "display_name": "Last Name",
                "short_desc": "Last name", "desc": "Recipient last name"},
            "company": {"type": SoftStringOrNothingType, "display_name": "Company",
                "short_desc": "Company", "desc": "Company name"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Phone number"},
            "line_1": {"type": SoftStringOrNothingType, "display_name": "Line 1",
                "short_desc": "Address line 1", "desc": "First line of street address"},
            "line_2": {"type": SoftStringOrNothingType, "display_name": "Line 2",
                "short_desc": "Address line 2", "desc": "Second line of street address"},
            "city": {"type": SoftStringOrNothingType, "display_name": "City",
                "short_desc": "City", "desc": "City"},
            "state": {"type": SoftStringOrNothingType, "display_name": "State",
                "short_desc": "State", "desc": "State or region"},
            "zip": {"type": SoftStringOrNothingType, "display_name": "Postal Code",
                "short_desc": "Postal code", "desc": "Postal/ZIP code"},
            "country": {"type": SoftStringOrNothingType, "display_name": "Country",
                "short_desc": "Country", "desc": "Country"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateCustomerAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Address ID",
                "short_desc": "Address ID", "desc": "ID of the address to update",
                "ref_data": "customer_addresses"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateCustomerAddressRequestDataType::Fields)
            - ("customer_id", "entity_type"));
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteCustomerAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Address ID",
                "short_desc": "Address ID", "desc": "ID of the address to delete",
                "ref_data": "customer_addresses"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
