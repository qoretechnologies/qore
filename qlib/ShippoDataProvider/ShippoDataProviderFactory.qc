# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoDataProviderFactory.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! The Shippo data provider factory
public class ShippoDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("ShippoDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "shippo",
            "desc": "Shippo data provider factory for shipping, labels, and order management",
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };
    }

    #! Returns static factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the class for the data provider
    Class getClassImpl() {
        return cls;
    }

    #! Returns static provider info
    /** @note the \a name and \a children attributes are not returned as they are dynamic attributes
    */
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return ShippoDataProvider::ProviderInfo;
    }
}

} # namespace ShippoDataProvider
