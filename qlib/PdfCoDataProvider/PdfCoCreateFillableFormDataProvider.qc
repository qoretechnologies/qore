# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoCreateFillableFormDataProvider.qc PDF.co create fillable form action data provider

/*  PdfCoCreateFillableFormDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating fillable PDF forms
public class PdfCoCreateFillableFormRequestDataType inherits HashDataType {
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
                "type": new SoftListDataType("FormFieldDefinitions",
                    new PdfCoFormFieldDefinitionDataType(), False),
                "display_name": "Form Fields",
                "short_desc": "Form field definitions to add",
                "desc": "Form field definitions to add to the PDF",
                "required": True,
                "preselected": True,
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

#! PDF.co create fillable form action data provider
public class PdfCoCreateFillableFormDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-fillable-form",
            "desc": "Adds form fields to a PDF document to make it fillable",
            "type": "PdfCoCreateFillableFormDataProvider",
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
        return "create-fillable-form";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Adds form fields to a PDF document to make it fillable";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoCreateFillableFormRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    # NOTE: pdf/edit/add is the unified PDF.co endpoint for annotations, images, and form fields;
    # the request body (annotations field) determines behavior
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = uploadFileIfNeeded(req);
        if (req.annotations) {
            req.annotations = parseJsonField(req.annotations);
            req.annotations = expandTitleAnnotations(req.annotations);
            req.annotations = stripNilFields(req.annotations);
        }
        return doPost("pdf/edit/add", req);
    }

    #! Expands form field definitions with title hashes into separate text annotations
    private static list<auto> expandTitleAnnotations(list<auto> annotations) {
        list<auto> result = ();
        foreach hash<auto> ann in (annotations) {
            if (ann.title) {
                float title_x;
                float title_y;
                if (exists ann.title.x && exists ann.title.y) {
                    # User provided explicit coordinates
                    title_x = ann.title.x;
                    title_y = ann.title.y;
                } else {
                    # Auto-position based on field type
                    int font_size = ann.title.size ?? 12;
                    bool is_checkbox = (ann.type == "Checkbox"
                        || ann.type == "CheckboxChecked");
                    if (is_checkbox) {
                        title_x = ann.x + (ann.width ?? 20) + 4;
                        title_y = ann.y;
                    } else {
                        title_x = ann.x;
                        title_y = ann.y - font_size - 4;
                    }
                    # If the title would go off-page, place it below/after the field
                    if (title_y < 0) {
                        title_y = ann.y + (ann.height ?? 20) + 4;
                    }
                    if (title_x < 0) {
                        title_x = ann.x + (ann.width ?? 20) + 4;
                    }
                }
                # Create a text annotation for the title/label
                hash<auto> title_ann = {
                    "type": "Text",
                    "text": ann.title.text,
                    "x": title_x,
                    "y": title_y,
                };
                if (exists ann.pages) {
                    title_ann.pages = ann.pages;
                }
                # Copy optional styling from title
                foreach string key in ("size", "fontName", "fontBold", "fontItalic",
                        "fontUnderline", "fontStrikeout", "color") {
                    if (exists ann.title{key}) {
                        title_ann{key} = ann.title{key};
                    }
                }
                result += title_ann;
                # Remove title from the field annotation before sending
                delete ann.title;
            }
            result += ann;
        }
        return result;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
