# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusEnvironmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce system status environment data type
public class WooCommerceSystemStatusEnvironmentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "home_url": {
                "display_name": "Home URL",
                "short_desc": "Site home URL",
                "type": StringOrNothingType,
                "desc": "The home URL of the WordPress site",
            },
            "site_url": {
                "display_name": "Site URL",
                "short_desc": "WordPress site URL",
                "type": StringOrNothingType,
                "desc": "The WordPress site URL",
            },
            "wc_version": {
                "display_name": "WooCommerce Version",
                "short_desc": "WooCommerce version",
                "type": StringOrNothingType,
                "desc": "The installed WooCommerce version",
            },
            "wp_version": {
                "display_name": "WordPress Version",
                "short_desc": "WordPress version",
                "type": StringOrNothingType,
                "desc": "The installed WordPress version",
            },
            "wp_multisite": {
                "display_name": "Multisite",
                "short_desc": "WordPress multisite enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, WordPress multisite is enabled",
            },
            "wp_memory_limit": {
                "display_name": "WP Memory Limit",
                "short_desc": "WordPress memory limit",
                "type": IntOrNothingType,
                "desc": "The WordPress memory limit in bytes",
            },
            "wp_debug_mode": {
                "display_name": "Debug Mode",
                "short_desc": "WordPress debug mode",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, WordPress debug mode is enabled",
            },
            "wp_cron": {
                "display_name": "WP Cron",
                "short_desc": "WordPress cron enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, WordPress cron is enabled",
            },
            "language": {
                "display_name": "Language",
                "short_desc": "WordPress language",
                "type": StringOrNothingType,
                "desc": "The WordPress language locale",
            },
            "server_info": {
                "display_name": "Server Info",
                "short_desc": "Web server software",
                "type": StringOrNothingType,
                "desc": "The web server software and version",
            },
            "php_version": {
                "display_name": "PHP Version",
                "short_desc": "PHP version",
                "type": StringOrNothingType,
                "desc": "The installed PHP version",
            },
            "php_post_max_size": {
                "display_name": "PHP Post Max Size",
                "short_desc": "PHP post max size",
                "type": IntOrNothingType,
                "desc": "The PHP post max size in bytes",
            },
            "php_max_execution_time": {
                "display_name": "PHP Max Execution Time",
                "short_desc": "PHP max execution time",
                "type": IntOrNothingType,
                "desc": "The PHP max execution time in seconds",
            },
            "php_max_input_vars": {
                "display_name": "PHP Max Input Vars",
                "short_desc": "PHP max input vars",
                "type": IntOrNothingType,
                "desc": "The PHP max input variables limit",
            },
            "curl_version": {
                "display_name": "cURL Version",
                "short_desc": "cURL version",
                "type": StringOrNothingType,
                "desc": "The installed cURL version",
            },
            "max_upload_size": {
                "display_name": "Max Upload Size",
                "short_desc": "Max file upload size",
                "type": IntOrNothingType,
                "desc": "The maximum file upload size in bytes",
            },
            "default_timezone": {
                "display_name": "Default Timezone",
                "short_desc": "Default PHP timezone",
                "type": StringOrNothingType,
                "desc": "The default PHP timezone (should be `UTC`)",
            },
            "fsockopen_or_curl_enabled": {
                "display_name": "fsockopen/cURL",
                "short_desc": "fsockopen or cURL enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, fsockopen or cURL is available for remote requests",
            },
            "soapclient_enabled": {
                "display_name": "SoapClient",
                "short_desc": "SoapClient enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the PHP SoapClient extension is available",
            },
            "domdocument_enabled": {
                "display_name": "DOMDocument",
                "short_desc": "DOMDocument enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the PHP DOMDocument extension is available",
            },
            "gzip_enabled": {
                "display_name": "GZip",
                "short_desc": "GZip compression enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, GZip compression is available",
            },
            "mbstring_enabled": {
                "display_name": "Multibyte String",
                "short_desc": "mbstring enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the PHP mbstring extension is available",
            },
            "remote_post_successful": {
                "display_name": "Remote POST",
                "short_desc": "Remote POST successful",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, remote POST requests are functioning correctly",
            },
            "remote_get_successful": {
                "display_name": "Remote GET",
                "short_desc": "Remote GET successful",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, remote GET requests are functioning correctly",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceSystemStatusEnvironment") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
