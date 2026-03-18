# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusDatabaseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce system status database data type
public class WooCommerceSystemStatusDatabaseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "wc_database_version": {
                "display_name": "WC Database Version",
                "short_desc": "WooCommerce database version",
                "type": StringOrNothingType,
                "desc": "The WooCommerce database version",
            },
            "database_prefix": {
                "display_name": "Database Prefix",
                "short_desc": "WordPress database table prefix",
                "type": StringOrNothingType,
                "desc": "The WordPress database table prefix (e.g., `wp_`)",
            },
            "maxdb": {
                "display_name": "MaxDB",
                "short_desc": "Max database size",
                "type": IntOrNothingType,
                "desc": "The maximum database packet size",
            },
            "database_size": {
                "display_name": "Database Size",
                "short_desc": "Total database size",
                "type": AutoHashOrNothingType,
                "desc": "The total database size including data and index sizes",
            },
            "database_tables": {
                "display_name": "Database Tables",
                "short_desc": "Database tables information",
                "type": AutoHashOrNothingType,
                "desc": "Information about WooCommerce and WordPress database tables",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceSystemStatusDatabase") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
