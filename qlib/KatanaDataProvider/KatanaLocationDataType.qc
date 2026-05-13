# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaLocationDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Location ID", "desc": "Unique location identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Location name", "desc": "Name of the location"},
            "legal_name": {"type": SoftStringOrNothingType, "display_name": "Legal Name",
                "short_desc": "Legal name", "desc": "Legal entity name"},
            "address_id": {"type": SoftIntOrNothingType, "display_name": "Address ID",
                "short_desc": "Address ID", "desc": "ID of the location address"},
            "is_primary": {"type": SoftBoolOrNothingType, "display_name": "Primary",
                "short_desc": "Primary location",
                "desc": "If `True`, this is the primary location"},
            "sales_allowed_for_regions": {"type": new ListDataType(
                    "KatanaLocSalesRegions", SoftStringType, True),
                "display_name": "Sales Regions",
                "short_desc": "Sales regions",
                "desc": "Regions allowed to sell from this location"},
            "purchase_allowed_for_regions": {"type": new ListDataType(
                    "KatanaLocPurchaseRegions", SoftStringType, True),
                "display_name": "Purchase Regions",
                "short_desc": "Purchase regions",
                "desc": "Regions allowed to purchase from this location"},
            "manufacturing_allowed_for_regions": {"type": new ListDataType(
                    "KatanaLocMfgRegions", SoftStringType, True),
                "display_name": "Manufacturing Regions",
                "short_desc": "Manufacturing regions",
                "desc": "Regions allowed for manufacturing"},
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

} # namespace KatanaDataProvider
