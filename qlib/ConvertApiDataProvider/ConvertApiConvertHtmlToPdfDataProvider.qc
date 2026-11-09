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
                    <AllowedValueInfo>{"value": "portrait", "display_name": "Portrait", "desc": "Portrait (the default)"},
                    <AllowedValueInfo>{"value": "landscape", "display_name": "Landscape"},
                ),
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
