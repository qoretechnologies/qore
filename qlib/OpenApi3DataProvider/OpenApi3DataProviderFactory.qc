# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenApi3DataProviderFactory class definition

/** OpenApi3DataProviderFactory.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenApi3DataProvider module
public namespace OpenApi3DataProvider {
#! The OpenAPI 3 data provider factory
public class OpenApi3DataProviderFactory inherits AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("OpenApi3DataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "openapi3",
            "desc": "OpenAPI 3.0 / 3.1 API data provider factory",
            "api_management": True,
            "children_can_support_apis": True,
            "api_profiles": "rest",
        };
    }

    #! Returns static factory information without \a provider_info
    /** @return static factory information without \a provider_info which is provided by \c getProviderInfo()
    */
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    /** @note the \c name and \c children attributes are not returned as they are dynamic attributes
    */
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return OpenApi3DataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }
}
}
