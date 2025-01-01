# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/*  EmpathicBuildingRestConnection.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
public class EmpathicBuildingRestClient inherits RestClient::RestClient {
    public {
        #! Login URI path
        const LoginUriPath = "v1/login";

        #! Token URI path
        const TokenRefreshUriPath = "v1/token";

        #! Default ping method
        const DefaultPingMethod= "GET";

        #! Default ping path
        const DefaultPingPath = "v1/current_time";

        #! Default options
        const DefaultOptions = {
            "ping_method": DefaultPingMethod,
            "ping_path": DefaultPingPath,
        };
    }

    private {
        #! Is refresh broken?
        /** 2024-01-23: all "access_token" values returned by the Haltian server on this date are invalid; only the
            undocumented "token" response to the login API is working
        */
        bool broken_refresh = True;
    }

    constructor(*hash<auto> opts, *softbool do_not_connect) : RestClient(DefaultOptions + opts, do_not_connect) {
    }

    #! Sets up authentication info
    private setupAuth(hash<auto> opts, *bool do_not_connect) {
        # set any token
        if (opts.token) {
            setToken(opts.token_type ?? "Bearer", opts.token, opts.refresh_token);
        } else if (opts.refresh_token) {
            refresh_token = opts.refresh_token;
        }

        if (!exists getUsername() && !exists getPassword() && exists opts.username && exists opts.password) {
            username = opts.username;
            password = opts.password;
        }
        if (*string u = getUsername()) {
            username = u;
        }
        if (*string p = getPassword()) {
            password = p;
        }

        # do not send a basic auth header in any request
        clearUserPassword();
    }

    #! Gets a new token using the refresh token
    /** @return the response from the refresh token call

        @since RestClient 2.2
    */
    private hash<auto> refreshTokenIntern(*reference<hash<auto>> info) {
        # restore the old serialization option on exit
        string old_ds = ds;
        on_exit setSerialization(old_ds);
        # use URL form encoding for the message body
        setSerialization("url");

        # do a normal login if there is no refresh token, or if refresh operations are marked as broken
        if ((broken_refresh || !refresh_token) && (username.val() && password.val())) {
            return loginIntern(\info);
        }

        # get a new token with the refresh token
        try {
            return gotOAuth2LoginInfo(doHaltianTokenRefreshRequest(\info));
        } catch (hash<ExceptionInfo> ex) {
            if (refreshTokenInvalid(ex, info) && username.val() && password.val()) {
                debug("");
                return loginIntern(\info);
            }
            rethrow;
        }
    }

    private bool refreshTokenInvalid(hash<ExceptionInfo> ex, *hash<auto> info) {
        return info."response-code" == 401;
    }

    #! Checks if a login is necessary; if so, then the login is performed
    private checkLogin(*reference<hash<auto>> info) {
        if (!token.val()) {
            if (refresh_token) {
                refreshTokenIntern(\info);
            } else if (username.val() && password.val()) {
                loginIntern(\info);
            }
        }
    }

    #! Called when OAuth2 login information has been received
    hash<auto> gotOAuth2LoginInfo(hash<auto> h) {
        # prefer "token" to "access_token"
        if (h.token && h.token != h.access_token) {
            h.access_token = h.token;
        }
        return RestClient::gotOAuth2LoginInfo(h);
    }

    #! Returns the deserialized response body of a Haltian token refresh request
    private hash<auto> doHaltianTokenRefreshRequest(*reference<hash<auto>> info) {
        hash<auto> login = {
            "refresh_token": refresh_token,
        };

        string url = TokenRefreshUriPath;
        on_success debug("Haltian: POST to %y successful", url);
        on_error debug("Haltian: POST to %y failed: %s: %s request: %y headers: %y body: %y response: %y "
            "headers: %y body: %y", url, $1.err, $1.desc, info."request-uri", info.headers, info."request-body",
            info."response-uri", info."response-headers-raw", info."response-body");
        return RestClient::doRequestIntern("POST", url, login, \info).body;
    }

    #! Returns the deserialized response body of a Haltian authorization / token request
    private hash<auto> doHaltianLoginRequest(*reference<hash<auto>> info) {
        hash<auto> login = {
            "email": username,
            "password": password,
        };

        string url = LoginUriPath;
        on_success debug("Haltian: POST to %y successful", url);
        on_error debug("Haltian: POST to %y failed: %s: %s request: %y headers: %y body: %y response: %y "
            "headers: %y body: %y", url, $1.err, $1.desc, info."request-uri", info.headers, info."request-body",
            info."response-uri", info."response-headers-raw", info."response-body");
        return RestClient::doRequestIntern("POST", url, login, \info).body;
    }

