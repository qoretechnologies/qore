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
                "type": StringType,
                "required": True,
                "display_name": "Image Format",
                "short_desc": "The image format",
                "desc": "The image format of the output files",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "jpg", "display_name": "JPG"},
                    <AllowedValueInfo>{"value": "png", "display_name": "PNG"},
                    <AllowedValueInfo>{
                        "value": "tiff",
                        "display_name": "TIFF",
                        "desc": "One multi-page TIFF file for all the pages",
                    },
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
            "ImageWidth": {
                "type": SoftIntOrNothingType,
                "display_name": "Image Width",
                "short_desc": "The image width in pixels",
                "desc": "The width of the images in pixels, from `10` to `10000`",
                "example_value": 800,
            },
            "ImageHeight": {
                "type": SoftIntOrNothingType,
                "display_name": "Image Height",
                "short_desc": "The image height in pixels",
                "desc": "The height of the images in pixels, from `10` to `10000`",
                "example_value": 600,
            },
            "Scale": {
                "type": SoftIntOrNothingType,
                "display_name": "Scale",
                "short_desc": "The image size in percent of the page size",
                "desc": "The size of the images in percent of the size of the pages at the image resolution, from "
                    "`10` to `300`; it overrides `ImageWidth` and `ImageHeight`.\n\n"
                    "**Formats**: JPG, PNG and TIFF",
                "example_value": 50,
            },
            "FitToSize": {
                "type": SoftBoolOrNothingType,
                "display_name": "Fit to Size",
                "short_desc": "Fit the images within the width and height",
                "desc": "Set to `True` to scale the images to fit within both `ImageWidth` and `ImageHeight`, which "
                    "must both be given, keeping the aspect ratio.\n\n"
                    "**Formats**: JPG, PNG and TIFF",
            },
            "ScaleProportions": {
                "type": SoftBoolOrNothingType,
                "display_name": "Scale Proportions",
                "short_desc": "Keep the aspect ratio of the pages",
                "desc": "Set to `False` to scale the images to `ImageWidth` and `ImageHeight` without keeping the "
                    "aspect ratio of the pages; it is kept by default.\n\n"
                    "**Formats**: WebP",
            },
            "Rotate": {
                "type": StringOrNothingType,
                "display_name": "Rotate Pages",
                "short_desc": "Rotate the pages before converting them",
                "desc": "The rotation of the pages before they are converted.\n\n"
                    "**Formats**: JPG, PNG and TIFF; for WebP, give the angle in degrees, from `-360` to `360`, as "
                    "`Rotate` in `parameters`",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "default",
                        "display_name": "Default",
                        "desc": "The rotation of each page in the PDF (the default)",
                    },
                    <AllowedValueInfo>{"value": "none", "display_name": "None", "desc": "No rotation"},
                    <AllowedValueInfo>{"value": "rotate90", "display_name": "Rotate 90°"},
                    <AllowedValueInfo>{"value": "rotate180", "display_name": "Rotate 180°"},
                    <AllowedValueInfo>{"value": "rotate270", "display_name": "Rotate 270°"},
                ),
            },
            "CropTo": {
                "type": StringOrNothingType,
                "display_name": "Crop to Box",
                "short_desc": "The page box that the images show",
                "desc": "The page box that the images are cropped to.\n\n"
                    "**Formats**: JPG, PNG and TIFF",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "BoundingBox",
                        "display_name": "Bounding Box",
                        "desc": "The content of the page (the default)",
                    },
                    <AllowedValueInfo>{"value": "TrimBox", "display_name": "Trim Box"},
                    <AllowedValueInfo>{"value": "MediaBox", "display_name": "Media Box"},
                    <AllowedValueInfo>{"value": "ArtBox", "display_name": "Art Box"},
                    <AllowedValueInfo>{"value": "BleedBox", "display_name": "Bleed Box"},
                ),
            },
            "BackgroundColor": {
                "type": StringOrNothingType,
                "display_name": "Background Color",
                "short_desc": "The background color of transparent pages",
                "desc": "The background color of the images where the pages are transparent: a color name, RGB values "
                    "or a hexadecimal color code, or `transparent` to keep the transparency; the default is "
                    "`white`.\n\n"
                    "**Examples**: `black`, `255,0,0` or `#FF0000`\n\n"
                    "**Formats**: JPG, PNG and TIFF",
                "example_value": "#FFFFFF",
            },
            "TransparentColor": {
                "type": StringOrNothingType,
                "display_name": "Transparent Color",
                "short_desc": "The color to make transparent",
                "desc": "The color that is made transparent in the images: a color name, RGB values with an optional "
                    "alpha value, or a hexadecimal color code.\n\n"
                    "**Examples**: `white`, `255,255,255` or `255,255,255,150`\n\n"
                    "**Formats**: WebP",
                "example_value": "white",
            },
            "ImageQuality": {
                "type": SoftIntOrNothingType,
                "display_name": "Image Quality",
                "short_desc": "The image quality in percent",
                "desc": "The quality of the images in percent, from `10` to `100`; the default is `75`.\n\n"
                    "**Formats**: JPG and WebP",
                "example_value": 90,
            },
            "ColorSpace": {
                "type": StringOrNothingType,
                "display_name": "Color Space",
                "short_desc": "The color space of the images",
                "desc": "The color space of the images.\n\n"
                    "**Formats**: JPG and PNG",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "rgb", "display_name": "RGB", "desc": "The default"},
                    <AllowedValueInfo>{"value": "cmyk", "display_name": "CMYK"},
                    <AllowedValueInfo>{"value": "gray", "display_name": "Gray"},
                    <AllowedValueInfo>{"value": "monochrome", "display_name": "Monochrome"},
                ),
            },
            "ColorMode": {
                "type": StringOrNothingType,
                "display_name": "Color Mode",
                "short_desc": "The color mode of the TIFF file",
                "desc": "The color mode of the TIFF file.\n\n"
                    "**Formats**: TIFF",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "default", "display_name": "Default", "desc": "The default"},
                    <AllowedValueInfo>{"value": "cmyk", "display_name": "CMYK"},
                    <AllowedValueInfo>{"value": "grayscale", "display_name": "Grayscale"},
                    <AllowedValueInfo>{"value": "bitonal", "display_name": "Bitonal"},
                ),
            },
            "BitDepth": {
                "type": StringOrNothingType,
                "display_name": "Bit Depth",
                "short_desc": "The number of bits per color channel",
                "desc": "The number of bits per color channel; more bits give more precise colors and smoother "
                    "gradients but larger files.\n\n"
                    "**Formats**: JPG, PNG and TIFF",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "default", "display_name": "Default", "desc": "The default"},
                    <AllowedValueInfo>{"value": "1", "display_name": "1 Bit"},
                    <AllowedValueInfo>{"value": "2", "display_name": "2 Bits"},
                    <AllowedValueInfo>{"value": "4", "display_name": "4 Bits"},
                    <AllowedValueInfo>{"value": "8", "display_name": "8 Bits"},
                    <AllowedValueInfo>{"value": "16", "display_name": "16 Bits"},
                ),
            },
            "CompressionLevel": {
                "type": SoftIntOrNothingType,
                "display_name": "Compression Level",
                "short_desc": "The compression level, from 0 to 9",
                "desc": "The compression level, from `0` to `9`; a higher level gives smaller files, and for JPG "
                    "also a lower image quality.\n\n"
                    "**Formats**: JPG, PNG and TIFF",
                "example_value": 6,
            },
            "ImageAntialiasing": {
                "type": SoftIntOrNothingType,
                "display_name": "Antialiasing",
                "short_desc": "The subsampling box size for antialiasing",
                "desc": "The subsampling box size for the antialiasing of text and graphics, from `0` to `8`; the "
                    "default, `8`, gives the best text, and smaller values render faster.\n\n"
                    "**Formats**: WebP",
                "example_value": 4,
            },
            "MultiPage": {
                "type": SoftBoolOrNothingType,
                "display_name": "Multi-Page File",
                "short_desc": "Create one multi-page TIFF file",
                "desc": "Set to `False` to create a TIFF file for each page; by default, all pages are in one "
                    "multi-page TIFF file.\n\n"
                    "**Formats**: TIFF",
            },
        } + ConvertApiOutputFields;

        #! The image formats of the parameters that only some image formats take
        /** The other fields apply to all formats.
        */
        const FormatFields = {
            "Scale": ("jpg", "png", "tiff"),
            "FitToSize": ("jpg", "png", "tiff"),
            "ScaleProportions": ("webp",),
            "Rotate": ("jpg", "png", "tiff"),
            "CropTo": ("jpg", "png", "tiff"),
            "BackgroundColor": ("jpg", "png", "tiff"),
            "TransparentColor": ("webp",),
            "ImageQuality": ("jpg", "webp"),
            "ColorSpace": ("jpg", "png"),
            "ColorMode": ("tiff",),
            "BitDepth": ("jpg", "png", "tiff"),
            "CompressionLevel": ("jpg", "png", "tiff"),
            "ImageAntialiasing": ("webp",),
            "MultiPage": ("tiff",),
        };
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertPdfToImageRequest") {
        addQoreFields(Fields);
    }

    #! Returns the image formats of the action
    static list<auto> getFormats() {
        return map $1.value, Fields.format.allowed_values;
    }

    #! Returns @ref True if a field applies to an image format
    /** @param field the field name
        @param format the image format

        @return @ref True if the field applies to the format: a field of all formats, or a parameter of the format
    */
    static bool isFormatField(string field, string format) {
        return !FormatFields{field} || inlist(format, FormatFields{field});
    }

    #! Returns the request type for an image format, with the fields that apply to it
    /** @param format the image format

        @return the request type, or @ref nothing if the format is not one of the image formats of the action
    */
    static *HashDataType getFormatRequestType(auto format) {
        if (format.typeCode() != NT_STRING || !inlist(format.lwr(),
                ConvertApiConvertPdfToImageRequestDataType::getFormats())) {
            return;
        }
        string fmt = format.lwr();
        HashDataType rv("ConvertApiConvertPdfToImageRequest-" + fmt);
        rv.addQoreFields(map {$1.key: $1.value}, Fields.pairIterator(),
            ConvertApiConvertPdfToImageRequestDataType::isFormatField($1.key, fmt));
        return rv;
    }
}

