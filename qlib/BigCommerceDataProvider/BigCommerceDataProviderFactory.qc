# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceDataProviderFactory.qc defines the BigCommerceDataProviderFactory class

/*  BigCommerceDataProviderFactory.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! The BigCommerce data provider factory
public class BigCommerceDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "bigcommerce",
            "desc": "BigCommerce data provider factory",
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! Factory info
        static hash<DataProviderFactoryInfo> FactoryInfoVar = FactoryInfo;
    }

    #! Returns factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfoVar;
    }

    #! Returns static provider information
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return BigCommerceDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    Class getClassImpl() {
        return Class::forName("BigCommerceDataProvider");
    }

    #! Returns a data provider given the options
    /** @param options the options for the data provider; will be processed by the constructor
    */
    AbstractDataProvider getProviderFromExampleImpl(hash<auto> options) {
        return new BigCommerceDataProvider(options);
    }

    #! Returns the example constructor options for the data provider
    *hash<auto> getExampleProviderOptions() {
        return BigCommerceDataProviderBase::BaseConstructorOptions;
    }
}

} # namespace BigCommerceDataProvider
