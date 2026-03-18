# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusSecurityDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce system status security data type
public class WooCommerceSystemStatusSecurityDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "secure_connection": {
                "display_name": "Secure Connection",
                "short_desc": "HTTPS connection status",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the store is using a secure HTTPS connection",
            },
            "hide_errors": {
                "display_name": "Hide Errors",
                "short_desc": "PHP errors hidden from visitors",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, PHP errors are hidden from store visitors",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceSystemStatusSecurity") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
