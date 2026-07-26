# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FhirRestDataProviderFactory class definition

/*  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FhirRestDataProvider module
public namespace FhirRestDataProvider {
#! Factory class for FHIR REST data providers
public class FhirRestDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": FactoryName,
            "desc": "Data provider factory for FHIR REST server connections",
            "children_can_support_apis": True,
        };
    }

    #! Returns the factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the class for the data provider
    Class getClassImpl() {
        return Class::forName("FhirRestDataProvider::FhirRestDataProvider");
    }

    #! Returns the data provider from the given options
    /** @param options provider constructor options
        @param logger optional logger
        @return the data provider created from the given options
    */
    private DataProvider::AbstractDataProvider createImpl(hash<auto> options, *LoggerInterface logger) {
        return new FhirRestDataProvider(options);
    }

    #! Returns static provider information as data; no connection is required
    *hash<auto> getInfoAsDataImpl(hash<auto> constructor_options, *bool with_type_info) {
        return FhirRestDataProvider::ProviderInfo;
    }

    #! Returns the description of the factory
    string getDescImpl() {
        return FactoryInfo.desc;
    }

    #! Returns provider info
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return FhirRestDataProvider::ProviderInfo;
    }
}
}
