# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiDataProviderDefs.qc ConvertAPI data provider definitions

/*  ConvertApiDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "ConvertApi";

    #! Maximum number of retries for transient errors
    public const MaxRetries = 3;

    #! The longest wait in seconds that a rate-limited request (HTTP 429) waits for before it is sent again
    /** If the \c Retry-After header of the response asks for a longer wait, the request is not sent again and the
        error is raised, as a retry after a shorter wait would come before the server permits it
    */
    public const MaxRetryAfter = 60;

    #! The wait in seconds before a rate-limited request is sent again when the response has no \c Retry-After header
    public const DefaultRetryAfter = 5;

    #! How long the converter list from the \c info endpoint is cached
    public const ConverterCacheTtl = 1h;

    #! ConvertAPI's conversion timeout in seconds when a request gives none in its \c timeout field
    public const DefaultConversionTimeout = 900;

    #! The time in seconds added to the conversion timeout for sending a conversion request and receiving its result
    /** A conversion request is sent with a transport deadline of its conversion timeout plus this allowance, or of
        the client's own request timeout if that is longer, so that the client neither gives up on a conversion that
        ConvertAPI is still running nor sends it again.
    */
    public const ConversionTransferAllowance = 120;

    #! Set of retriable transport errors of the synchronous and asynchronous REST clients
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
        "HTTPCLIENT-CONNECT-ERROR": True,
        "HTTPCLIENT-STREAM-CLOSED": True,
        "HTTPCLIENT-TIMEOUT": True,
        "FUTURE-TIMEOUT": True,
    };

    #! ConvertAPI logo in SVG format (loaded from file)
    public const ConvertApiLogo = File::readTextFile(get_script_dir() + "/convertapi-logo.svg");

    #! The names of the request fields handled by the module itself; all other fields are converter parameters
    public const ConvertApiCommonFieldNames = ("file", "url", "files", "urls", "store_file", "file_name",
        "timeout", "parameters");

    #! Request fields for the input file(s) of a conversion
    /** One of these fields must be given (they form the \c source required group); \c file and \c url give a
        single input file, and \c files and \c urls give several input files for converters that take them.
    */
    public const ConvertApiSourceFields = {
        "file": {
            "type": FileOrNothingDataType,
            "display_name": "File",
            "short_desc": "The file to convert",
            "desc": "The file to convert; provide either this or a source URL.\n\n"
                "The file is sent to ConvertAPI in the request; the file name extension must match the source "
                "format of the converter.",
            "required_groups": ("source",),
            "preselected": True,
        },
        "url": {
            "type": UriOrNothingType,
            "display_name": "Source URL",
            "short_desc": "URL of the file to convert",
            "desc": "The URL of the file to convert; ConvertAPI downloads the file from this URL.\n\n"
                "**Example**: `https://cdn.convertapi.com/public/files/demo.docx`",
            "required_groups": ("source",),
        },
        "files": {
            "type": new ListDataType("ConvertApiFileList", FileDataType, True),
            "display_name": "Files",
            "short_desc": "The files to convert for converters that take several files",
            "desc": "The files to convert for converters that take several input files, such as `pdf` to "
                "`merge`, `images` to `pdf` or `any` to `zip`.\n\n"
                "They are sent as the `Files` parameter, together with any files given in `urls`.",
            "required_groups": ("source",),
        },
        "urls": {
            "type": new ListDataType("ConvertApiUrlList", SoftStringType, True),
            "display_name": "Source URLs",
            "short_desc": "URLs of the files for converters that take several files",
            "desc": "The URLs of the files to convert for converters that take several input files, such as "
                "`pdf` to `merge`.\n\n"
                "They are sent as the `Files` parameter, together with any files given in `files`.",
            "required_groups": ("source",),
        },
    };

    #! Request fields for the output of a conversion and the other converter parameters
    public const ConvertApiOutputFields = {
        "store_file": {
            "type": SoftBoolOrNothingType,
            "display_name": "Store File",
            "short_desc": "Return a download URL instead of the file data",
            "desc": "Set to `True` to have ConvertAPI store the output files and return a download `Url` for "
                "each one; the files are kept in encrypted temporary storage for up to **3 hours**.\n\n"
                "If `False` (the default), the output files are returned in the response as `FileData` and "
                "nothing is stored.",
            "default_value": False,
        },
        "file_name": {
            "type": StringOrNothingType,
            "display_name": "Output File Name",
            "short_desc": "The name of the output file without the extension",
            "desc": "The name of the output file(s); ConvertAPI appends the extension of the destination "
                "format, and an index when the conversion creates several files.\n\n"
                "**Example**: `report` gives `report.pdf`, or `report_0.jpg`, `report_1.jpg`, ...",
        },
        "timeout": {
            "type": SoftIntOrNothingType,
            "display_name": "Timeout",
            "short_desc": "The conversion timeout in seconds",
            "desc": "The conversion timeout in seconds, from `10` to `1200`; the default is `900`.",
            "example_value": 300,
        },
        "parameters": {
            "type": AutoHashOrNothingType,
            "display_name": "Converter Parameters",
            "short_desc": "Other converter parameters by their ConvertAPI names",
            "desc": "Other parameters of the converter, as a hash of ConvertAPI parameter names to values.\n\n"
                "The parameters of each converter are listed in the "
                "[ConvertAPI documentation](https://docs.convertapi.com) and returned by the "
                "`get-converter-info` action.\n\n"
                "- Scalar values are sent as they are\n"
                "- A file hash (`name` and `content`) is sent as a file\n"
                "- A list of file hashes is sent as a list of files\n\n"
                "**Example**: `{\"PageRange\": \"1-3\", \"Pdfa\": true}`",
        },
    };

    #! All common request fields of a conversion: @ref ConvertApiSourceFields and @ref ConvertApiOutputFields
    public const ConvertApiCommonFields = ConvertApiSourceFields + ConvertApiOutputFields;

    #! Request fields for a single input file
    public const ConvertApiSingleSourceFields = ConvertApiSourceFields{"file", "url"};

    #! Request fields for several input files
    public const ConvertApiMultiSourceFields = ConvertApiSourceFields{"files", "urls"};
}
