# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordGuildsByNameDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The parent class for REST APIs related to specific Discord guilds
public class DiscordGuildsByNameDataProvider inherits DiscordDataProviderBase {
    public {
        #! Provider info
        static hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "guilds-by-name",
            "desc": "Discord guilds data provider; parent provider for REST APIs related to specific Discord "
                "guilds indexed by name",
            "type": "DiscordGuildsByNameDataProvider",
            "constructor_options": DiscordDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_identical": True,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Guild list
        hash<string, hash<auto>> guilds;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", DiscordDataProvider::ConstructorOptions, options);
        rest = DiscordDataProvider::getAsyncRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(DiscordRestClient::DiscordRestClientIo rest) : DiscordDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("Discord guild %y", $1),
            "type": "DiscordGuildsByNameDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllGuilds();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DiscordDataProviderBase getChildProviderImpl(string name) {
        if (*hash<auto> guild = getAllGuilds(){name}) {
            return new DiscordGuildDataProvider(rest, guild);
        }
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns access info for all tables
    private hash<string, hash<auto>> getAllGuilds() {
        if (exists guilds) {
            return guilds;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (exists guilds) {
            return guilds;
        }

        # the exception argument of an error response includes the response body
        guilds = cast<*hash<string, hash<auto>>>(map {$1.name: $1}, doRestCommand("GET", "users/@me/guilds").body)
            ?? {};

        return guilds;
    }
}
}
