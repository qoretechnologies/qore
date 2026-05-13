# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaSalesOrderAddressDataType.qc Katana sales order address sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana sales order address sub-type
public class KatanaSalesOrderAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Address ID",
                "desc": "The unique identifier of the address",
                "ref_data": "sales_order_addresses",
            },
            "sales_order_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Sales order ID",
                "desc": "The ID of the sales order this address belongs to",
                "ref_data": "sales_orders",
            },
            "entity_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Entity Type",
                "short_desc": "Address entity type",
                "desc": "Address type: `billing` or `shipping`",
                "allowed_values": AddressEntityTypeAllowed,
            },
            "first_name": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Recipient first name",
                "desc": "First name of the recipient",
            },
            "last_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Recipient last name",
                "desc": "Last name of the recipient",
            },
            "company": {
                "type": SoftStringOrNothingType,
                "display_name": "Company",
                "short_desc": "Company name",
                "desc": "Company name for the address",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Contact phone number",
            },
            "line_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Line 1",
                "short_desc": "Address line 1",
                "desc": "First line of the street address",
            },
            "line_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Line 2",
                "short_desc": "Address line 2",
                "desc": "Second line of the street address",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City name",
            },
            "state": {
                "type": SoftStringOrNothingType,
                "display_name": "State",
                "short_desc": "State or region",
                "desc": "State, province, or region",
            },
            "zip": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal/ZIP code",
                "desc": "Postal or ZIP code",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "Country name or ISO code",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KatanaDataProvider
