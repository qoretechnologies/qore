# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceProductImagesDataProvider.qc Squarespace product images container data provider

/*  SquarespaceProductImagesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace product images container data provider
public class SquarespaceProductImagesDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "images",
            "desc": "Squarespace product images data provider for managing product images",
            "type": "SquarespaceProductImagesDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "upload": Class::forName("SquarespaceUploadProductImageDataProvider"),
            "assign": Class::forName("SquarespaceAssignProductImageDataProvider"),
            "update": Class::forName("SquarespaceUpdateProductImageDataProvider"),
            "delete": Class::forName("SquarespaceDeleteProductImageDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "images";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace product images data provider for managing product images";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
