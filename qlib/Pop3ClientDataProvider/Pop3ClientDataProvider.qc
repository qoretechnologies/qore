# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientDataProvider class definition

/** Pop3ClientDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the Pop3ClientDataProvider module
public namespace Pop3ClientDataProvider {
#! The POP3 client data provider class, provides API classes as children
public class Pop3ClientDataProvider inherits Pop3ClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "Pop3ClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            #"children_can_support_observers": True,
        };
    }

    private {
        const ChildMap = {
            "list": Class::forName("Pop3ClientDataProvider::Pop3ClientListDataProvider"),
            "capabilities": Class::forName("Pop3ClientDataProvider::Pop3ClientCapabilitiesDataProvider"),
            "uidl": Class::forName("Pop3ClientDataProvider::Pop3ClientUidlDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : Pop3ClientDataProviderBase(options) {
    }

    #! Creates the object from an POP3 connection
    constructor(Pop3Client pop3) : Pop3ClientDataProviderBase(pop3) {
    }

    #! Returns the data provider name
    string getName() {
        return "pop3";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for POP3 servers";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(pop3);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}