# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2A Agent Card data providers

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
#! A2A agent card container data provider
public class A2aAgentCardDataProvider inherits A2aClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "agent-card",
            "desc": "A2A agent card data provider",
            "type": "A2aAgentCardDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "agent-card";
    }

    private *list<string> getChildProviderNamesImpl() {
        return ("get", "get-extended");
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "get":
                return new A2aAgentCardGetDataProvider(a2a);
            case "get-extended":
                return new A2aAgentCardGetExtendedDataProvider(a2a);
        }
        return NOTHING;
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A get agent card data provider
public class A2aAgentCardGetDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Retrieves the A2A agent card",
            "type": "A2aAgentCardGetDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "get";
    }

    *string getDesc() {
        return "Retrieves the A2A agent card";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aAgentCardResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Getting agent card");
        return a2a.getAgentCard();
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A get authenticated extended agent card data provider
public class A2aAgentCardGetExtendedDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-extended",
            "desc": "Retrieves the authenticated extended A2A agent card",
            "type": "A2aAgentCardGetExtendedDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "get-extended";
    }

    *string getDesc() {
        return "Retrieves the authenticated extended A2A agent card";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aAgentCardResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Getting authenticated extended agent card");
        return a2a.getAuthenticatedExtendedCard();
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
