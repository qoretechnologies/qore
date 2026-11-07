# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertExcelToPdfDataProvider.qc ConvertAPI convert-excel-to-pdf action data provider

/*  ConvertApiConvertExcelToPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the convert-excel-to-pdf action
public class ConvertApiConvertExcelToPdfRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiSingleSourceFields + {
            "Password": {
                "type": StringOrNothingType,
                "display_name": "Open Password",
                "short_desc": "The password to open a protected workbook",
                "desc": "The password to open a protected workbook",
                "sensitive": True,
            },
            "WorksheetName": {
                "type": StringOrNothingType,
                "display_name": "Worksheet Name",
                "short_desc": "The name of the worksheet to convert",
                "desc": "The name of the worksheet to convert; all worksheets are converted by default",
            },
            "ActiveWorksheetOnly": {
                "type": SoftBoolOrNothingType,
                "display_name": "Active Worksheet Only",
                "short_desc": "Convert only the active worksheet",
                "desc": "Set to `True` to convert only the active worksheet",
            },
            "PageOrientation": {
                "type": StringOrNothingType,
                "display_name": "Page Orientation",
                "short_desc": "The page orientation",
                "desc": "The page orientation; `default` uses the page setup of each worksheet",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "default", "display_name": "Default"},
                    <AllowedValueInfo>{"value": "portrait", "display_name": "Portrait"},
                    <AllowedValueInfo>{"value": "landscape", "display_name": "Landscape"},
                ),
            },
            "FitToPage": {
                "type": SoftBoolOrNothingType,
                "display_name": "Fit to Page",
                "short_desc": "Scale each sheet to the page width",
                "desc": "Set to `True` to enlarge the page size, switch to landscape when needed, trim the margins "
                    "and scale each sheet to one page width",
            },
            "AutoFitColumns": {
                "type": SoftBoolOrNothingType,
                "display_name": "Auto Fit Columns",
                "short_desc": "Fit the column widths to their content",
                "desc": "Set to `True` to fit the column widths to their content, which removes unnecessary empty "
                    "space",
            },
            "Pdfa": {
                "type": SoftBoolOrNothingType,
                "display_name": "Create PDF/A",
                "short_desc": "Create a PDF/A-1b compliant document",
                "desc": "Set to `True` to create a PDF/A-1b compliant document for long-term archiving",
            },
        } + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiConvertExcelToPdfRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI convert-excel-to-pdf action data provider
/** Uses the <tt>xlsx</tt> to <tt>pdf</tt> converter.
*/
public class ConvertApiConvertExcelToPdfDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-excel-to-pdf",
            "desc": "Converts an Excel workbook (XLS or XLSX) to PDF",
            "type": "ConvertApiConvertExcelToPdfDataProvider",
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
        return "convert-excel-to-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts an Excel workbook (XLS or XLSX) to PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConvertApiConvertExcelToPdfRequestDataType();
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return {"from": "xlsx", "to": "pdf"};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
