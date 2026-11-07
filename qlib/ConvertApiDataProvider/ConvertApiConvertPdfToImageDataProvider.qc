# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertPdfToImageDataProvider.qc ConvertAPI convert-pdf-to-image action data provider

/*  ConvertApiConvertPdfToImageDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the convert-pdf-to-image action
public class ConvertApiConvertPdfToImageRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
            "format": {
                "type": StringOrNothingType,
                "display_name": "Image Format",
                "short_desc": "The image format",
                "desc": "The image format of the output files; one image is created for each page",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "jpg", "display_name": "JPG"},
                    <AllowedValueInfo>{"value": "png", "display_name": "PNG"},
                    <AllowedValueInfo>{"value": "tiff", "display_name": "TIFF"},
                    <AllowedValueInfo>{"value": "webp", "display_name": "WebP"},
                ),
                "default_value": "jpg",
            },
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
                "short_desc": "The password to open a protected PDF",
                "desc": "The password to open a protected PDF",
                "sensitive": True,
            },
            "ImageResolution": {
                "type": SoftIntOrNothingType,
                "display_name": "Image Resolution",
                "short_desc": "The image resolution in DPI",
                "desc": "The image resolution in DPI; the default is `200`",
                "example_value": 300,
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertPdfToImageRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI convert-pdf-to-image action data provider
/** Uses the <tt>pdf</tt> to <tt>jpg</tt> converter, or the converter for the image format given in the \c format field.
*/
public class ConvertApiConvertPdfToImageDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-pdf-to-image",
            "desc": "Converts the pages of a PDF to images (JPG, PNG, TIFF or WebP)",
            "type": "ConvertApiConvertPdfToImageDataProvider",
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
        return "convert-pdf-to-image";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts the pages of a PDF to images (JPG, PNG, TIFF or WebP)";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConvertApiConvertPdfToImageRequestDataType();
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "pdf", "to": req.format ?? "jpg"};
    }

    #! Returns the request fields that are not converter parameters
    private *list<string> getNonParameterFields() {
        return ("format",);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
