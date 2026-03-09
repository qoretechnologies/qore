# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoValidationResultsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo address validation message data type
public class ShippoValidationMessageDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "source": {
                "type": SoftStringOrNothingType,
                "display_name": "Source",
                "short_desc": "Validation source",
                "desc": "The validator source (e.g., `Shippo Address Validator`, `USPS`)",
                "example_value": "Shippo Address Validator",
            },
            "type": {
                "type": SoftStringOrNothingType,
                "display_name": "Type",
                "short_desc": "Message type",
                "desc": "The type of validation message (e.g., `address_correction`, `address_error`, "
                    "`geocode_level`, `address_warning`)",
                "example_value": "address_warning",
            },
            "code": {
                "type": SoftStringOrNothingType,
                "display_name": "Code",
                "short_desc": "Validation code",
                "desc": "A code identifying the specific validation result (e.g., `unknown_street`, "
                    "`verification_error`)",
                "example_value": "Unknown Street",
            },
            "text": {
                "type": SoftStringOrNothingType,
                "display_name": "Text",
                "short_desc": "Message text",
                "desc": "A human-readable description of the validation result",
                "example_value": "City, State and ZIP Code are valid, but street address is not a match.",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoValidationMessageDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo address validation results data type
public class ShippoValidationResultsDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "is_valid": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Valid",
                "short_desc": "Address is valid",
                "desc": "Indicates whether the address is valid and deliverable",
            },
            "messages": {
                "type": new ListDataType("messages", new ShippoValidationMessageDataType(), True),
                "display_name": "Messages",
                "short_desc": "Validation messages",
                "desc": "Array of validation messages with details about any corrections or errors found",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoValidationResultsDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoValidationResultsDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
