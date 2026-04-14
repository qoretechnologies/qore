# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CircleDataProvider module definition

/*  CircleDataProviderFactory.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! The Circle data provider factory
public class CircleDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "circle",
            "desc": "Circle data provider factory",
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! Factory singleton
        static CircleDataProviderFactory factory;
    }

    #! Returns the singleton factory
    static CircleDataProviderFactory getFactory() {
        if (!factory) {
            factory = new CircleDataProviderFactory();
        }
        return factory;
    }

    #! Creates the object
    constructor() : AbstractDataProviderFactory(FactoryInfo) {
    }

    #! Returns a data provider with the given options
    /** @param constructor_options factory constructor options

        @return the data provider created
    */
    private AbstractDataProvider createImpl(hash<auto> constructor_options,
            *hash<auto> template_options) {
        return new CircleDataProvider(constructor_options);
    }

    #! Returns factory info
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return CircleDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    Class getClassImpl() {
        return new Class("CircleDataProvider");
    }
}

} # namespace CircleDataProvider
