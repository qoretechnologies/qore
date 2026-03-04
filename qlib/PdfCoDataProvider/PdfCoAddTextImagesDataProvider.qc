# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoAddTextImagesDataProvider.qc PDF.co add text and images action data provider

/*  PdfCoAddTextImagesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for adding text and images to a PDF
public class PdfCoAddTextImagesRequestDataType inherits HashDataType {
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
                "type": StringOrNothingType,
                "display_name": "Source URL",
                "short_desc": "URL to the source PDF file",
                "desc": "URL to the source PDF file",
                "required_groups": ("source",),
            },
            "annotations": {
                "type": new SoftListDataType("TextAnnotations",
                    new PdfCoTextAnnotationDataType(), True),
                "display_name": "Annotations",
                "short_desc": "Text and form annotations to add",
                "desc": "Text and form annotations to add to the PDF",
            },
            "images": {
                "type": new SoftListDataType("ImageAnnotations",
                    new PdfCoImageAnnotationDataType(), True),
                "display_name": "Images",
                "short_desc": "Images to insert",
                "desc": "Images to insert into the PDF",
            },
            "annotationsString": {
                "type": StringOrNothingType,
                "display_name": "Annotations String",
                "short_desc": "Semicolon-delimited text objects",
                "desc": "Semicolon-delimited text objects (alternative to `annotations`).\n\n"
                    "**Format**: `x;y;pages;text;size+styles;fontName;color`\n\n"
                    "Styles: `bold`, `italic`, `underline`, `strikeout` (joined with `+`)\n\n"
                    "**Example**: `100;200;0;Hello;14+bold;Arial;#FF0000`",
            },
            "imagesString": {
                "type": StringOrNothingType,
                "display_name": "Images String",
                "short_desc": "Pipe-delimited image objects",
                "desc": "Pipe-delimited image objects (alternative to `images`).\n\n"
                    "**Format**: `x;y;pages;url` — multiple objects separated by `|`\n\n"
                    "**Example**: "
                    "`50;100;0;https://example.com/logo.png|300;100;0;https://example.com/sig.png`",
            },
            "password": {
                "type": StringOrNothingType,
                "display_name": "Password",
                "short_desc": "PDF file password",
                "desc": "PDF file password if the document is protected",
                "sensitive": True,
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

#! PDF.co add text and images action data provider
public class PdfCoAddTextImagesDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-text-images",
            "desc": "Adds text and images to a PDF document",
            "type": "PdfCoAddTextImagesDataProvider",
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
        return "add-text-images";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Adds text and images to a PDF document";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoAddTextImagesRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    # NOTE: pdf/edit/add is the unified PDF.co endpoint for annotations, images, and form fields;
    # the request body (annotations/images fields) determines behavior
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = uploadFileIfNeeded(req);
        if (req.annotations) {
            req.annotations = parseJsonField(req.annotations);
            req.annotations = stripNilFields(req.annotations);
        }
        if (req.images) {
            req.images = parseJsonField(req.images);
            req.images = stripNilFields(req.images);
        }
        return doPost("pdf/edit/add", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
