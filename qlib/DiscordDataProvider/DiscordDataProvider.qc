# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Discord application name
public const AppName = "Discord";

#! The Discord data provider class
public class DiscordDataProvider inherits DiscordDataProviderBase {
    public {
        #! Provider info
        static hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "type": "DiscordDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The DiscordRestClient to use for the connection",
                "type": AbstractDataProviderType::get(new Type("DiscordRestClient")),
                "desc": "The `DiscordRestClient` object to use for the connection",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the DiscordRestClient constructor",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the DiscordRestClient constructor; only used if a DiscordRestClient object is "
                    "created for a call",
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client ID",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client secret",
            },
            "oauth2_refresh_token": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Refresh Token",
                "short_desc": "Sets the OAuth2 refresh token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 refresh token, if known",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 token, if known",
            },
        };
    }

    private {
        static hash<string, code> dynamic_children;

        const ChildMap = {
            "guilds": Class::forName("DiscordDataProvider::DiscordGuildsDataProvider"),
            "guilds-by-name": Class::forName("DiscordDataProvider::DiscordGuildsByNameDataProvider"),
            "user-info": Class::forName("DiscordDataProvider::DiscordUserInfoGetDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = DiscordDataProvider::getRestConnection(copts);
    }

    #! Creates the object from a DiscordRestClient
    constructor(DiscordRestClient rest) : DiscordDataProviderBase(rest) {
    }

    #! Creates the object from a DiscordRestClient
    constructor(DiscordRestConnection conn) : DiscordDataProviderBase(conn) {
    }

    #! Registers a child data provider
    /** @param name the child's name
        @param generator code to generate the child; prototype: sub (string name, AbstractConnection conn) {}
    */
    static registerChild(string name, code generator) {
        dynamic_children{name} = generator;
    }

    #! Returns a DiscordRestClient connection from the options
    static DiscordRestClient::DiscordRestClient getRestConnection(*hash<auto> options) {
        DiscordRestClient rest;
        if (options.restclient) {
            rest = options.restclient;
        } else {
            if (!options.oauth2_client_id) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient' or 'oauth2_client_id' option passed; "
                    "cannot create REST client to Discord instance without authentication information";
            }
            if (!options.oauth2_client_secret) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient', 'oauth2_client_secret' option passed; "
                    "cannot create REST client to Discord instance without authentication information";
            }
            if (!options.token) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient', 'token' option passed; "
                    "cannot create REST client to Discord instance without authentication information";
            }

            hash<auto> opts;
            opts += options.restclient_options + options{
                "oauth2_client_id",
                "oauth2_client_secret",
                "token",
                "oauth2_refresh_token",
            };
            rest = new DiscordRestClient(opts);
        }
        rest.setConnectionPath("/");
        return rest;
    }

    #! Returns the data provider name
    string getName() {
        return "discord";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord REST API data provider for `%s`", rest.getSafeURL());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<hash<DataProvider::DataProviderSummaryInfo>> rv;
        foreach Class cls in (ChildMap.iterator()) {
            try {
                rv += cls.getConstant("ProviderSummaryInfo").getValue();
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "UNKNOWN-CONSTANT") {
                    rethrow;
                }
            }
            rv += cls.getStaticMember("ProviderSummaryInfo").getValue();
        }
        if (conn) {
            map rv += $1.value($1.key, conn).getSummaryInfo(), dynamic_children.pairIterator();
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        list<string> rv = keys ChildMap;
        if (dynamic_children && conn) {
            rv += keys dynamic_children;
        }
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls && conn) {
            if (*code gen = dynamic_children{name}) {
                return gen(name, conn);
            }
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
