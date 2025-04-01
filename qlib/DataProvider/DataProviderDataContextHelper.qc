# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderDataContextHelper class definition

/** DataProviderDataContextHelper.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Current data provider data context
thread_local auto _thread_data_ctx;

#! Data provider context helper
public class DataProviderDataContextHelper {
    public {}

    private {
        auto old;
    }

    constructor(auto dat) {
        old = dat;
        _thread_data_ctx = dat;
    }

    destructor() {
        _thread_data_ctx = old;
    }

    static auto get() {
        return _thread_data_ctx;
    }

    static *hash<auto> getHash() {
        return _thread_data_ctx.typeCode() == NT_HASH ? _thread_data_ctx : NOTHING;
    }
}
}
