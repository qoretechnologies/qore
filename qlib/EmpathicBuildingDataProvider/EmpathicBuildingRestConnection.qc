# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/*  EmpathicBuildingRestConnection.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
            setToken(opts.token_type ?? "Bearer", opts.token, opts.refresh_token, opts.oauth2_token_expiry);
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
            return gotOAuth2LoginInfo(doHaltianTokenRefreshRequest(\info), True);
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
    hash<auto> gotOAuth2LoginInfo(hash<auto> h, *bool refresh) {
        # prefer "token" to "access_token"
        if (h.token && h.token != h.access_token) {
            h.access_token = h.token;
        }
        return RestClient::gotOAuth2LoginInfo(h, refresh);
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
    private bool needsRefresh(*hash<ExceptionInfo> ex, *hash<auto> info, bool decode_errors, auto body) {
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

#! The asynchronous REST client for the Haltian Empathic Building API
/** This class accepts the options of <a href="../../RestClientIo/html/index.html">RestClientIo</a> and authenticates
    like @ref EmpathicBuildingRestClient "EmpathicBuildingRestClient":
    - \c token: a bearer token to send in requests
    - \c username and \c password: credentials for the Haltian login API (\c v1/login); they are not sent with
      HTTP Basic authentication
    - \c refresh_token: a Haltian refresh token (\c v1/token)

    If no token is set, the client logs in when it is created (unless the \c skip_login option is set) or before its
    first request.  When a request is rejected with HTTP status code 401, the client acquires a new token and sends
    the request again once.  Because the tokens returned by the Haltian token refresh API are not accepted by the
    server, a new token is acquired with the login API whenever a username and password are set, otherwise with the
    refresh token.

    Token acquisition is serialized; a thread that waits while another thread acquires a new token uses that token
    instead of logging in again.

    @since EmpathicBuildingDataProvider 1.2
*/
public class EmpathicBuildingRestClientIo inherits RestClientIo::RestClientIo {
    private {
        #! The username for the login API
        *string login_username;

        #! The password for the login API
        *string login_password;

        #! Is refresh broken?
        /** see @ref EmpathicBuildingRestClient "EmpathicBuildingRestClient"; only the \c "token" response to the login API is accepted by the
            server
        */
        bool broken_refresh = True;

        #! Serializes token acquisition
        Mutex login_lock();
    }

    #! Creates the object with the given options
    /** @param opts the options of <a href="../../RestClientIo/html/index.html">RestClientIo</a> plus
        \c refresh_token; see the class description for the authentication options
        @param shared_mgr an optional shared connection manager

        @throw RESTCLIENTIO-ERROR the \c url option is missing
        @throw REST-RESPONSE-ERROR the login request returned an error response; the exception argument is the
        response
        @throw EMPATHICBUILDING-LOGIN-ERROR the login response has no token
    */
    constructor(hash<auto> opts, *HttpClientIo::HttpClientConnectionManager shared_mgr)
            : RestClientIo(EmpathicBuildingRestClientIo::getOptions(opts), shared_mgr) {
        hash<auto> creds = EmpathicBuildingRestClientIo::getCredentials(opts);
        # the caller's options with the credentials, also any given in the URL, so that copyWithUrl() creates a
        # client with the same credentials
        saved_opts = {};
        saved_opts += opts;
        foreach hash<auto> i in (creds.pairIterator()) {
            if (i.value.val()) {
                saved_opts{i.key} = i.value;
            }
        }
        login_username = creds.username;
        login_password = creds.password;
        if (creds.refresh_token) {
            AutoWriteLock al(config_lock);
            refresh_token = creds.refresh_token;
        }
        if (!opts.skip_login) {
            checkLogin();
        }
    }

    #! Returns the options for the @ref RestClientIo::RestClientIo "RestClientIo" constructor
    /** The credentials are removed, including any in the URL, so that they are not sent with HTTP Basic
        authentication, and the login is deferred to the constructor of this class

        @param opts the options passed to the constructor

        @return the options for the @ref RestClientIo::RestClientIo "RestClientIo" constructor
    */
    static hash<auto> getOptions(hash<auto> opts) {
        # the defaults have string values, so the hash must not take their narrow type
        hash<auto> rv = {};
        rv += EmpathicBuildingRestClient::DefaultOptions;
        rv += opts;
        remove rv{"username", "password", "refresh_token", "oauth2_refresh_token"};
        if (rv.url.typeCode() == NT_STRING) {
            rv.url =~ s/^([a-z][a-z0-9+.-]*:\/\/)[^@\/]*@/$1/i;
        }
        rv.skip_login = True;
        return rv;
    }

    #! Returns the credentials from the options
    /** @param opts the options passed to the constructor

        @return a hash with \c username, \c password, and \c refresh_token keys; the username and password are
        taken from the URL, percent-decoded, if they are not set as options
    */
    static hash<auto> getCredentials(hash<auto> opts) {
        hash<auto> rv = {};
        rv.username = opts.username;
        rv.password = opts.password;
        rv.refresh_token = opts.refresh_token ?? opts.oauth2_refresh_token;
        if ((!rv.username.val() || !rv.password.val()) && opts.url.typeCode() == NT_STRING) {
            hash<UrlInfo> url_info = parse_url(opts.url);
            if (url_info.username.val() && url_info.password.val()) {
                # the user information in a URL is percent-encoded
                rv.username = decode_url(url_info.username);
                rv.password = decode_url(url_info.password);
            }
        }
        return rv;
    }

    #! Logs in if there is no token and credentials are set
    private checkLogin() {
        if (getToken() || !canLogin()) {
            return;
        }
        AutoLock al(login_lock);
        # another thread may have logged in while this one waited
        if (getToken()) {
            return;
        }
        acquireTokenLocked();
    }

    #! Returns @ref True if a new token can be acquired after an HTTP 401 response
    private bool canRetryAuth() {
        return canLogin();
    }

    #! Acquires a new token
    /** @return the login or token refresh response
    */
    private hash<auto> getNewTokenIntern() {
        *string old_token = getToken();
        AutoLock al(login_lock);
        # use a token acquired by another thread while this one waited
        *string token = getToken();
        if (token.val() && token != old_token) {
            return {
                "token_type": getTokenType(),
                "access_token": token,
            };
        }
        return acquireTokenLocked();
    }

    #! Logs in with the Haltian login API
    /** @return the login response
    */
    private hash<auto> loginIntern() {
        return gotOAuth2LoginInfo(doTokenRequest(EmpathicBuildingRestClient::LoginUriPath, {
            "email": login_username,
            "password": login_password,
        }));
    }

    #! Processes a login or token refresh response
    /** The \c "token" value of the response is preferred to \c "access_token", and the token type defaults to
        \c "Bearer"

        @param h the response

        @return the processed response
    */
    private hash<auto> gotOAuth2LoginInfo(hash<auto> h) {
        if (h.token.typeCode() == NT_STRING && h.token != h.access_token) {
            h.access_token = h.token;
        }
        if (h.access_token.typeCode() != NT_STRING || !h.access_token.val()) {
            throw "EMPATHICBUILDING-LOGIN-ERROR", sprintf("the login response has no token: %y", h - "password");
        }
        if (h.token_type.typeCode() != NT_STRING || !h.token_type.val()) {
            h.token_type = "Bearer";
        }
        return RestClientIo::gotOAuth2LoginInfo(h);
    }

    #! Returns the connection options to update after a login
    /** @param h the login response

        @return the \c token_type, \c token, and \c refresh_token connection options that have changed
    */
    private *hash<auto> getUpdateOptionsAfterLogin(hash<auto> h) {
        *string token_type;
        *string token;
        *string cur_refresh_token;
        {
            AutoReadLock al(config_lock);
            token_type = token_type_val;
            token = token_val;
            cur_refresh_token = refresh_token;
        }
        hash<auto> new_opts;
        if (h.token_type && h.token_type != token_type) {
            new_opts.token_type = h.token_type;
        }
        if (h.access_token && h.access_token != token) {
            new_opts.token = h.access_token;
        }
        if (h.refresh_token && h.refresh_token != cur_refresh_token) {
            new_opts.refresh_token = h.refresh_token;
        }
        return new_opts;
    }

    #! Returns @ref True if a token can be acquired
    private bool canLogin() {
        if (login_username.val() && login_password.val()) {
            return True;
        }
        AutoReadLock al(config_lock);
        return refresh_token.val();
    }

    #! Acquires a new token; must be called with @ref login_lock held
    /** @return the login or token refresh response
    */
    private hash<auto> acquireTokenLocked() {
        *string current_refresh_token;
        {
            AutoReadLock al(config_lock);
            current_refresh_token = refresh_token;
        }
        bool have_login = login_username.val() && login_password.val();
        if ((broken_refresh || !current_refresh_token) && have_login) {
            return loginIntern();
        }
        if (!current_refresh_token) {
            throw "EMPATHICBUILDING-LOGIN-ERROR", "cannot acquire a token; no username and password or refresh "
                "token is set";
        }
        try {
            return gotOAuth2LoginInfo(doTokenRequest(EmpathicBuildingRestClient::TokenRefreshUriPath, {
                "refresh_token": current_refresh_token,
            }));
        } catch (hash<ExceptionInfo> ex) {
            # log in if the refresh token is not accepted
            if (have_login && ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 401) {
                return loginIntern();
            }
            rethrow;
        }
    }

    #! Sends a login or token refresh request and returns the deserialized response body
    /** The request is sent with the connection manager directly, without the token, so that it cannot recurse into
        token acquisition

        @param uri_path the request path relative to the connection path
        @param args the form arguments to send

        @return the deserialized response body

        @throw REST-RESPONSE-ERROR the server returned an error response; the exception argument is the response
        @throw EMPATHICBUILDING-LOGIN-ERROR the response is not a JSON object
    */
    private hash<auto> doTokenRequest(string uri_path, hash<auto> args) {
        *string conn_path = getConnectionPath();
        string path = conn_path ?? "";
        path =~ s/\/+$//;
        path += "/" + uri_path;
        hash<auto> hdr = {
            "content-type": MimeTypeFormUrlEncoded,
            "accept": MimeTypeJson,
        };
        log(LoggerLevel::DEBUG, "Haltian: POST %s%s", url, path);
        hash<auto> raw = mgr.request(url, "POST", path, hdr, mime_get_form_urlencoded_string(args));
        auto body = raw.body;
        if (body.typeCode() == NT_BINARY) {
            body = body.toString();
        }
        if (body.typeCode() == NT_STRING && body.val()) {
            try {
                body = parse_json(body);
            } catch (hash<ExceptionInfo> ex) {
                # keep the raw body
            }
        }
        if (raw.status_code < 200 || raw.status_code >= 300) {
            throw "REST-RESPONSE-ERROR", sprintf("HTTP %d response received to POST %s", raw.status_code, path),
                <RestClient::RestResponseInfo>{
                    "status_code": raw.status_code,
                    "headers": raw.headers,
                    "body": body,
                    "request_method": "POST",
                    "request_path": path,
                };
        }
        if (body.typeCode() != NT_HASH) {
            throw "EMPATHICBUILDING-LOGIN-ERROR", sprintf("POST %s returned a response that is not a JSON object "
                "(type %y)", path, body.type());
        }
        return body;
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
        return new EmpathicBuildingDataProvider({"restclient": getAsync()});
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

    #! Returns an asynchronous REST client
    /** @param connect if @ref False, the client does not log in until its first request
        @param rtopts runtime options

        @return an @ref EmpathicBuildingRestClientIo object

        @since EmpathicBuildingDataProvider 1.2
    */
    private EmpathicBuildingRestClientIo getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        return new EmpathicBuildingRestClientIo(getConnectionOptions(rtopts) + (connect ? NOTHING
            : {"skip_login": True}));
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
