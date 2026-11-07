# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertDataProvider.qc ConvertAPI generic convert action data provider

/*  ConvertApiConvertDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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


#! Request fields for the source and destination formats of a converter
public const ConvertApiFormatFields = {
    "from": {
        "type": StringType,
        "display_name": "Source Format",
        "short_desc": "The source format of the converter",
        "desc": "The source format of the converter, for example `docx`, `html`, `pdf`, `jpg` or `eml`.\n\n"
            "The list of converters is in the [ConvertAPI documentation](https://www.convertapi.com/doc).",
        "example_value": "docx",
    },
    "to": {
        "type": StringType,
        "display_name": "Destination Format",
        "short_desc": "The destination format of the converter",
        "desc": "The destination format of the converter, for example `pdf`, `jpg` or `txt`.\n\n"
            "Some converters are PDF tools, whose destination format names the operation, for example `pdf` to "
            "`merge`, `split`, `compress`, `protect`, `ocr` or `pdfa`.",
        "example_value": "pdf",
    },
};

#! Request type for the generic convert action
public class ConvertApiConvertRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSourceFields + ConvertApiFormatFields + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI generic convert action data provider
/** Converts files with any ConvertAPI converter given by its source and destination formats.
*/
public class ConvertApiConvertDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert",
            "desc": "Converts files with any ConvertAPI converter, given by its source and destination formats",
            "type": "ConvertApiConvertDataProvider",
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
        return "convert";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts files with any ConvertAPI converter, given by its source and destination formats";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConvertApiConvertRequestDataType();
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return req{"from", "to"};
    }

    #! Returns the request fields that are not converter parameters
    private *list<string> getNonParameterFields() {
        return ("from", "to");
    }

    #! Returns the name of the parameter for the \c url field
    /** The \c html converters render the web page given with the \c Url parameter; all other converters download
        the source file from the URL
    */
    private *string getUrlParameter(hash<auto> req) {
        if (req.from.lwr() == "html") {
            return "Url";
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
