# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaSupplierDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Supplier ID", "desc": "Unique supplier identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Supplier name", "desc": "Name of the supplier"},
            "email": {"type": SoftStringOrNothingType, "display_name": "Email",
                "short_desc": "Email", "desc": "Supplier email address"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Supplier phone number"},
            "comment": {"type": SoftStringOrNothingType, "display_name": "Comment",
                "short_desc": "Comment", "desc": "Free-form comment about the supplier"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "Default currency"},
            "addresses": {"type": new ListDataType("KatanaSupplierAddrList",
                    new KatanaSupplierAddressDataType(), True),
                "display_name": "Addresses", "short_desc": "Addresses",
                "desc": "Supplier addresses"},
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

public class KatanaCreateSupplierRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {"type": SoftStringType, "required": True, "display_name": "Name",
                "short_desc": "Supplier name", "desc": "Name of the supplier (required)",
                "example_value": "Steel Supplier Inc"},
            "email": {"type": SoftStringOrNothingType, "display_name": "Email",
                "short_desc": "Email", "desc": "Supplier email address"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Supplier phone number"},
            "comment": {"type": SoftStringOrNothingType, "display_name": "Comment",
                "short_desc": "Comment", "desc": "Free-form comment about the supplier"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "Default currency"},
            "addresses": {"type": new ListDataType("KatanaCreateSupplierAddrList",
                    new KatanaSupplierAddressDataType(), True),
                "display_name": "Addresses", "short_desc": "Addresses",
                "desc": "Supplier addresses"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateSupplierRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Supplier ID",
                "short_desc": "Supplier ID", "desc": "ID of the supplier to update",
                "ref_data": "suppliers"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateSupplierRequestDataType::Fields)
            - ("addresses",));
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteSupplierRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Supplier ID",
                "short_desc": "Supplier ID", "desc": "ID of the supplier to delete",
                "ref_data": "suppliers"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
