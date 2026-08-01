# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpClientDataProvider class definition

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
#! Root MCP client data provider
/** Provides access to MCP server tools, resources, and prompts through the DataProvider API.

    @par Example:
    @code{.py}
    McpClientDataProvider provider({"url": "mcp://localhost:8080"});
    # List tools
    auto tools = provider.getChildProvider("tools").getChildProvider("list").doRequest({});
    # Call a tool
    auto result = provider.getChildProvider("tools").getChildProvider("call").doRequest({
        "name": "get_weather",
        "arguments": {"city": "Prague"},
    });
    @endcode
*/
public class McpClientDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "McpClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Child providers available with every protocol version
        const ChildMap = {
            "tools": Class::forName("McpClientDataProvider::McpToolsDataProvider"),
            "resources": Class::forName("McpClientDataProvider::McpResourcesDataProvider"),
            "prompts": Class::forName("McpClientDataProvider::McpPromptsDataProvider"),
            "completion": Class::forName("McpClientDataProvider::McpCompletionDataProvider"),
            "events": Class::forName("McpClientDataProvider::McpNotificationEventDataProvider"),
        };

        #! Child providers for methods removed in 2026-07-28
        /** \c ping was removed when MCP became stateless; it is only advertised when the
            server speaks a handshake-based protocol version, so that a provider tree built
            against a modern server does not offer an operation that server cannot perform.
        */
        const LegacyChildMap = {
            "ping": Class::forName("McpClientDataProvider::McpPingDataProvider"),
        };

        #! Child providers introduced in 2026-07-28
        const ModernChildMap = {
            "discover": Class::forName("McpClientDataProvider::McpDiscoverDataProvider"),
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : McpClientDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return "mcp";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("MCP data provider for %s", mcp.getUrl());
    }

    #! Returns a list of child data provider names
    /** The set depends on the protocol version in use: operations that a given protocol
        revision does not define are not advertised.
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getChildMap();
    }

    #! Returns the child providers available for the protocol version in use
    private hash<auto> getChildMap() {
        return ChildMap + (mcp.isModern() ? ModernChildMap : LegacyChildMap);
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = getChildMap(){name};
        if (!cls) {
            return NOTHING;
        }
        @debug("Creating child provider %y", name);
        return cls.newObject(mcp);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
