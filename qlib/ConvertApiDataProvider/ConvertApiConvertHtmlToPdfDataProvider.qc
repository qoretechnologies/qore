# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertHtmlToPdfDataProvider.qc ConvertAPI convert-html-to-pdf action data provider

/*  ConvertApiConvertHtmlToPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConvertApiDataProvider module
public namespace ConvertApiDataProvider {

#! Request type for the convert-html-to-pdf action
public class ConvertApiConvertHtmlToPdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
            "url": ConvertApiSingleSourceFields.url + {
                "desc": "The URL of the web page to convert; the page is rendered in a browser with its scripts, "
                    "styles and images.\n\n"
                    "**Example**: `https://www.example.com`",
            },
        } + {
            "PageRange": {
                "type": StringOrNothingType,
                "display_name": "Page Range",
                "short_desc": "The pages to convert",
                "desc": "The pages to convert, as a range or a comma-separated list.\n\n**Example**: `1-10` or "
                    "`1,2,5`; all pages are converted by default",
                "example_value": "1-3",
            },
            "AuthUsername": {
                "type": StringOrNothingType,
                "display_name": "Authentication User Name",
                "short_desc": "The user name for HTTP authentication of the page",
                "desc": "The user name for a web page that is protected with HTTP authentication",
            },
            "AuthPassword": {
                "type": StringOrNothingType,
                "display_name": "Authentication Password",
                "short_desc": "The password for HTTP authentication of the page",
                "desc": "The password for a web page that is protected with HTTP authentication",
                "sensitive": True,
            },
            "Cookies": {
                "type": StringOrNothingType,
                "display_name": "Cookies",
                "short_desc": "Cookies to send with the page request",
                "desc": "Cookies to send with the request for the web page, separated by semicolons.\n\n"
                    "**Example**: `name1=value1; name2=value2`",
                "sensitive": True,
            },
            "Headers": {
                "type": StringOrNothingType,
                "display_name": "HTTP Headers",
                "short_desc": "Custom HTTP headers to send with the page request",
                "desc": "Custom HTTP headers to send with the request for the web page, as `name:value` pairs "
                    "separated by `|`.\n\n"
                    "**Example**: `Accept-Language:de|Cache-Control:no-cache`",
                # an Authorization header can carry credentials
                "sensitive": True,
            },
            "AdBlock": {
                "type": SoftBoolOrNothingType,
                "display_name": "Block Ads",
                "short_desc": "Block the ads of the page",
                "desc": "Set to `True` to block the ads of the web page",
            },
            "CookieConsentBlock": {
                "type": SoftBoolOrNothingType,
                "display_name": "Remove Cookie Warnings",
                "short_desc": "Remove cookie consent banners",
                "desc": "Set to `True` to try to remove the cookie consent warnings of the web page",
            },
            "WaitElement": {
                "type": StringOrNothingType,
                "display_name": "Wait for Element",
                "short_desc": "CSS selector of an element to wait for",
                "desc": "The CSS selector of an element that the converter waits for: the conversion begins when "
                    "the element appears in the page.\n\n"
                    "**Example**: `#content`",
            },
            "UserJs": {
                "type": StringOrNothingType,
                "display_name": "Run JavaScript",
                "short_desc": "JavaScript to run before the conversion",
                "desc": "JavaScript code to run in the web page before the conversion begins",
            },
            "UserCss": {
                "type": StringOrNothingType,
                "display_name": "Additional CSS",
                "short_desc": "CSS to apply before the conversion",
                "desc": "CSS to apply to the web page before the conversion begins",
            },
            "HideElements": {
                "type": StringOrNothingType,
                "display_name": "Hide Elements",
                "short_desc": "CSS selector of the elements to hide",
                "desc": "The CSS selector of the elements to hide during the conversion.\n\n"
                    "**Example**: `.banner, #chat`",
            },
            "ShowElements": {
                "type": StringOrNothingType,
                "display_name": "Show Only Elements",
                "short_desc": "CSS selector of the only elements to show",
                "desc": "The CSS selector of the elements to show during the conversion; all other elements are "
                    "hidden",
            },
            "ViewportWidth": {
                "type": SoftIntOrNothingType,
                "display_name": "Viewport Width",
                "short_desc": "The browser viewport width in pixels",
                "desc": "The width of the browser viewport in pixels, from `200` to `4000`; the default is `1366`",
                "example_value": 1920,
            },
            "ViewportHeight": {
                "type": SoftIntOrNothingType,
                "display_name": "Viewport Height",
                "short_desc": "The browser viewport height in pixels",
                "desc": "The height of the browser viewport in pixels, from `200` to `4000`; the default is `1024`",
                "example_value": 1080,
            },
            "FixedElements": {
                "type": StringOrNothingType,
                "display_name": "Fixed Elements",
                "short_desc": "How elements with a fixed position are handled",
                "desc": "How the elements with a fixed CSS position are changed to adapt the page for the PDF",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "fixed",
                        "display_name": "Leave Unchanged",
                        "desc": "The elements keep their fixed position (the default)",
                    },
                    <AllowedValueInfo>{
                        "value": "absolute",
                        "display_name": "Absolute",
                        "desc": "The elements get an absolute position",
                    },
                    <AllowedValueInfo>{
                        "value": "relative",
                        "display_name": "Relative",
                        "desc": "The elements get a relative position",
                    },
                    <AllowedValueInfo>{
                        "value": "hide",
                        "display_name": "Hide",
                        "desc": "The elements are hidden",
                    },
                ),
            },
            "AvoidBreakElements": {
                "type": StringOrNothingType,
                "display_name": "Avoid Page Breaks in Elements",
                "short_desc": "CSS selector of elements not to split across pages",
                "desc": "The CSS selector of the elements that must not be split by a page break",
            },
            "BreakBeforeElements": {
                "type": StringOrNothingType,
                "display_name": "Page Break Before Elements",
                "short_desc": "CSS selector of elements that start a new page",
                "desc": "The CSS selector of the elements to insert a page break before",
            },
            "BreakAfterElements": {
                "type": StringOrNothingType,
                "display_name": "Page Break After Elements",
                "short_desc": "CSS selector of elements that end a page",
                "desc": "The CSS selector of the elements to insert a page break after",
            },
            "RespectViewport": {
                "type": SoftBoolOrNothingType,
                "display_name": "Respect Viewport",
                "short_desc": "Show the page as it looks in the browser",
                "desc": "Set to `False` to create the PDF like the print to PDF function of Chrome; by default, the "
                    "PDF shows the page as it looks in the browser",
            },
            "Scale": {
                "type": SoftIntOrNothingType,
                "display_name": "Scale",
                "short_desc": "The page scale in percent",
                "desc": "The scale of the web page in percent, from `10` to `200`; the default is `100`",
                "example_value": 80,
            },
            "PageSize": {
                "type": StringOrNothingType,
                "display_name": "Page Size",
                "short_desc": "The PDF page size",
                "desc": "The PDF page size",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "a0", "display_name": "A0"},
                    <AllowedValueInfo>{"value": "a1", "display_name": "A1"},
                    <AllowedValueInfo>{"value": "a2", "display_name": "A2"},
                    <AllowedValueInfo>{"value": "a3", "display_name": "A3"},
                    <AllowedValueInfo>{"value": "a4", "display_name": "A4"},
                    <AllowedValueInfo>{"value": "a5", "display_name": "A5"},
                    <AllowedValueInfo>{"value": "a6", "display_name": "A6"},
                    <AllowedValueInfo>{"value": "a7", "display_name": "A7"},
                    <AllowedValueInfo>{"value": "a8", "display_name": "A8"},
                    <AllowedValueInfo>{"value": "a9", "display_name": "A9"},
                    <AllowedValueInfo>{"value": "b0", "display_name": "B0"},
                    <AllowedValueInfo>{"value": "b1", "display_name": "B1"},
                    <AllowedValueInfo>{"value": "b2", "display_name": "B2"},
                    <AllowedValueInfo>{"value": "b3", "display_name": "B3"},
                    <AllowedValueInfo>{"value": "b4", "display_name": "B4"},
                    <AllowedValueInfo>{"value": "b5", "display_name": "B5"},
                    <AllowedValueInfo>{"value": "letter", "display_name": "Letter", "desc": "Letter (the default)"},
                    <AllowedValueInfo>{"value": "legal", "display_name": "Legal"},
                    <AllowedValueInfo>{"value": "ledger", "display_name": "Ledger"},
                ),
            },
            "PageOrientation": {
                "type": StringOrNothingType,
                "display_name": "Page Orientation",
                "short_desc": "The PDF page orientation",
                "desc": "The PDF page orientation",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "portrait",
                        "display_name": "Portrait",
                        "desc": "Portrait (the default)",
                    },
                    <AllowedValueInfo>{"value": "landscape", "display_name": "Landscape"},
                ),
            },
            "PageWidth": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Width",
                "short_desc": "A custom page width in millimeters",
                "desc": "A custom page width in millimeters, from `10` to `30000`, which overrides `PageSize`",
                "example_value": 210,
            },
            "PageHeight": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Height",
                "short_desc": "A custom page height in millimeters",
                "desc": "A custom page height in millimeters, from `10` to `30000`, which overrides `PageSize`",
                "example_value": 297,
            },
            "MarginTop": {
                "type": SoftIntOrNothingType,
                "display_name": "Top Margin",
                "short_desc": "The top page margin in millimeters",
                "desc": "The top page margin in millimeters; the default is `10`",
                "example_value": 10,
            },
            "MarginRight": {
                "type": SoftIntOrNothingType,
                "display_name": "Right Margin",
                "short_desc": "The right page margin in millimeters",
                "desc": "The right page margin in millimeters; the default is `10`",
                "example_value": 10,
            },
            "MarginBottom": {
                "type": SoftIntOrNothingType,
                "display_name": "Bottom Margin",
                "short_desc": "The bottom page margin in millimeters",
                "desc": "The bottom page margin in millimeters; the default is `10`",
                "example_value": 10,
            },
            "MarginLeft": {
                "type": SoftIntOrNothingType,
                "display_name": "Left Margin",
                "short_desc": "The left page margin in millimeters",
                "desc": "The left page margin in millimeters; the default is `10`",
                "example_value": 10,
            },
            "Header": {
                "type": StringOrNothingType,
                "display_name": "Header HTML",
                "short_desc": "HTML for the header of each page",
                "desc": "HTML for the header of each page; elements with the classes `pageNumber`, `totalPages`, "
                    "`title` and `date` are filled with the values for each page, and inline CSS can style the "
                    "header.\n\n"
                    "**Example**: `<span class='pageNumber'></span> / <span class='totalPages'></span>`",
            },
            "Footer": {
                "type": StringOrNothingType,
                "display_name": "Footer HTML",
                "short_desc": "HTML for the footer of each page",
                "desc": "HTML for the footer of each page; elements with the classes `pageNumber`, `totalPages`, "
                    "`title` and `date` are filled with the values for each page, and inline CSS can style the "
                    "footer.\n\n"
                    "**Example**: `<span class='date'></span>`",
            },
            "Background": {
                "type": SoftBoolOrNothingType,
                "display_name": "Background",
                "short_desc": "Print the page background",
                "desc": "Set to `False` to leave out the page background; the default is `True`",
            },
            "JavaScript": {
                "type": SoftBoolOrNothingType,
                "display_name": "JavaScript",
                "short_desc": "Allow the page to run JavaScript",
                "desc": "Set to `False` to disable JavaScript in the page; the default is `True`",
            },
            "ConversionDelay": {
                "type": SoftIntOrNothingType,
                "display_name": "Conversion Delay",
                "short_desc": "Seconds to wait before creating the PDF",
                "desc": "The delay in seconds after the page is loaded and before the PDF is created, to let the "
                    "page finish loading; the default is `0`",
                "example_value": 2,
            },
            "CssMediaType": {
                "type": StringOrNothingType,
                "display_name": "CSS Media Type",
                "short_desc": "The CSS media type used for rendering",
                "desc": "The CSS media type used for rendering the page; other media types can also be given",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "screen",
                        "display_name": "Screen",
                        "desc": "The page as shown on a screen (the default)",
                    },
                    <AllowedValueInfo>{
                        "value": "print",
                        "display_name": "Print",
                        "desc": "The page with its print styles",
                    },
                ),
                "allowed_values_creatable": True,
                "example_value": "print",
            },
            "LoadLazyContent": {
                "type": SoftBoolOrNothingType,
                "display_name": "Load Lazy Content",
                "short_desc": "Load images that load only when visible",
                "desc": "Set to `True` to load the images of the page that load only when they become visible",
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertHtmlToPdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI convert-html-to-pdf action data provider
/** Uses the <tt>html</tt> to <tt>pdf</tt> converter; a source URL is rendered as a web page in a browser.
*/
public class ConvertApiConvertHtmlToPdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-html-to-pdf",
            "desc": "Converts a web page or an HTML file to PDF",
            "type": "ConvertApiConvertHtmlToPdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiConvertHtmlToPdfRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "convert-html-to-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts a web page or an HTML file to PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "html", "to": "pdf"};
    }

    #! Returns the name of the parameter for the \c url field: the \c Url parameter of the web page to render
    private *string getUrlParameter(hash<auto> req) {
        return "Url";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
