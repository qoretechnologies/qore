# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectDataProviderFactory.qc Connect data provider factory

/*  ConnectDataProviderFactory.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConnectDataProvider {

#! Factory for creating Connect data providers
/** Registered as \c "connect" in the DataProvider framework.

    @par Example
    @code{.py}
# Create a provider via the global factory registry
auto provider = DataProvider::getFactoryObject("connect", {"url": "https://example.com:8080"});

# Or create the factory directly
ConnectDataProviderFactory factory();
auto provider2 = factory.create({"url": "https://example.com:8080"});
    @endcode
*/
public class ConnectDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory metadata
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "connect",
            "desc": "Data provider factory for Connect RPC protocol",
            "children_can_support_observers": True,
        };
    }

    #! Returns factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the provider class
    Class getClassImpl() {
        return Class::forName("ConnectDataProvider::ConnectDataProvider");
    }

    #! Creates a provider instance
    private DataProvider::AbstractDataProvider createImpl(hash<auto> options,
            *LoggerInterface logger) {
        return new ConnectDataProvider(options);
    }

    #! Returns the description
    string getDescImpl() {
        return FactoryInfo.desc;
    }

    #! Returns provider info
    private hash<DataProvider::DataProviderInfo> getProviderInfoImpl() {
        return ConnectDataProvider::ProviderInfo;
    }
}

} # namespace ConnectDataProvider
