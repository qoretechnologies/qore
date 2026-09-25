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
                "short_desc": "The DiscordRestClientIo to use for the connection",
                "type": AbstractDataProviderType::get(new Type("RestClient::AbstractRestClient")),
                "desc": "The `DiscordRestClientIo` object to use for the connection; a synchronous "
                    "`DiscordRestClient` is also accepted, and a `DiscordRestClientIo` is created with its options",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the DiscordRestClientIo constructor",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the `DiscordRestClientIo` constructor; only used if a `DiscordRestClientIo` "
                    "object is created for a call",
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
                "sensitive": True,
            },
            "oauth2_refresh_token": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Refresh Token",
                "short_desc": "Sets the OAuth2 refresh token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 refresh token, if known",
                "sensitive": True,
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 token, if known",
                "sensitive": True,
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
        rest = DiscordDataProvider::getAsyncRestConnection(copts);
    }

    #! Creates the object from a DiscordRestClientIo
    /** @since DiscordDataProvider 2.1
    */
    constructor(DiscordRestClientIo rest) : DiscordDataProviderBase(rest) {
    }

    #! Creates the object from a DiscordRestClient
    /** API calls are made with a @ref DiscordRestClient::DiscordRestClientIo "DiscordRestClientIo" created with the
        options and the connection path of \a rest
    */
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

    #! Returns a DiscordRestClientIo for API calls from the options
    /** @param options the constructor options; if \c restclient is a synchronous
        @ref DiscordRestClient::DiscordRestClient "DiscordRestClient", a
        @ref DiscordRestClient::DiscordRestClientIo "DiscordRestClientIo" is created with its options and connection
        path

        @return the REST client for API calls

        @throw CONSTRUCTOR-ERROR the \c restclient option has an unsupported type, or no authentication information
        is available

        @since DiscordDataProvider 2.1
    */
    static DiscordRestClient::DiscordRestClientIo getAsyncRestConnection(*hash<auto> options) {
        if (options.restclient) {
            if (options.restclient instanceof DiscordRestClientIo) {
                return options.restclient;
            }
            if (options.restclient instanceof DiscordRestClient) {
                return DiscordDataProviderBase::getAsyncClient(options.restclient);
            }
            throw "CONSTRUCTOR-ERROR", sprintf("the 'restclient' option must be a DiscordRestClientIo or a "
                "DiscordRestClient object; got class %y", options.restclient.className());
        }
        return new DiscordRestClientIo(DiscordDataProvider::getRestClientOptions(options));
    }

    #! Returns a DiscordRestClient connection from the options
    /** @deprecated use @ref getAsyncRestConnection(); the data provider makes API calls with a
        @ref DiscordRestClient::DiscordRestClientIo "DiscordRestClientIo"
    */
    static DiscordRestClient::DiscordRestClient getRestConnection(*hash<auto> options) {
        DiscordRestClient rest;
        if (options.restclient) {
            if (!(options.restclient instanceof DiscordRestClient)) {
                throw "CONSTRUCTOR-ERROR", sprintf("the 'restclient' option must be a DiscordRestClient object; "
                    "got class %y", options.restclient.className());
            }
            rest = options.restclient;
        } else {
            rest = new DiscordRestClient(DiscordDataProvider::getRestClientOptions(options));
        }
        rest.setConnectionPath("/");
        return rest;
    }

    #! Returns REST client options from the constructor options
    /** @throw CONSTRUCTOR-ERROR no authentication information is available
    */
    private static hash<auto> getRestClientOptions(*hash<auto> options) {
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
        return opts;
    }

    #! Returns the data provider name
    string getName() {
        return "discord";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord REST API data provider for `%s`", rest.getSafeUrl());
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
