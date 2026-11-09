# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiCompressPdfDataProvider.qc ConvertAPI compress-pdf action data provider

/*  ConvertApiCompressPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the compress-pdf action
public class ConvertApiCompressPdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
            "Preset": {
                "type": StringOrNothingType,
                "display_name": "Preset",
                "short_desc": "The compression preset",
                "desc": "The compression preset; when a preset other than `none` is given, the other compression "
                    "parameters are ignored",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "Not Set (Advanced Parameters)"},
                    <AllowedValueInfo>{"value": "lossless", "display_name": "Lossless"},
                    <AllowedValueInfo>{"value": "text", "display_name": "Text (20 DPI Images)"},
                    <AllowedValueInfo>{"value": "archive", "display_name": "Archive (40 DPI Images)"},
                    <AllowedValueInfo>{"value": "web", "display_name": "Web (96 DPI Images)"},
                    <AllowedValueInfo>{"value": "ebook", "display_name": "Ebook (150 DPI Images)"},
                    <AllowedValueInfo>{"value": "printer", "display_name": "Printer (300 DPI Images)"},
                ),
                "example_value": "web",
            },
            "Password": {
                "type": StringOrNothingType,
                "display_name": "Open Password",
                "short_desc": "The password to open a protected PDF",
                "desc": "The password to open a protected PDF",
                "sensitive": True,
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiCompressPdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI compress-pdf action data provider
/** Uses the <tt>pdf</tt> to <tt>compress</tt> converter.
*/
public class ConvertApiCompressPdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "compress-pdf",
            "desc": "Compresses a PDF to reduce its size",
            "type": "ConvertApiCompressPdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiCompressPdfRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "compress-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Compresses a PDF to reduce its size";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "pdf", "to": "compress"};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
