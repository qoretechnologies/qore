# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareInventoryCountDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square inventory count record type
public class SquareInventoryCountDataType inherits HashDataType {
    public {
        #! Field definitions for Square inventory counts
        const Fields = {
            "catalog_object_id": {
                "type": StringType,
                "display_name": "Catalog Object ID",
                "short_desc": "The catalog object ID",
                "desc": "The ID of the catalog item variation being tracked",
            },
            "catalog_object_type": {
                "type": StringOrNothingType,
                "display_name": "Catalog Object Type",
                "short_desc": "The catalog object type",
                "desc": "The type of the catalog object (typically ITEM_VARIATION)",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The inventory state",
                "desc": "The state of the inventory (IN_STOCK, SOLD, RETURNED_BY_CUSTOMER, "
                    "RESERVED_FOR_SALE, SOLD_ONLINE, ORDERED_FROM_VENDOR, RECEIVED_FROM_VENDOR, "
                    "IN_TRANSIT_TO, NONE, WASTE, UNLINKED_RETURN, COMPOSED, DECOMPOSED)",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where this inventory count applies",
            },
            "quantity": {
                "type": StringOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The inventory quantity",
                "desc": "The quantity of items (stored as a string in Square)",
            },
            "calculated_at": {
                "type": StringOrNothingType,
                "display_name": "Calculated At",
                "short_desc": "When the count was calculated",
                "desc": "The timestamp when this inventory count was calculated (RFC 3339 format)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
