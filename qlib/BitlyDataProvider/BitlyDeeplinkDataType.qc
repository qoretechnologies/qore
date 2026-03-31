# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyDeeplinkDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly deep link configuration type for mobile app routing
public class BitlyDeeplinkDataType inherits HashDataType {
    public {
        #! Field definitions for a deep link configuration
        const Fields = {
            "app_id": {
                "type": StringOrNothingType,
                "display_name": "App ID",
                "short_desc": "Application identifier",
                "desc": "The identifier for the mobile application",
            },
            "app_uri_path": {
                "type": StringOrNothingType,
                "display_name": "App URI Path",
                "short_desc": "Path within the app",
                "desc": "The path within the app to route to (e.g., `/store?id=123456`)",
                "example_value": "/store?id=123456",
            },
            "install_url": {
                "type": StringOrNothingType,
                "display_name": "Install URL",
                "short_desc": "App installation URL",
                "desc": "The URL where users can install the app if not already installed",
                "example_value": "https://apps.apple.com/app/id123456",
            },
            "install_type": {
                "type": StringOrNothingType,
                "display_name": "Install Type",
                "short_desc": "App install behavior",
                "desc": "Controls what happens when the app is not installed",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "no_install", "display_name": "No Install"},
                    <AllowedValueInfo>{"value": "auto_install",
                        "display_name": "Auto Install"},
                    <AllowedValueInfo>{"value": "promote_install",
                        "display_name": "Promote Install"},
                ),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