#! ConvertAPI convert-pdf-to-image action data provider
/** Uses the <tt>pdf</tt> to <tt>jpg</tt> converter, or the converter for the image format given in the \c format field.

    The converters of the image formats take different parameters, so the action's options depend on the format:
    the parameters that only other formats take are left out (see
    @ref ConvertApiConvertPdfToImageRequestDataType::FormatFields).
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

        #! The request type
        static ConvertApiConvertPdfToImageRequestDataType RequestType();
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
        return RequestType;
    }

    #! Returns the request type for the image format given by the \c format option value
    /** @param options the option values given so far

        @return the request type with the fields that apply to the format, or the request type of the action if no
        image format is given
    */
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return ConvertApiConvertPdfToImageRequestDataType::getFormatRequestType(options.format) ?? RequestType;
    }

    #! Returns the request type for the image format of a request, and checks the request with it
    /** @param req the request

        @return the request type with the fields that apply to the format of the request (JPG if it gives none)

        @throw TYPE-VALUE-ERROR a field of the request has a value that its type does not accept, or is a parameter
        that only other image formats take; a parameter without a field, such as the \c Rotate angle of the WebP
        converter, is given in the \c parameters field
    */
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        AbstractDataProviderType rv = RequestType;
        if (req.typeCode() == NT_HASH) {
            rv = ConvertApiConvertPdfToImageRequestDataType::getFormatRequestType(req.format ?? "jpg")
                ?? RequestType;
        }
        if (exists req) {
            try {
                rv.acceptsValue(req);
            } catch (hash<ExceptionInfo> ex) {
                # the data can be partial
                if (ex.err != "MISSING-VALUE-ERROR") {
                    rethrow;
                }
            }
        }
        return rv;
    }

    #! Returns the options of the action for the option values given so far
    /** When an image format is given, the options are the registered options that apply to the format (see
        @ref ConvertApiConvertPdfToImageRequestDataType::isFormatField()); otherwise all registered options.

        @param action the action
        @param options the option values given so far

        @return the action options
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForRequestWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        if (options.format.typeCode() != NT_STRING || !inlist(options.format.lwr(),
                ConvertApiConvertPdfToImageRequestDataType::getFormats())) {
            return action.options;
        }
        string fmt = options.format.lwr();
        return map {$1.key: $1.value}, action.options.pairIterator(),
            ConvertApiConvertPdfToImageRequestDataType::isFormatField($1.key, fmt);
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
