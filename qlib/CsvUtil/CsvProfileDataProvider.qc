# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CsvProfileDataProvider class definition

/*  CsvProfileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Profiles CSV data: detects its format and describes its layout and column types
/** @see CsvProfiler

    @since CsvUtil 1.12
*/
public class CsvProfileDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "profile",
            "desc": "Profiles CSV data: detects its separator, encoding, and line ending, and describes its header, "
                "data region, totals rows, and column types",
            "type": "CsvProfileDataProvider",
            "supports_request": True,
        };

        #! Request type
        const RequestType = new CsvProfileRequestDataType();
    }

    #! Creates the object
    constructor(*hash<auto> options) {
    }

    #! Returns the data provider name
    string getName() {
        return "profile";
    }

    #! Profiles CSV data
    /** @param req the request; see @ref CsvProfileRequestDataType
        @param request_options request options

        @return the profile; see @ref DataProvider::TabularWorkbookProfileDataType "TabularWorkbookProfileDataType"

        @throw CSV-PROFILE-ERROR neither or both of \c path and \c data were given, or \c max_rows is not positive
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (exists req.path == exists req.data) {
            throw "CSV-PROFILE-ERROR", "exactly one of the \"path\" and \"data\" options must be given";
        }
        binary content = exists req.path
            ? FileLocationHandler::getBinaryFileFromLocation(req.path)
            : binary(req.data);
        return CsvProfiler::profile(content, req{"max_rows", "separator", "encoding", "quote"},
            <TabularProfileOptions>{
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

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
