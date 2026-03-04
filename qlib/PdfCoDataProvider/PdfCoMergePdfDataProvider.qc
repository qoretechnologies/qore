# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoMergePdfDataProvider.qc PDF.co merge PDF action data provider

/*  PdfCoMergePdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Request type for merging PDF files
public class PdfCoMergePdfRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "type": FileOrNothingDataType,
                "display_name": "File",
                "short_desc": "File to process",
                "desc": "File to process. Provide either this or a source URL.",
                "required_groups": ("source",),
                "preselected": True,
            },
            "url": {
                "type": SoftListOrNothingType,
                "display_name": "Source URLs",
                "short_desc": "URLs of the source PDF files to merge",
                "desc": "Source URLs of PDF files to merge",
                "required_groups": ("source",),
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Output Filename",
                "short_desc": "Output filename",
                "desc": "Output filename",
            },
            "expiration": {
                "type": SoftIntOrNothingType,
                "display_name": "Expiration",
                "short_desc": "Output link expiration time in minutes",
                "desc": "Output link expiration time in minutes (default: `60`)",
            },
            "RenameMatchingFieldsDuringMerge": {
                "type": SoftBoolOrNothingType,
                "display_name": "Rename Matching Fields During Merge",
                "short_desc": "Rename matching form field names during merge",
                "desc": "Set to `True` to rename matching form field names during merge (default: "
                    "`True`)",
            },
            "GenerateBookmarks": {
                "type": SoftBoolOrNothingType,
                "display_name": "Generate Bookmarks",
                "short_desc": "Add bookmarks for each merged document",
                "desc": "Set to `True` to add bookmarks for each merged document",
            },
            "MergedDocumentTitle": {
                "type": StringOrNothingType,
                "display_name": "Merged Document Title",
                "short_desc": "Custom title for the merged document",
                "desc": "Custom title for the merged document",
            },
            "profiles": {
                "type": StringOrNothingType,
                "display_name": "Profiles",
                "short_desc": "Advanced processing profiles",
                "desc": "Advanced processing profiles as a JSON string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co merge PDF action data provider
public class PdfCoMergePdfDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "merge-pdf",
            "desc": "Merges multiple PDF or other supported files into one PDF",
            "type": "PdfCoMergePdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "merge-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Merges multiple PDF or other supported files into one PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoMergePdfRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = uploadFileIfNeeded(req);
        if (req.url.typeCode() == NT_LIST) {
            req.url = (foldl $1 + "," + $2, req.url);
        }
        return doPost("pdf/merge2", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
