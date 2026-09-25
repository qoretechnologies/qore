# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordGuildDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DiscordDataProvider module
public namespace DiscordDataProvider {
#! The parent class for Discord guild REST APIs
public class DiscordGuildDataProvider inherits DiscordDataProviderBase {
    public {
        #! Provider info
        static hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "guild",
            "desc": "Discord guild data provider; parent provider for APIs related to guilds",
            "type": "DiscordGuildDataProvider",
            "constructor_options": DiscordDataProvider::ConstructorOptions + {
                "id": <DataProviderOptionInfo>{
                    "display_name": "Server ID",
                    "short_desc": "The ID of the server or guild",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The ID of the server or guild",
                },
            },
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The guild info
        hash<auto> guild;

        #! The guild's metadata
        *hash<auto> cal;

        const DefaultChildMap = {
            "get": Class::forName("DiscordDataProvider::DiscordGuildGetDataProvider"),
        };

        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", DiscordDataProvider::ConstructorOptions, options);
        rest = DiscordDataProvider::getAsyncRestConnection(options);

        guild = getGuild(options.id);
    }

    #! Creates the object from a REST connection
    constructor(DiscordRestClient::DiscordRestClientIo rest, hash<auto> guild) : DiscordDataProviderBase(rest) {
        self.guild = guild;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord %s data provider for `%s` (%y) for `%s`", ProviderInfo.name, guild.id, guild.name,
            rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Register a new child data provider for Discord guilds
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "DISCORD-GUILD-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }

        childMap{pathname} = cls;

        ImplicitModuleTransaction::add(sub () { DiscordGuildDataProvider::deregisterChild(pathname); });

        hash<DataProviderSummaryInfo> info = cls.getConstant("ProviderSummaryInfo").getValue();
        if ((info.children_can_support_observers || info.supports_observable)
            && !ProviderInfo.children_can_support_observers) {
            ProviderInfo.children_can_support_observers = True;
            ProviderSummaryInfo.children_can_support_observers = True;

            # update parents
            DiscordGuildsDataProvider::ProviderInfo.children_can_support_observers = True;
            DiscordGuildsByNameDataProvider::ProviderInfo.children_can_support_observers = True;

            # update grandparent
            DiscordDataProvider::ProviderInfo.children_can_support_observers = True;
        }

        if ((info.children_can_support_messages || info.supports_messages)
            && !ProviderInfo.children_can_support_messages) {
            ProviderInfo.children_can_support_messages = True;
            ProviderSummaryInfo.children_can_support_messages = True;

            # update parent
            DiscordGuildsDataProvider::ProviderInfo.children_can_support_messages = True;
            DiscordGuildsByNameDataProvider::ProviderInfo.children_can_support_observers = True;

            # update grandparent
            DiscordDataProvider::ProviderInfo.children_can_support_messages = True;
        }
    }

    #! Deregister the named child data provider
    private static deregisterChild(string pathname) {
        if (!DefaultChildMap{pathname}) {
            remove childMap{pathname};
        }
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DiscordDataProviderBase getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, guild);
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
