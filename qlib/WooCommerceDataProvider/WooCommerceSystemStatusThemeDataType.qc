# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusThemeDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce system status theme data type
public class WooCommerceSystemStatusThemeDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "display_name": "Theme Name",
                "short_desc": "Active theme name",
                "type": StringOrNothingType,
                "desc": "The name of the active WordPress theme",
            },
            "version": {
                "display_name": "Version",
                "short_desc": "Theme version",
                "type": StringOrNothingType,
                "desc": "The version of the active theme",
            },
            "version_latest": {
                "display_name": "Latest Version",
                "short_desc": "Latest available theme version",
                "type": StringOrNothingType,
                "desc": "The latest available version of the theme",
            },
            "author_url": {
                "display_name": "Author URL",
                "short_desc": "Theme author URL",
                "type": StringOrNothingType,
                "desc": "The URL of the theme author",
            },
            "is_child_theme": {
                "display_name": "Child Theme",
                "short_desc": "Whether this is a child theme",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the active theme is a child theme",
            },
            "has_woocommerce_support": {
                "display_name": "WooCommerce Support",
                "short_desc": "Theme has WooCommerce support",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the theme declares WooCommerce support",
            },
            "has_woocommerce_file": {
                "display_name": "WooCommerce File",
                "short_desc": "Theme has woocommerce.php file",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the theme has a `woocommerce.php` template file",
            },
            "has_outdated_templates": {
                "display_name": "Outdated Templates",
                "short_desc": "Theme has outdated templates",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the theme has outdated WooCommerce template overrides",
            },
            "overrides": {
                "display_name": "Template Overrides",
                "short_desc": "List of template overrides",
                "type": SoftListOrNothingType,
                "desc": "List of WooCommerce templates overridden by the theme",
            },
            "parent_name": {
                "display_name": "Parent Theme Name",
                "short_desc": "Parent theme name",
                "type": StringOrNothingType,
                "desc": "The name of the parent theme (if using a child theme)",
            },
            "parent_version": {
                "display_name": "Parent Version",
                "short_desc": "Parent theme version",
                "type": StringOrNothingType,
                "desc": "The version of the parent theme",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceSystemStatusTheme") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