    #! Returns True if the request error indicates that a refresh operation should be performed
    private bool needsRefresh(hash<ExceptionInfo> ex, *hash<auto> info, bool decode_errors) {
        return refresh_token.val() || (username.val() && password.val());
    }

    #! Returns options to update after a login operation
    /** @since %RestClient 2.0
    */
    private *hash<auto> getUpdateOptionsAfterLogin(hash<auto> h) {
        hash<auto> new_opts;
        if (h.token_type && h.token_type != token_type) {
            new_opts.token_type = h.token_type;
        }
        if (h.access_token && h.access_token != token) {
            new_opts.token = h.access_token;
        }
        if (h.refresh_token && h.refresh_token != refresh_token) {
            new_opts.refresh_token = h.refresh_token;
        }
        return new_opts;
    }

    #! Authenticates with a user and password if configured
    private hash<auto> loginIntern(*reference<hash<auto>> info) {
        # restore the old serialization option on exit
        string old_ds = ds;
        on_exit setSerialization(old_ds);
        # use URL form encoding for the message body
        setSerialization("url");

        hash<auto> info;
        return gotOAuth2LoginInfo(doHaltianLoginRequest(\info));
    }
}

#! EmpathicBuilding REST connection
public class EmpathicBuildingRestConnection inherits RestClient::RestConnection {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Haltian Empathic Building REST Connection",
            "short_desc": "A connection to a Haltian Empathic Building REST server",
            "desc": "A connection to a Haltian Empathic Building REST server",
            "cls": Class::forName("EmpathicBuildingRestConnection"),
            "schemes": {
                "ebrests": True,
            },
            "base_scheme_map": {
                "https": "ebrests",
            },
            "options": (map {$1.key: $1.value}, RestConnection::ConnectionScheme.options.pairIterator(),
                $1.key !~ /^oauth2_/) + {
                "ping_method": RestConnection::ConnectionScheme.options.ping_method + <ConnectionOptionInfo>{
                    "default_value": EmpathicBuildingRestClient::DefaultPingMethod,
                },
                "ping_path": RestConnection::ConnectionScheme.options.ping_path + <ConnectionOptionInfo>{
                    "default_value": EmpathicBuildingRestClient::DefaultPingPath,
                },
                "refresh_token": <ConnectionOptionInfo>{
                    "display_name": "Refresh Token",
                    "short_desc": "Sets the Haltian refresh token",
                    "type": "string",
                    "desc": "Sets the Haltian refresh token, if any (complements option `token`)",
                    "sensitive": True,
                },
            },
        };
    }

    #! creates the EmpathicBuildingRestConnection object
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored? Default: True
        - enabled (optional bool): is the connection enabled? Default: True
        - locked (optional bool): is the connection locked? Default: False
        - debug_data (optional bool): debug data? Default: False
        - tags (optional hash): tags for the connection; no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : RestConnection(config, attr) {
    }

    #! returns \c "ebrests"
    string getType() {
        return "ebrests";
    }

    #! returns a data provider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return a data provider object for this connection; the data provider is @ref EmpathicBuildingDataProvider
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new EmpathicBuildingDataProvider({"restclient": get()});
    }

    #! Sets features during REST client initialization
    private setFeatures() {
        features{CF_UPDATE_OPTIONS} = True;
    }

    #! returns a @ref RestClient::RestClient "RestClient" object
    /** @param connect if @ref True "True", then the connection is returned already connected
        @param rtopts this connection type does not accept any runtime options, so this parameter is ignored

        @return a @ref RestClient::RestClient "RestClient" object
    */
    private RestClient getImpl(bool connect = True, *hash<auto> rtopts) {
        return new EmpathicBuildingRestClient(getConnectionOptions(), !connect);
    }

    #! Returns the connection scheme for this class
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }

    #! Processes login responses and returns updated options
    private *hash<auto> processOAuth2TokenResponseImpl(hash<auto> resp) {
        hash<auto> new_opts;
        if (resp.access_token != real_opts.token) {
            new_opts.token = resp.access_token;
        }
        if (resp.refresh_token && resp.refresh_token != real_opts.refresh_token) {
            new_opts.refresh_token = resp.refresh_token;
        }
        return new_opts;
    }
}
}
