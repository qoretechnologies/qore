# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractTabularProfileDataProvider class definition

/** AbstractTabularProfileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! The request of a spreadsheet \c profile action
/** @see AbstractTabularProfileDataProvider

    @since %DataProvider 3.9
*/
public class TabularProfileRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "path": {
                "display_name": "Spreadsheet Location",
                "short_desc": "The location of the spreadsheet to profile",
                "type": StringOrNothingType,
                "desc": "The location of the spreadsheet to profile, for example `file:///data/stock.xlsx` or an "
                    "`sftp://` location; conflicts with `data`",
                "example_value": "file:///data/po-confirmations.xlsx",
            },
            "data": {
                "display_name": "Spreadsheet Data",
                "short_desc": "The spreadsheet content to profile",
                "type": AbstractDataProviderTypeMap."*base64binary",
                "desc": "The spreadsheet content to profile, as bytes or as a Base64 string; conflicts with `path`",
            },
            "max_rows": {
                "display_name": "Maximum Rows",
                "short_desc": "The maximum number of rows read per sheet",
                "type": SoftIntOrNothingType,
                "desc": "The maximum number of rows read and profiled per sheet; the total row count is still "
                    "reported, and a `partial_profile` finding notes that the sheet has more rows",
                "default_value": AbstractTabularProfileDataProvider::DefaultMaxRows,
            },
            "sample_rows": {
                "display_name": "Sample Rows",
                "short_desc": "The number of sample rows returned per sheet",
                "type": SoftIntOrNothingType,
                "desc": "The number of sample data rows returned per sheet",
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
    constructor() : HashDataType(AutoHashType, "TabularProfileRequest") {
        addQoreFields(Fields);
    }
}

#! Base class for the data providers of spreadsheet \c profile actions
/** A subclass reads its format as a raw grid of cells; this class handles the request and profiles the grid with
    @ref TabularProfiler.

    @since %DataProvider 3.9
*/
public class AbstractTabularProfileDataProvider inherits AbstractDataProvider {
    public {
        #! The default maximum number of rows read per sheet
        const DefaultMaxRows = 10000;

        #! Request type
        const RequestType = new TabularProfileRequestDataType();
    }

    #! Creates the object
    constructor(*hash<auto> options) {
    }

    #! Returns the data provider name
    string getName() {
        return "profile";
    }

    #! Reads the content as a raw grid of cells
    /** @param content the file content
        @param max_rows the maximum number of rows read per sheet

        @return the raw grid
    */
    abstract private hash<TabularWorkbookGrid> readGrid(binary content, int max_rows);

    #! Returns the content of the file at a location
    /** @param location the file location

        @return the content
    */
    abstract private binary readLocation(string location);

    #! Profiles a spreadsheet
    /** @param req the request; see @ref TabularProfileRequestDataType
        @param request_options request options

        @return the profile; see @ref TabularWorkbookProfileDataType

        @throw TABULAR-PROFILE-ERROR neither or both of \c path and \c data were given, or \c max_rows is not
        positive
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (exists req.path == exists req.data) {
            throw "TABULAR-PROFILE-ERROR", "exactly one of the \"path\" and \"data\" options must be given";
        }
        int max_rows = req.max_rows ?? DefaultMaxRows;
        if (max_rows <= 0) {
            throw "TABULAR-PROFILE-ERROR", sprintf("max_rows must be positive; got %d", max_rows);
        }
        binary content = exists req.path ? readLocation(req.path) : req.data;
        return TabularProfiler::profile(readGrid(content, max_rows), <TabularProfileOptions>{
            "sample_rows": req.sample_rows ?? 5,
            "redact": req.redact ?? True,
        });
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return TabularWorkbookProfileDataType;
    }
}
}
