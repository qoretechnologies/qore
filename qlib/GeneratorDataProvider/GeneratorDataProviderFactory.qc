# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeneratorDataProviderFactory class definition

/** GeneratorDataProviderFactory.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeneratorDataProvider module
public namespace GeneratorDataProvider {

#! The data generator data provider factory: \c generator
public class GeneratorDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("GeneratorDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "generator",
            "desc": "Data generator data provider factory for generating test record streams",
            "children_can_support_records": True,
        };
    }

    #! Returns static factory information without \a provider_info
    private hash<DataProvider::DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    private hash<DataProvider::DataProviderInfo> getProviderInfoImpl() {
        return GeneratorDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Qore::Reflection::Class getClassImpl() {
        return cls;
    }
}
}
