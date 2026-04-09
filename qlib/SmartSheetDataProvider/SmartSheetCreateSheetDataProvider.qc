# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetCreateSheetDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Smartsheet create sheet action data provider
public class SmartSheetCreateSheetDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetCreateSheetRequestDataType RequestType();
        static SmartSheetSheetDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new Smartsheet sheet with the given name and column definitions",
            "type": "SmartSheetCreateSheetDataProvider",
            "supports_request": True,
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return ProviderInfo.desc;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("sheets", req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SmartSheetDataProvider
