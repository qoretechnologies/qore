# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiDataTypes.qc ConvertAPI data provider response types

/*  ConvertApiDataTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! A list of strings in a converter description (formats, extensions, tags)
public const ConvertApiStringListType = new SoftListDataType("ConvertApiStringList", StringType, True);

#! Data type for one output file of a conversion
public class ConvertApiFileResultDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "FileName": {
                "type": StringOrNothingType,
                "display_name": "File Name",
                "short_desc": "The name of the output file",
                "desc": "The name of the output file, with the extension of the destination format",
            },
            "FileExt": {
                "type": StringOrNothingType,
                "display_name": "File Extension",
                "short_desc": "The extension of the output file",
                "desc": "The extension of the output file, for example `pdf`",
            },
            "FileSize": {
                "type": SoftIntOrNothingType,
                "display_name": "File Size",
                "short_desc": "The size of the output file in bytes",
                "desc": "The size of the output file in bytes",
            },
            "FileId": {
                "type": StringOrNothingType,
                "display_name": "File ID",
                "short_desc": "The ID of the stored output file",
                "desc": "The ID of the output file in ConvertAPI's temporary storage; only set when `store_file` "
                    "is `True`",
            },
            "Url": {
                "type": UriOrNothingType,
                "display_name": "Download URL",
                "short_desc": "The download URL of the stored output file",
                "desc": "The download URL of the output file; only set when `store_file` is `True`.\n\n"
                    "The file can be downloaded with the same API token for up to **3 hours**.",
            },
            "FileData": {
                "type": BinaryOrNothingType,
                "display_name": "File Data",
                "short_desc": "The content of the output file",
                "desc": "The content of the output file as binary data; only set when `store_file` is `False` "
                    "(the default)",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiFileResult") {
        addQoreFields(Fields);
    }
}

#! The output file data type
public const ConvertApiFileResultDataType = new ConvertApiFileResultDataType();

#! Response data type for conversions
public class ConvertApiConversionResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ConversionCost": {
                "type": SoftIntOrNothingType,
                "display_name": "Conversion Cost",
                "short_desc": "The number of conversions charged for the request",
                "desc": "The number of conversions charged to the ConvertAPI account for this request",
            },
            "Files": {
                "type": new ListDataType("ConvertApiFileResultList", ConvertApiFileResultDataType, True),
                "display_name": "Files",
                "short_desc": "The output files",
                "desc": "The output files; a conversion can create several files, for example one image for "
                    "each page of a PDF",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConversionResponse") {
        addQoreFields(Fields);
    }
}

#! Data type for a converter parameter
public class ConvertApiConverterParameterDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The parameter name",
                "desc": "The parameter name, as used in the `parameters` request field",
            },
            "Label": {
                "type": StringOrNothingType,
                "display_name": "Label",
                "short_desc": "The display label of the parameter",
                "desc": "The display label of the parameter",
            },
            "Description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The description of the parameter",
                "desc": "The description of the parameter (markdown)",
            },
            "GroupName": {
                "type": StringOrNothingType,
                "display_name": "Group Name",
                "short_desc": "The parameter group",
                "desc": "The name of the parameter group, for example `Input`, `Output` or `PDF Output`",
            },
            "Type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The parameter type",
                "desc": "The parameter type: one of `String`, `Integer`, `Double`, `Bool`, `Collection` (one of "
                    "the `Values`), `Color`, `Uri` or `File`",
            },
            "Required": {
                "type": SoftBoolOrNothingType,
                "display_name": "Required",
                "short_desc": "Whether the parameter is required",
                "desc": "`True` if the parameter is required",
            },
            "Array": {
                "type": SoftBoolOrNothingType,
                "display_name": "Array",
                "short_desc": "Whether the parameter takes a list of values",
                "desc": "`True` if the parameter takes a list of values",
            },
            "Default": {
                "type": AutoType,
                "display_name": "Default",
                "short_desc": "The default value of the parameter",
                "desc": "The default value of the parameter, if any",
            },
            "Values": {
                "type": AutoHashOrNothingType,
                "display_name": "Values",
                "short_desc": "The allowed values of a Collection parameter",
                "desc": "The allowed values of a `Collection` parameter, as a hash of values to labels",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConverterParameter") {
        addQoreFields(Fields);
    }
}

#! The converter parameter data type
public const ConvertApiConverterParameterDataType = new ConvertApiConverterParameterDataType();

#! Response data type for converter information
public class ConvertApiConverterInfoResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The converter name",
                "desc": "The converter name, for example `Word`",
            },
            "Title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The converter title",
                "desc": "The converter title, for example `DOCX to PDF API`",
            },
            "Description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The converter description",
                "desc": "The converter description",
            },
            "ConverterGroup": {
                "type": StringOrNothingType,
                "display_name": "Converter Group",
                "short_desc": "The converter group",
                "desc": "The converter group, for example `Microsoft Office` or `PDF Tools`",
            },
            "Tags": {
                "type": ConvertApiStringListType,
                "display_name": "Tags",
                "short_desc": "The converter tags",
                "desc": "The converter tags, for example `document`, `office` and `pdf`",
            },
            "SourceFileFormats": {
                "type": ConvertApiStringListType,
                "display_name": "Source File Formats",
                "short_desc": "The source formats of the converter",
                "desc": "The source formats of the converter",
            },
            "SourceExtensions": {
                "type": ConvertApiStringListType,
                "display_name": "Source Extensions",
                "short_desc": "The file extensions accepted by the converter",
                "desc": "The file name extensions accepted by the converter, for example `doc` and `docx`",
            },
            "DestinationFileFormats": {
                "type": ConvertApiStringListType,
                "display_name": "Destination File Formats",
                "short_desc": "The destination formats of the converter",
                "desc": "The destination formats of the converter",
            },
            "Parameters": {
                "type": new ListDataType("ConvertApiConverterParameterList", ConvertApiConverterParameterDataType,
                    True),
                "display_name": "Parameters",
                "short_desc": "The converter parameters",
                "desc": "The parameters of the converter; the authentication parameters are not included, as the "
                    "API token of the connection is used",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConverterInfoResponse") {
        addQoreFields(Fields);
    }
}

} # namespace ConvertApiDataProvider
