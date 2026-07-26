# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JsonFileReadDataProviderFactory class definition

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JsonFileDataProvider module
public namespace JsonFileDataProvider {
#! Factory class for JSON file read data providers
public class JsonFileReadDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("JsonFileReadDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "jsonread",
            "desc": "JSON file reader data provider factory for reading standard JSON files",
            "children_can_support_records": True,
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
        return JsonFileReadDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid constructor options, API not supported

        @note
        - If constructor options provide actual input data, then this overrides the example data
    */
    private AbstractDataProvider getProviderFromExampleImpl(InputStream example, *hash<auto> constructor_options) {
        if (constructor_options.path || constructor_options.data) {
            return new JsonFileReadDataProvider(constructor_options);
        }
        return new JsonFileReadDataProvider({"stream": example} + constructor_options);
    }

    #! Returns information about a data provider constructor given the example data as an argument
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return example record data parsed from the given options

        @throw DATA-PROVIDER-ERROR invalid constructor options, API not supported
    */
    private list<hash<auto>> getExampleProviderRecordOutputImpl(InputStream example,
            *hash<auto> constructor_options) {
        JsonFileReadDataProvider prov({"stream": example} + constructor_options);
        return map $1, prov.searchRecords();
    }
}
}
