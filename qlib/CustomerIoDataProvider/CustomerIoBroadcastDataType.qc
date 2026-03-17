# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoBroadcastDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io broadcast record type
/** Represents a broadcast in Customer.io.
*/
public class CustomerIoBroadcastDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io broadcasts
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "Broadcast ID",
                "desc": "The unique identifier for the broadcast",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Broadcast name",
                "desc": "The name of the broadcast",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Type",
                "desc": "The type of the broadcast",
            },
            "created": {
                "type": IntOrNothingType,
                "display_name": "Created",
                "short_desc": "Created timestamp",
                "desc": "Unix timestamp when the broadcast was created",
            },
            "updated": {
                "type": IntOrNothingType,
                "display_name": "Updated",
                "short_desc": "Updated timestamp",
                "desc": "Unix timestamp when the broadcast was last updated",
            },
            "active": {
                "type": BoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Active status",
                "desc": "Whether the broadcast is currently active",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "The current state of the broadcast",
            },
            "tags": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags",
                "desc": "Tags associated with the broadcast",
            },
            "actions": {
                "type": AutoListOrNothingType,
                "display_name": "Actions",
                "short_desc": "Broadcast actions",
                "desc": "The actions configured for the broadcast",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
