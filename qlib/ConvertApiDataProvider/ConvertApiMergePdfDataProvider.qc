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
            "BookmarksTableOfContents": {
                "type": StringOrNothingType,
                "display_name": "Bookmarks Table of Contents",
                "short_desc": "Add a bookmark for each merged file",
                "desc": "Adds a top-level bookmark for each merged file:\n- `disabled`: no bookmarks (the "
                    "default)\n- `filename`: the file name\n- `title`: the PDF title from the metadata",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "disabled", "display_name": "Disabled"},
                    <AllowedValueInfo>{"value": "filename", "display_name": "File Name"},
                    <AllowedValueInfo>{"value": "title", "display_name": "Title"},
                ),
            },
            "RemoveDuplicateFonts": {
                "type": SoftBoolOrNothingType,
                "display_name": "Remove Duplicate Fonts",
                "short_desc": "Include fonts used by several files only once",
                "desc": "Set to `True` to include the fonts used by several of the merged files only once",
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
        return new ConvertApiMergePdfRequestDataType();
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
