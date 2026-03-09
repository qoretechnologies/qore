# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareCatalogItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square catalog object record type
public class SquareCatalogItemDataType inherits HashDataType {
    public {
        #! Field definitions for Square catalog objects
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The catalog object type",
                "desc": "The type of the catalog object (ITEM, ITEM_VARIATION, CATEGORY, "
                    "DISCOUNT, TAX, MODIFIER, MODIFIER_LIST, IMAGE, etc.)",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The catalog object ID",
                "desc": "The unique identifier of the catalog object",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the object was last updated",
                "desc": "The timestamp when the catalog object was last updated (RFC 3339 format)",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The catalog object version",
                "desc": "The current version of the catalog object",
            },
            "is_deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Deleted",
                "short_desc": "Whether the object is deleted",
                "desc": "Whether this catalog object has been deleted",
            },
            "present_at_all_locations": {
                "type": SoftBoolOrNothingType,
                "display_name": "Present at All Locations",
                "short_desc": "Available at all locations",
                "desc": "Whether this catalog object is present at all locations",
            },
            "present_at_location_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Present At Locations",
                "short_desc": "Locations where available",
                "desc": "List of location IDs where this catalog object is present",
            },
            "item_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Item Data",
                "short_desc": "Item-specific data",
                "desc": "Item-specific data (hash with name, description, abbreviation, "
                    "category_id, variations, product_type, etc.). Only present for ITEM type.",
            },
            "item_variation_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Item Variation Data",
                "short_desc": "Item variation data",
                "desc": "Item variation data (hash with item_id, name, sku, pricing_type, "
                    "price_money, etc.). Only present for ITEM_VARIATION type.",
            },
            "category_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Category Data",
                "short_desc": "Category-specific data",
                "desc": "Category-specific data (hash with name). Only present for CATEGORY type.",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
