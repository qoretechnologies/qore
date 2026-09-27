# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectReflectionClient.qc Client-side reflection helper for Connect services

/*  ConnectReflectionClient.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace Connect {

#! Client-side helper for Connect service reflection
/** Thin wrapper around @ref ConnectClient that calls the built-in
    \c qore.connect.reflection service to discover services, methods,
    and type information at runtime.

    @par Example
    @code{.py}
ConnectClient client("https://example.com:8080");
ConnectReflectionClient refl(client);
list<string> services = refl.listServices();
hash<auto> svc_desc = refl.getServiceDescriptor(services[0]);
    @endcode
*/
public class ConnectReflectionClient {
    private {
        #! The Connect client used for reflection calls
        ConnectClient m_client;
    }

    #! Creates a reflection client
    /** @param client the Connect client to use for reflection calls
    */
    constructor(ConnectClient client) {
        m_client = client;
    }

    #! Lists all services available on the server (excluding the reflection service)
    /** @return a list of service names

        @throw CONNECT-ERROR if the reflection call fails
    */
    list<string> listServices() {
        auto response = m_client.call("qore.connect.reflection", "ListServices", {});
        list<string> result = ();
        if (response.services) {
            map result += $1.name, response.services;
        }
        return result;
    }

    #! Gets the full descriptor for a service
    /** @param service the service name

        @return a hash with service name, methods list (each with path, rpc_type, and
        optional request_type/response_type descriptors)

        @throw CONNECT-ERROR if the service is not found or the call fails
    */
    hash<auto> getServiceDescriptor(string service) {
        return m_client.call("qore.connect.reflection", "GetServiceDescriptor",
            {"service": service});
    }

    #! Gets the descriptor for a single method
    /** @param service the service name
        @param method the method name

        @return a hash with method name, path, rpc_type, and optional type descriptors

        @throw CONNECT-ERROR if the method is not found or the call fails
    */
    hash<auto> getMethodDescriptor(string service, string method) {
        return m_client.call("qore.connect.reflection", "GetMethodDescriptor",
            {"service": service, "method": method});
    }

    #! Discovers all services and their methods with type information
    /** @return a hash keyed by service name, each value containing the full service descriptor

        @throw CONNECT-ERROR if the reflection calls fail
    */
    hash<string, hash<auto>> discoverAll() {
        list<string> services = listServices();
        hash<string, hash<auto>> result();
        for (int i = 0; i < services.size(); ++i) {
            result{services[i]} = getServiceDescriptor(services[i]);
        }
        return result;
    }
}

} # namespace Connect
