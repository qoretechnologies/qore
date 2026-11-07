# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertWordToPdfDataProvider.qc ConvertAPI convert-word-to-pdf action data provider

/*  ConvertApiConvertWordToPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the convert-word-to-pdf action
public class ConvertApiConvertWordToPdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
            "PageRange": {
                "type": StringOrNothingType,
                "display_name": "Page Range",
                "short_desc": "The pages to convert",
                "desc": "The pages to convert, as a range or a comma-separated list.\n\n**Example**: `1-10` or "
                    "`1,2,5`; all pages are converted by default",
                "example_value": "1-3",
            },
            "Password": {
                "type": StringOrNothingType,
                "display_name": "Open Password",
                "short_desc": "The password to open a protected document",
                "desc": "The password to open a protected document",
                "sensitive": True,
            },
            "BookmarkMode": {
                "type": StringOrNothingType,
                "display_name": "Bookmark Mode",
                "short_desc": "How bookmarks are created in the PDF",
                "desc": "How bookmarks are created in the PDF:\n- `none`: no bookmarks\n- `headings`: bookmarks from "
                    "the document headings (the default)\n- `bookmarks`: the bookmarks of the document",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None"},
                    <AllowedValueInfo>{"value": "headings", "display_name": "Headings"},
                    <AllowedValueInfo>{"value": "bookmarks", "display_name": "Bookmarks"},
                ),
            },
            "UpdateTableOfContents": {
                "type": SoftBoolOrNothingType,
                "display_name": "Update Table of Contents",
                "short_desc": "Update the tables of contents before converting",
                "desc": "Set to `True` to update all tables of contents in the document before the conversion",
            },
            "Pdfa": {
                "type": SoftBoolOrNothingType,
                "display_name": "Create PDF/A",
                "short_desc": "Create a PDF/A-3a compliant document",
                "desc": "Set to `True` to create a PDF/A-3a compliant document for long-term archiving",
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertWordToPdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI convert-word-to-pdf action data provider
/** Uses the <tt>docx</tt> to <tt>pdf</tt> converter.
*/
public class ConvertApiConvertWordToPdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-word-to-pdf",
            "desc": "Converts a Word document (DOC or DOCX) to PDF",
            "type": "ConvertApiConvertWordToPdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "convert-word-to-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts a Word document (DOC or DOCX) to PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConvertApiConvertWordToPdfRequestDataType();
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "docx", "to": "pdf"};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
