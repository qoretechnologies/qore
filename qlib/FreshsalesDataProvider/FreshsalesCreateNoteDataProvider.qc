# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesCreateNoteDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace FreshsalesDataProvider {

public class FreshsalesCreateNoteDataProvider inherits FreshsalesDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new note in Freshsales CRM",
            "type": "FreshsalesCreateNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    string getName() { return "create"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new FreshsalesCreateNoteRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FreshsalesNoteDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("notes", {"note": req});
        return result.note ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
