# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2aClientDataProvider class definition

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {
#! Root A2A client data provider
/** Provides access to A2A agent operations through the DataProvider API.

    @par Example:
    @code{.py}
    A2aClientDataProvider provider({"url": "a2a://localhost:8080"});
    # Get agent card
    auto card = provider.getChildProvider("agent-card").getChildProvider("get").doRequest({});
    # Send a message
    auto task = provider.getChildProvider("messages").getChildProvider("send").doRequest({
        "role": "user",
        "parts": ({"type": "text", "text": "Hello!"},),
    });
    @endcode
*/
public class A2aClientDataProvider inherits A2aClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "A2aClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Child providers
        const ChildMap = {
            "agent-card": Class::forName("A2aClientDataProvider::A2aAgentCardDataProvider"),
            "messages": Class::forName("A2aClientDataProvider::A2aMessagesDataProvider"),
            "tasks": Class::forName("A2aClientDataProvider::A2aTasksDataProvider"),
            "push-notifications": Class::forName("A2aClientDataProvider::A2aPushNotificationsDataProvider"),
            "events": Class::forName("A2aClientDataProvider::A2aEventDataProvider"),
        };
    }

    #! Creates the object with the given A2A client
    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : A2aClientDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return "a2a";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("A2A data provider for %s", a2a.getUrl());
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return NOTHING;
        }
        @debug("Creating child provider %y", name);
        return cls.newObject(a2a);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
