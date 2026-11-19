# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekDataProvider root class definition

/** DeepSeekDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DeepSeekDataProvider module
public namespace DeepSeekDataProvider {
#! The DeepSeek data provider root class
public class DeepSeekDataProvider inherits DeepSeekDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "deepseek",
            "type": "DeepSeekDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Default child data providers
        const DefaultChildMap = {
            "chat-completion":
                Class::forName("DeepSeekDataProvider::DeepSeekChatCompletionDataProvider"),
        };
    }

    private:internal {
        #! Serializes registration and snapshots of extension providers
        static RWLock childLock();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : DeepSeekDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DeepSeekDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        AutoWriteLock al(childLock);
        if (childMap{pathname} || DeepSeekActions::Operations{pathname}) {
            throw "DEEPSEEK-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Returns data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        AutoReadLock al(childLock);
        list<hash<DataProviderSummaryInfo>> rv = map
            $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
        foreach string name in (keys DeepSeekActions::Operations) {
            push rv, <DataProviderSummaryInfo>{"name": name, "type": "DeepSeekApiDataProvider",
                "supports_request": True};
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        AutoReadLock al(childLock);
        return (keys childMap) + (keys DeepSeekActions::Operations);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (DeepSeekActions::Operations{name}) {
            return new DeepSeekApiDataProvider(rest, name);
        }
        *Class cls;
        {
            AutoReadLock al(childLock);
            cls = childMap{name};
        }
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
