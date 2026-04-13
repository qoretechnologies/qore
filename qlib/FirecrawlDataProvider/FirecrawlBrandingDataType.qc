# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlBrandingDataType.qc branding result type for Firecrawl scrape responses

/*  FirecrawlBrandingDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Color palette type for Firecrawl branding responses
public class FirecrawlBrandingColorsDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "primary": {
                "display_name": "Primary",
                "short_desc": "Primary brand color",
                "desc": "Primary brand color as a hex value",
                "type": StringOrNothingType,
            },
            "secondary": {
                "display_name": "Secondary",
                "short_desc": "Secondary brand color",
                "desc": "Secondary brand color as a hex value",
                "type": StringOrNothingType,
            },
            "accent": {
                "display_name": "Accent",
                "short_desc": "Accent color",
                "desc": "Accent color as a hex value",
                "type": StringOrNothingType,
            },
            "background": {
                "display_name": "Background",
                "short_desc": "Background color",
                "desc": "Background color as a hex value",
                "type": StringOrNothingType,
            },
            "textPrimary": {
                "display_name": "Text Primary",
                "short_desc": "Primary text color",
                "desc": "Primary text color as a hex value",
                "type": StringOrNothingType,
            },
            "textSecondary": {
                "display_name": "Text Secondary",
                "short_desc": "Secondary text color",
                "desc": "Secondary text color as a hex value",
                "type": StringOrNothingType,
            },
            "link": {
                "display_name": "Link",
                "short_desc": "Link color",
                "desc": "Link color as a hex value",
                "type": StringOrNothingType,
            },
            "success": {
                "display_name": "Success",
                "short_desc": "Success state color",
                "desc": "Success state color as a hex value",
                "type": StringOrNothingType,
            },
            "warning": {
                "display_name": "Warning",
                "short_desc": "Warning state color",
                "desc": "Warning state color as a hex value",
                "type": StringOrNothingType,
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error state color",
                "desc": "Error state color as a hex value",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Typography settings type for Firecrawl branding responses
public class FirecrawlBrandingTypographyDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "fontFamilies": {
                "display_name": "Font Families",
                "short_desc": "Font family assignments",
                "desc": "Font family assignments with `primary`, `heading`, and `code` keys",
                "type": AutoHashOrNothingType,
            },
            "fontSizes": {
                "display_name": "Font Sizes",
                "short_desc": "Font sizes by heading level",
                "desc": "Font sizes with `h1`, `h2`, `h3`, and `body` keys",
                "type": AutoHashOrNothingType,
            },
            "fontWeights": {
                "display_name": "Font Weights",
                "short_desc": "Font weight values",
                "desc": "Font weight values with `light`, `regular`, `medium`, and `bold` keys",
                "type": AutoHashOrNothingType,
            },
            "lineHeights": {
                "display_name": "Line Heights",
                "short_desc": "Line height values",
                "desc": "Line height values with `heading` and `body` keys",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Spacing settings type for Firecrawl branding responses
public class FirecrawlBrandingSpacingDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "baseUnit": {
                "display_name": "Base Unit",
                "short_desc": "Base spacing unit in pixels",
                "desc": "Base spacing unit in pixels",
                "type": SoftIntOrNothingType,
            },
            "borderRadius": {
                "display_name": "Border Radius",
                "short_desc": "Default border radius",
                "desc": "Default border radius value",
                "type": StringOrNothingType,
            },
            "padding": {
                "display_name": "Padding",
                "short_desc": "Padding settings",
                "desc": "Padding settings detected from the page",
                "type": AutoHashOrNothingType,
            },
            "margins": {
                "display_name": "Margins",
                "short_desc": "Margin settings",
                "desc": "Margin settings detected from the page",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! UI component styles type for Firecrawl branding responses
public class FirecrawlBrandingComponentsDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "buttonPrimary": {
                "display_name": "Primary Button",
                "short_desc": "Primary button styles",
                "desc": "Primary button styles including `background`, `textColor`, "
                    "and `borderRadius`",
                "type": AutoHashOrNothingType,
            },
            "buttonSecondary": {
                "display_name": "Secondary Button",
                "short_desc": "Secondary button styles",
                "desc": "Secondary button styles including `background`, `textColor`, "
                    "`borderColor`, and `borderRadius`",
                "type": AutoHashOrNothingType,
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Input field styles",
                "desc": "Input field styles detected from the page",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Key image URLs type for Firecrawl branding responses
public class FirecrawlBrandingImagesDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "logo": {
                "display_name": "Logo",
                "short_desc": "Logo image URL",
                "desc": "URL of the brand logo image",
                "type": StringOrNothingType,
            },
            "favicon": {
                "display_name": "Favicon",
                "short_desc": "Favicon URL",
                "desc": "URL of the site favicon",
                "type": StringOrNothingType,
            },
            "ogImage": {
                "display_name": "OG Image",
                "short_desc": "Open Graph image URL",
                "desc": "URL of the Open Graph image",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Branding result type for Firecrawl scrape responses
/** Contains branding information extracted from the scraped page, including colors,
    fonts, typography, spacing, and component styles. Available when `branding` is
    in the `formats` list.
*/
public class FirecrawlBrandingDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "colorScheme": {
                "display_name": "Color Scheme",
                "short_desc": "Light or dark color scheme",
                "desc": "The detected color scheme of the page",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "light", "display_name": "Light"},
                    <AllowedValueInfo>{"value": "dark", "display_name": "Dark"},
                ),
            },
            "logo": {
                "display_name": "Logo",
                "short_desc": "Primary logo URL",
                "desc": "URL of the primary logo detected on the page",
                "type": StringOrNothingType,
            },
            "colors": {
                "display_name": "Colors",
                "short_desc": "Color palette",
                "desc": "Color palette extracted from the page, including `primary`, "
                    "`secondary`, `accent`, `background`, `textPrimary`, `textSecondary`, "
                    "`link`, `success`, `warning`, and `error` colors as hex values",
                "type": new FirecrawlBrandingColorsDataType(AutoHashOrNothingType),
            },
            "fonts": {
                "display_name": "Fonts",
                "short_desc": "Font families used",
                "desc": "List of font families detected on the page. Each entry has a "
                    "`family` field with the font name",
                "type": SoftListOrNothingType,
            },
            "typography": {
                "display_name": "Typography",
                "short_desc": "Typography settings",
                "desc": "Typography settings including `fontFamilies` (primary, heading, "
                    "code), `fontSizes` (h1-h3, body), `fontWeights` (regular, medium, "
                    "bold, light), and `lineHeights`",
                "type": new FirecrawlBrandingTypographyDataType(AutoHashOrNothingType),
            },
            "spacing": {
                "display_name": "Spacing",
                "short_desc": "Spacing and layout measurements",
                "desc": "Spacing settings including `baseUnit`, `borderRadius`, "
                    "`padding`, and `margins`",
                "type": new FirecrawlBrandingSpacingDataType(AutoHashOrNothingType),
            },
            "components": {
                "display_name": "Components",
                "short_desc": "UI component styles",
                "desc": "UI component styles including `buttonPrimary`, "
                    "`buttonSecondary`, and `input` field styles",
                "type": new FirecrawlBrandingComponentsDataType(AutoHashOrNothingType),
            },
            "icons": {
                "display_name": "Icons",
                "short_desc": "Icon styling information",
                "desc": "Icon styling information detected on the page",
                "type": AutoHashOrNothingType,
            },
            "images": {
                "display_name": "Images",
                "short_desc": "Key image URLs",
                "desc": "Key image URLs including `logo`, `favicon`, and `ogImage`",
                "type": new FirecrawlBrandingImagesDataType(AutoHashOrNothingType),
            },
            "animations": {
                "display_name": "Animations",
                "short_desc": "Transition and animation settings",
                "desc": "Transition and animation settings detected on the page",
                "type": AutoHashOrNothingType,
            },
            "layout": {
                "display_name": "Layout",
                "short_desc": "Page layout information",
                "desc": "Page layout information including grid, header, and footer dimensions",
                "type": AutoHashOrNothingType,
            },
            "tone": {
                "display_name": "Tone",
                "short_desc": "Brand tone and personality",
                "desc": "Brand tone and personality characteristics detected from the page",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
