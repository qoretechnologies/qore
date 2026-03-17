# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSettingDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce setting data type
public class WooCommerceSettingDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Setting identifier",
                "type": StringOrNothingType,
                "desc": "The unique string identifier for the setting",
            },
            "label": {
                "display_name": "Label",
                "short_desc": "Setting label",
                "type": StringOrNothingType,
                "desc": "The human-readable label for the setting",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Setting description",
                "type": StringOrNothingType,
                "desc": "The description of the setting",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Setting input type",
                "type": StringOrNothingType,
                "desc": "The setting input type (e.g., `text`, `select`, `checkbox`)",
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Current setting value",
                "type": StringOrNothingType,
                "desc": "The current value of the setting",
            },
            "default_value": {
                "display_name": "Default Value",
                "short_desc": "Default setting value",
                "type": StringOrNothingType,
                "desc": "The default value for the setting (corresponds to the `default` field in the API)",
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Available setting options",
                "type": AutoHashOrNothingType,
                "desc": "A hash of available options for `select` type settings",
            },
            "group_id": {
                "display_name": "Group ID",
                "short_desc": "Setting group identifier",
                "type": StringOrNothingType,
                "desc": "The identifier of the settings group this setting belongs to",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
