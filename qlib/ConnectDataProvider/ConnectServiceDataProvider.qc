# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectServiceDataProvider.qc Connect service-level data provider

/*  ConnectServiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Connect service-level data provider
/** Represents a single Connect service.  Child providers are method names
    within this service.

    @par Example
    @code{.py}
auto provider = DataProvider::getFactoryObject("connect", {"url": "https://example.com:8080"});
auto svc = provider.getChildProvider("Greeter");

# List available methods
*list<string> methods = svc.getChildProviderNames();
printf("Methods: %y\n", methods);

# Access a method and make a call
auto result = svc.getChildProvider("SayHello").doRequest({"name": "World"});
    @endcode
*/
public class ConnectServiceDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "connect-service",
            "desc": "Connect RPC service data provider; children are method names",
            "type": "ConnectServiceDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": True,
        };
    }

    private {
        #! Service name
        string m_service;
        #! Provider options
        hash<ConnectDataProviderOptions> m_opts;
        #! Method map from discovery (method name -> method descriptor)
        *hash<string, hash<auto>> m_method_map;
    }

    #! Creates the service provider
    /** @param service the service name
        @param opts provider options (inherited from root)
    */
    constructor(string service, hash<ConnectDataProviderOptions> opts) {
        m_service = service;
        m_opts = opts;
    }

    #! Creates the service provider with discovery data
    /** @param service the service name
        @param opts provider options
        @param service_desc service descriptor from reflection (contains \c methods list)
    */
    constructor(string service, hash<ConnectDataProviderOptions> opts,
            hash<auto> service_desc) {
        m_service = service;
        m_opts = opts;
        if (service_desc.methods) {
            m_method_map = {};
            map (m_method_map{$1.method} = $1), service_desc.methods;
        }
    }

    #! Returns the service name
    string getName() {
        return m_service;
    }

    #! Returns provider description
    *string getDesc() {
        return sprintf("Connect service '%s' on %s", m_service, m_opts.url);
    }

    #! Returns a child provider for the given method name
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (m_method_map{name}) {
            return new ConnectMethodDataProvider(m_service, name, m_opts, m_method_map{name});
        }
        return new ConnectMethodDataProvider(m_service, name, m_opts);
    }

    #! Returns known child provider names
    /** Returns discovered method names if available, otherwise empty.
    */
    private *list<string> getChildProviderNamesImpl() {
        if (m_method_map) {
            return keys m_method_map;
        }
        return ();
    }

    #! Returns provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConnectDataProvider
