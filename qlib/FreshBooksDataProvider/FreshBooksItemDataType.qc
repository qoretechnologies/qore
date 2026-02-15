# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks item record type
public class FreshBooksItemDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks items
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The item ID",
                "desc": "The unique identifier of the item",
            },
            "itemid": {
                "type": IntType,
                "display_name": "Item ID",
                "short_desc": "The internal item ID",
                "desc": "The internal item ID",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The item name",
                "desc": "The name of the item",
                "example_value": "Consulting",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The item description",
                "desc": "The description of the item",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The SKU",
                "desc": "The stock keeping unit of the item",
            },
            "unit_cost": {
                "type": AutoHashType,
                "display_name": "Unit Cost",
                "short_desc": "The unit cost",
                "desc": "The unit cost of the item (hash with 'amount' and 'code' keys)",
            },
            "qty": {
                "type": StringOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The default quantity",
                "desc": "The default quantity of the item (string in FreshBooks API)",
            },
            "inventory": {
                "type": StringOrNothingType,
                "display_name": "Inventory",
                "short_desc": "The inventory count",
                "desc": "The inventory count of the item",
            },
            "tax1": {
                "type": StringOrNothingType,
                "display_name": "Tax 1",
                "short_desc": "The default tax 1",
                "desc": "The default first tax for the item",
            },
            "tax2": {
                "type": StringOrNothingType,
                "display_name": "Tax 2",
                "short_desc": "The default tax 2",
                "desc": "The default second tax for the item",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the item (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks item create request type
public class FreshBooksItemCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating items
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The item name (required)",
                "desc": "The name of the item",
                "example_value": "Consulting",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The item description",
                "desc": "The description of the item",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The SKU",
                "desc": "The stock keeping unit of the item",
            },
            "unit_cost": {
                "type": AutoHashOrNothingType,
                "display_name": "Unit Cost",
                "short_desc": "The unit cost",
                "desc": "The unit cost of the item (hash with 'amount' and 'code' keys)",
            },
            "qty": {
                "type": StringOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The default quantity",
                "desc": "The default quantity of the item (string in FreshBooks API)",
            },
            "inventory": {
                "type": StringOrNothingType,
                "display_name": "Inventory",
                "short_desc": "The inventory count",
                "desc": "The inventory count of the item",
            },
            "tax1": {
                "type": StringOrNothingType,
                "display_name": "Tax 1",
                "short_desc": "The default tax 1",
                "desc": "The default first tax for the item",
            },
            "tax2": {
                "type": StringOrNothingType,
                "display_name": "Tax 2",
                "short_desc": "The default tax 2",
                "desc": "The default second tax for the item",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks item update request type
public class FreshBooksItemUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating items
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The item ID (required)",
                "desc": "The unique identifier of the item to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The item name",
                "desc": "The new name of the item",
                "example_value": "Consulting",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The item description",
                "desc": "The new description of the item",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The SKU",
                "desc": "The new stock keeping unit of the item",
            },
            "unit_cost": {
                "type": AutoHashOrNothingType,
                "display_name": "Unit Cost",
                "short_desc": "The unit cost",
                "desc": "The new unit cost of the item (hash with 'amount' and 'code' keys)",
            },
            "qty": {
                "type": StringOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The default quantity",
                "desc": "The new default quantity of the item (string in FreshBooks API)",
            },
            "inventory": {
                "type": StringOrNothingType,
                "display_name": "Inventory",
                "short_desc": "The inventory count",
                "desc": "The new inventory count of the item",
            },
            "tax1": {
                "type": StringOrNothingType,
                "display_name": "Tax 1",
                "short_desc": "The default tax 1",
                "desc": "The new default first tax for the item",
            },
            "tax2": {
                "type": StringOrNothingType,
                "display_name": "Tax 2",
                "short_desc": "The default tax 2",
                "desc": "The new default second tax for the item",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The new visibility state of the item (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
