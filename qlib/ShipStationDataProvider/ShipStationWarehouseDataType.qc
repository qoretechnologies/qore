# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationWarehouseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation warehouse (Ship From Location) record type
public class ShipStationWarehouseDataType inherits HashDataType {
    public {
        const Fields = {
            "warehouseId": {
                "type": IntOrNothingType,
                "display_name": "Warehouse ID",
                "short_desc": "Unique warehouse identifier",
                "desc": "The unique identifier for this warehouse",
            },
            "warehouseName": {
                "type": StringType,
                "display_name": "Warehouse Name",
                "short_desc": "Name of the warehouse",
                "desc": "The display name of this Ship From Location",
                "example_value": "Main Warehouse",
            },
            "originAddress": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Origin Address",
                "short_desc": "Ship from address",
                "desc": "The origin address used for shipping rate calculations",
            },
            "returnAddress": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "The return address (defaults to origin address if not set)",
            },
            "createDate": {
                "type": DateOrNothingType,
                "display_name": "Create Date",
                "short_desc": "When warehouse was created",
                "desc": "The date this warehouse was created",
            },
            "isDefault": {
                "type": BoolOrNothingType,
                "display_name": "Is Default",
                "short_desc": "Is default warehouse",
                "desc": "Whether this is the default Ship From Location",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
