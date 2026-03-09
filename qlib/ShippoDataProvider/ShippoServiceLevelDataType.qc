# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoServiceLevelDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Shippo service level data type
public class ShippoServiceLevelDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Service level name",
                "desc": "Name of the service level (e.g., `Priority Mail Express`, `Ground`)",
                "example_value": "Priority Mail",
            },
            "token": {
                "type": SoftStringOrNothingType,
                "display_name": "Token",
                "short_desc": "Service level token",
                "desc": "Token identifier of the service level (e.g., `usps_priority`, `fedex_ground`)",
                "example_value": "usps_priority",
            },
            "terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Service terms",
                "desc": "Further clarification of the service level terms",
            },
            "extended_token": {
                "type": SoftStringOrNothingType,
                "display_name": "Extended Token",
                "short_desc": "Extended service level token",
                "desc": "Unique, extended version of the service level token. Guaranteed to be unique across "
                    "all service levels",
            },
            "parent_servicelevel": {
                "type": AutoHashOrNothingType,
                "display_name": "Parent Service Level",
                "short_desc": "Parent service level",
                "desc": "Links to the more generic version of this service level (e.g., if this is "
                    "`ups_saver_eu`, the parent is `ups_saver`). Has the same structure: `name`, `token`, "
                    "`terms`, and `extended_token`",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoServiceLevelDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoServiceLevelDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
