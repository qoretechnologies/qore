# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CsvProfileRequestDataType class definition

/*  CsvProfileRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The primary CsvUtil namespace; all classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
#! The request of the CSV \c profile action
/** @since CsvUtil 1.12
*/
public class CsvProfileRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "path": {
                "display_name": "CSV Input Location",
                "short_desc": "The location of the CSV file to profile",
                "type": AbstractDataProviderType::get(StringOrNothingType, NOTHING, {
                    DTT_FromLocation: True,
                }),
                "desc": "The location of the CSV file to profile, for example `file:///data/orders.csv` or an "
                    "`sftp://` location; conflicts with `data`",
                "example_value": "file:///data/po-confirmations.csv",
            },
            "data": {
                "display_name": "CSV Data",
                "short_desc": "The CSV content to profile",
                "type": AbstractDataProviderTypeMap."*data",
                "desc": "The CSV content to profile, as text or bytes; conflicts with `path`",
                "example_value": "PO,SKU,Qty\nPO-1001,A-100,5\n",
            },
            "separator": {
                "display_name": "Separator",
                "short_desc": "The field separator, instead of detecting it",
                "type": StringOrNothingType,
                "desc": "The field separator; if not given, it is detected from the data",
                "example_value": ";",
            },
            "encoding": {
                "display_name": "Encoding",
                "short_desc": "The character encoding, instead of detecting it",
                "type": StringOrNothingType,
                "desc": "The character encoding; if not given, a byte order mark or valid UTF-8 is detected, and "
                    "other data is read as `ISO-8859-1`",
                "example_value": "windows-1250",
            },
            "quote": {
                "display_name": "Quote Character",
                "short_desc": "The field quote character, instead of the double quote",
                "type": StringOrNothingType,
                "desc": "The field quote character (default: `\"`); fields are read with it, and separators inside "
                    "quoted fields are not counted when the separator is detected",
                "example_value": "'",
            },
            "max_rows": {
                "display_name": "Maximum Rows",
                "short_desc": "The maximum number of rows read",
                "type": SoftIntOrNothingType,
                "desc": "The maximum number of rows read and profiled; the total row count is still reported, and "
                    "a `partial_profile` finding notes that the file has more rows",
                "default_value": CsvProfiler::DefaultMaxRows,
            },
            "sample_rows": {
                "display_name": "Sample Rows",
                "short_desc": "The number of sample rows returned",
                "type": SoftIntOrNothingType,
                "desc": "The number of sample data rows returned",
                "default_value": 5,
            },
            "redact": {
                "display_name": "Redact Samples",
                "short_desc": "Return sample values redacted to their shape",
                "type": SoftBoolOrNothingType,
                "desc": "If `True` (the default), sample values and counter-examples are redacted to their shape: "
                    "letters become `x` or `X` and digits `9`, so a profile can be shared without the data",
                "default_value": True,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "CsvProfileRequest") {
        addQoreFields(Fields);
    }
}
}
