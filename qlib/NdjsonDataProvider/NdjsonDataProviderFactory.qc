# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore NdjsonDataProviderFactory class definition

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

#! Contains all public definitions in the NdjsonDataProvider module
public namespace NdjsonDataProvider {
#! Factory class for NDJSON data providers
public class NdjsonDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "ndjson",
            "desc": "Data provider factory for reading NDJSON (Newline Delimited JSON) data",
        };
    }

    #! Returns the factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the class for the data provider
    Class getClassImpl() {
        return Class::forName("NdjsonDataProvider::NdjsonReadDataProvider");
    }

    #! Returns the data provider from the given options
    /** @param options the options for the data provider; one of:
        - \c path: path to an NDJSON file
        - \c stream: an input stream
        - \c data: NDJSON data as a string
        @param logger an optional logger

        @return the data provider created from the given options
    */
    private DataProvider::AbstractDataProvider createImpl(hash<auto> options, *LoggerInterface logger) {
        return new NdjsonReadDataProvider(options);
    }

    #! Returns static provider information as data; no connection is required
    *hash<auto> getInfoAsDataImpl(hash<auto> constructor_options, *bool with_type_info) {
        return NdjsonReadDataProvider::ProviderInfo;
    }

    #! Returns the description of the factory
    string getDescImpl() {
        return FactoryInfo.desc;
    }

    #! Returns provider info
    private hash<DataProvider::DataProviderInfo> getProviderInfoImpl() {
        return NdjsonReadDataProvider::ProviderInfo;
    }
}
}
