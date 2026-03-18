# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce system status data type
public class WooCommerceSystemStatusDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "environment": {
                "display_name": "Environment",
                "short_desc": "Server environment details",
                "type": new WooCommerceSystemStatusEnvironmentDataType(AutoHashOrNothingType),
                "desc": "Server environment information including PHP version, WordPress version, and "
                    "server software",
            },
            "database": {
                "display_name": "Database",
                "short_desc": "Database information",
                "type": new WooCommerceSystemStatusDatabaseDataType(AutoHashOrNothingType),
                "desc": "Database information including version, prefix, and table sizes",
            },
            "active_plugins": {
                "display_name": "Active Plugins",
                "short_desc": "List of active plugins",
                "type": SoftListOrNothingType,
                "desc": "List of currently active WordPress plugins",
            },
            "inactive_plugins": {
                "display_name": "Inactive Plugins",
                "short_desc": "List of inactive plugins",
                "type": SoftListOrNothingType,
                "desc": "List of currently inactive WordPress plugins",
            },
            "theme": {
                "display_name": "Theme",
                "short_desc": "Active theme information",
                "type": new WooCommerceSystemStatusThemeDataType(AutoHashOrNothingType),
                "desc": "Information about the currently active WordPress theme",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "WooCommerce settings",
                "type": new WooCommerceSystemStatusSettingsDataType(AutoHashOrNothingType),
                "desc": "General WooCommerce settings including currency and API status",
            },
            "security": {
                "display_name": "Security",
                "short_desc": "Security status",
                "type": new WooCommerceSystemStatusSecurityDataType(AutoHashOrNothingType),
                "desc": "Security status information including HTTPS and key checks",
            },
            "pages": {
                "display_name": "Pages",
                "short_desc": "WooCommerce pages status",
                "type": SoftListOrNothingType,
                "desc": "List of WooCommerce pages and their configuration status",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
