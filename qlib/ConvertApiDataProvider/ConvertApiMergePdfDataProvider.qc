# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiMergePdfDataProvider.qc ConvertAPI merge-pdf action data provider

/*  ConvertApiMergePdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the merge-pdf action
public class ConvertApiMergePdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiMultiSourceFields + {
            "Password": {
                "type": StringOrNothingType,
                "display_name": "Open Password",
                "short_desc": "The password to open protected PDF files",
                "desc": "The password to open the protected PDF files",
                "sensitive": True,
            },
            "BookmarksTableOfContents": {
                "type": StringOrNothingType,
                "display_name": "Bookmarks Table of Contents",
                "short_desc": "Add a bookmark for each merged file",
                "desc": "Adds a top-level bookmark for each merged file",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "disabled",
                        "display_name": "Disabled",
                        "desc": "No bookmarks (the default)",
                    },
                    <AllowedValueInfo>{
                        "value": "filename",
                        "display_name": "File Name",
                        "desc": "Bookmarks named with the file names",
                    },
                    <AllowedValueInfo>{
                        "value": "title",
                        "display_name": "Title",
                        "desc": "Bookmarks named with the PDF titles from the metadata",
                    },
                ),
            },
            "RemoveDuplicateFonts": {
                "type": SoftBoolOrNothingType,
                "display_name": "Remove Duplicate Fonts",
                "short_desc": "Include fonts used by several files only once",
                "desc": "Set to `True` to include the fonts used by several of the merged files only once",
            },
            "PageSize": {
                "type": StringOrNothingType,
                "display_name": "Page Size",
                "short_desc": "The page size that every page is scaled to",
                "desc": "The page size that every page of the merged PDF is scaled to",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "default",
                        "display_name": "Default",
                        "desc": "Every page keeps its size (the default)",
                    },
                    <AllowedValueInfo>{"value": "a2", "display_name": "A2"},
                    <AllowedValueInfo>{"value": "a3", "display_name": "A3"},
                    <AllowedValueInfo>{"value": "a4", "display_name": "A4"},
                    <AllowedValueInfo>{"value": "a5", "display_name": "A5"},
                    <AllowedValueInfo>{"value": "a6", "display_name": "A6"},
                    <AllowedValueInfo>{"value": "letter", "display_name": "Letter"},
                    <AllowedValueInfo>{"value": "legal", "display_name": "Legal"},
                ),
            },
            "PageOrientation": {
                "type": StringOrNothingType,
                "display_name": "Page Orientation",
                "short_desc": "The orientation of every page",
                "desc": "The orientation of every page of the merged PDF",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "default",
                        "display_name": "Default",
                        "desc": "Every page keeps its orientation (the default)",
                    },
                    <AllowedValueInfo>{"value": "portrait", "display_name": "Portrait"},
                    <AllowedValueInfo>{"value": "landscape", "display_name": "Landscape"},
                ),
            },
            "OpenPage": {
                "type": SoftIntOrNothingType,
                "display_name": "Open to Page",
                "short_desc": "The page that the PDF opens at",
                "desc": "The number of the page that the merged PDF opens at, from `1` to `3000`; the default is `1`",
                "example_value": 2,
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiMergePdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI merge-pdf action data provider
/** Uses the <tt>pdf</tt> to <tt>merge</tt> converter.
*/
public class ConvertApiMergePdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "merge-pdf",
            "desc": "Merges several PDF files into one PDF",
            "type": "ConvertApiMergePdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiMergePdfRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "merge-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Merges several PDF files into one PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "pdf", "to": "merge"};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
