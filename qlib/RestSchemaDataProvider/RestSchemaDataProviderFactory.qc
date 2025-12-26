# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaDataProvider module definition

/** RestSchemaDataProviderFactory.qc Copyright 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaDataProvider module
public namespace RestSchemaDataProvider {
#! The REST schema data provider factory
public class RestSchemaDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("RestSchemaDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "restschema",
            "desc": "REST schema data provider factory",
            "children_can_support_apis": True,
        };
    }

    #! Returns static factory information without \a provider_info
    /** @return static factory information without \a provider_info; no provider is created to return this information
    */
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    /** @note the \c name and \c children attributes are not returned as they are dynamic attributes
    */
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return RestSchemaDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /** @param example example data of the type accepted by the data provider
        @param constructor_options the constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid options or missing or unknown required option
    */
    private AbstractDataProvider getExampleProviderImpl(InputStream example, *hash<auto> constructor_options) {
        return new RestSchemaDataProvider(constructor_options + {"schema": example.read(-1)});
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /** @param example example data of the type accepted by the data provider
        @param constructor_options the constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid options or missing or unknown required option
    */
    private AbstractDataProvider getExampleProviderImpl(string example, *hash<auto> constructor_options) {
        return new RestSchemaDataProvider(constructor_options + {"schema": example});
    }
}
}
