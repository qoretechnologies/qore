# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpClientDataProviderFactory class definition

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

#! Contains all public definitions in the McpClientDataProvider module
public namespace McpClientDataProvider {
#! Factory class for MCP client data providers
public class McpClientDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "mcpclient",
            "desc": "Data provider factory for MCP (Model Context Protocol) server connections",
            "children_can_support_apis": True,
        };
    }

    #! Returns the factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the class for the data provider
    Class getClassImpl() {
        return Class::forName("McpClientDataProvider::McpClientDataProvider");
    }

    #! Returns the data provider from the given options
    /** @param options the options for the data provider; one of:
        - \c mcp: an McpClient object
        - \c mcpconnection: the name of an MCP connection
        - \c url: the URL to the MCP server
        @param logger an optional logger

        @return the data provider created from the given options
    */
    private DataProvider::AbstractDataProvider createImpl(hash<auto> options, *LoggerInterface logger) {
        @debug("Creating MCP client data provider with options: %y", options);
        return new McpClientDataProvider(options);
    }

    #! Returns static provider information as data; no connection is required
    *hash<auto> getInfoAsDataImpl(hash<auto> constructor_options, *bool with_type_info) {
        return McpClientDataProvider::ProviderInfo;
    }

    #! Returns the description of the factory
    string getDescImpl() {
        return FactoryInfo.desc;
    }

    #! Returns provider info
    private hash<DataProvider::DataProviderInfo> getProviderInfoImpl() {
        return McpClientDataProvider::ProviderInfo;
    }
}
}
