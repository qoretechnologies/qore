# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertPowerPointToPdfDataProvider.qc ConvertAPI convert-powerpoint-to-pdf action data provider

/*  ConvertApiConvertPowerPointToPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the convert-powerpoint-to-pdf action
public class ConvertApiConvertPowerPointToPdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
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
                "short_desc": "The password to open a protected presentation",
                "desc": "The password to open a protected presentation",
                "sensitive": True,
            },
            "ConvertHiddenSlides": {
                "type": SoftBoolOrNothingType,
                "display_name": "Convert Hidden Slides",
                "short_desc": "Include the hidden slides",
                "desc": "Set to `True` to include the hidden slides in the PDF",
            },
            "ConvertSpeakerNotes": {
                "type": StringOrNothingType,
                "display_name": "Convert Speaker Notes",
                "short_desc": "How speaker notes are converted",
                "desc": "How the speaker notes of the slides are converted",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "Disabled",
                        "display_name": "Disabled",
                        "desc": "Speaker notes are not converted (the default)",
                    },
                    <AllowedValueInfo>{
                        "value": "SeparatePage",
                        "display_name": "Convert to Separate Page",
                        "desc": "The notes of each slide on a separate page",
                    },
                    <AllowedValueInfo>{
                        "value": "PageComments",
                        "display_name": "Convert to Page Comments",
                        "desc": "The notes as page comments",
                    },
                ),
            },
            "Pdfa": {
                "type": SoftBoolOrNothingType,
                "display_name": "Create PDF/A",
                "short_desc": "Create a PDF/A-3a compliant document",
                "desc": "Set to `True` to create a PDF/A-3a compliant document for long-term archiving",
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertPowerPointToPdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI convert-powerpoint-to-pdf action data provider
/** Uses the <tt>pptx</tt> to <tt>pdf</tt> converter.
*/
public class ConvertApiConvertPowerPointToPdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-powerpoint-to-pdf",
            "desc": "Converts a PowerPoint presentation (PPT or PPTX) to PDF",
            "type": "ConvertApiConvertPowerPointToPdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiConvertPowerPointToPdfRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "convert-powerpoint-to-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts a PowerPoint presentation (PPT or PPTX) to PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "pptx", "to": "pdf"};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
