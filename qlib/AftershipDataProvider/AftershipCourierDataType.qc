# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipCourierDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Courier data type
public class AftershipCourierDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "display_name": "Courier Code",
                "short_desc": "Unique courier identifier",
                "desc": "Unique courier identifier (e.g., `fedex`, `ups`, `dhl`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Display name",
                "desc": "Human-readable courier name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Contact phone number",
                "desc": "Courier's contact phone number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "other_name": {
                "display_name": "Other Name",
                "short_desc": "Alternative name or alias",
                "desc": "Alternative name or alias for the courier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "web_url": {
                "display_name": "Website URL",
                "short_desc": "Courier website URL",
                "desc": "URL of the courier's website",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "default_language": {
                "display_name": "Default Language",
                "short_desc": "Primary language code",
                "desc": "Primary language code for the courier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "support_languages": {
                "display_name": "Supported Languages",
                "short_desc": "Languages supported by courier",
                "desc": "List of language codes supported by the courier",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
            },
            "service_from_country_regions": {
                "display_name": "Service Regions",
                "short_desc": "Countries/regions the courier serves",
                "desc": "List of country/region codes where the courier provides service",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipCourierDataType = new AftershipCourierDataType();
}
