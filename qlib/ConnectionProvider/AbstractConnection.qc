# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AbstractConnection class definition

/*  AbstractConnection.qc Copyright 2016 - 2024 Qore Technologies, s.r.o.

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

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
/** @defgroup PingFlags Ping Flags
*/
#/@{
#! Ping flag: throw exception on error
public const PF_THROW_EXCEPTION = (1 << 0);

#! Ping flag: automatic / scheduled ping
/** This should be set internally when calling ping() and can be used by connection classes to selectively disable
    automatic pings, for example, for metered or rate-limited connections
*/
public const PF_AUTOMATIC_PING =  (1 << 1);
#/@}

/** @defgroup ConnectionCustomizationStatuses Connection Customization Statuses
*/
#/@{
#! Customization status: unsupported
public const CCS_UNSUPPORTED = "unsupported";

#! Customization status: uncustomized
/** The connection supports customization, but customization has not yet been started
*/
public const CCS_UNCUSTOMIZED = "uncustomized";

#! Customization status: in-progress
/** The connection supports customization, and customization is currently in progress
*/
public const CCS_IN_PROGRESS = "in-progress";

#! Customization status: customized
/** The connection supports customization, and customization has been completed
*/
public const CCS_CUSTOMIZED = "customized";
#/@}

#! Abstract base class for connections
/** this class can be specialized to provide for user-defined connection types

    @deprecated use AbstractConnectionWithInfo instead
*/
public class AbstractConnection inherits Qore::Serializable, Logger::LoggerWrapper, OptionHelper {
    public {
        #! Connection name
        string name;

        #! Display name
        string display_name;

        #! Connection short description in plain text
        *string short_desc;

        #! Connection description with markdown formatting
        string desc;

        #! Connection URL (potentially with password info)
        string url;

        #! "safe" URL (password information removed)
        string safe_url;

        #! A string containing the real URL using a non-abstract / technical scheme
        string real_url;

        #! original connection options
        *hash<auto> orig_opts;

        #! connection options
        *hash<auto> opts;

        #! broken down URL hash (as returned by @ref Qore::parse_url())
        hash<auto> urlh;

        #! date/time of last check/ping
        *date last_check;

        #! status string; in case of a connection error this string will be the error string
        string status = "not checked";

        #! connection status flag; set by monitoring or explicit pings/connections
        bool up = True;

        #! monitoring flag
        bool monitor;

        #! date/time of last update
        date updated;

        #! set to @ref True "True" for loopback connections
        bool loopback = False;

        #! Customization support?
        bool supports_customization = False;

        #! Customization status
        string customization_status = CCS_UNSUPPORTED;

        #! Customization timestamp
        *date customization_timestamp;

        #! optional code for default post-processing of objects created by getImpl()
        /** must take the connection object and the object created as arguments
        */
        static *code post_processing;

        #! enabled/disabled indicator flag
        bool enabled = True;

        #! locked/unlocked flag
        bool locked = False;

        #! debug data flag; to be used an an indication for a higher-level implementation
        bool debug_data = False;

        #! connection tags; user-defined key-value pairs
        hash<auto> tags = {};

        #! if the data provider or any of its children support requests (APIs)
        bool children_can_support_apis = False;

        #! if the data provider or any of its children support records
        bool children_can_support_records = False;

        #! if the data provider or any of its children support events
        bool children_can_support_observers = False;

        #! if the data provider or any of its children support messages
        bool children_can_support_messages = False;

        #! if the data provider or any of its children support transaction management
        bool children_can_support_transactions = False;

        #! internal tags (serializable)
        hash<auto> internal_info;

        #! If automatic pings are disabled
        bool disable_automatic_pings;

        #! internal transient tags (non-serializable)
        transient hash<auto> transient_info;

        #! Connection feature: Logger support
        /** Indicates that connection objects support setting / updating a logger interface object in the connection
        */
        const CF_LOGGER = "logger";

        #! Connection feature: Update Options support
        /** Returned as a connection feature indicating that the connection object inherits
            @ref UpdateOptionsInterface and therefore supports setting a closure to update connection options in
            external systems (in blocking I/O calls)

            @see @ref UpdateOptionsInterface
        */
        const CF_UPDATE_OPTIONS = "update-options";

        #! Default option description
        const DefaultDescription = "n/a; update connection to use the `AbstractConnectionWithInfo` class instead";

        #! Connection info keys mapped 1:1 to DataProviderOptionInfo keys
        const CommonConnectionKeys = (
            "allowed_values",
            "allowed_values_creatable",
            "default_value",
            "desc",
            "display_name",
            "example_value",
            "short_desc",
            "sensitive",
        );
    }

    #! creates the AbstractConnection object
    /** @param name the name of the connection
        @param desc connection description
        @param url connection URL (potentially with password info)
        @param monitor monitoring flag
        @param opts connection options
        @param urlh broken down URL hash (as returned by @ref Qore::parse_url())
        @param safe_url "safe" URL (password information removed); if not set this will be set automatically by
        calling @ref getSafeUrl()
        @param enabled enabled/disabled flag

        @note use constructor(hash<auto> config, *hash<auto> attr instead

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
        @throw CONNECTION-URL-ERROR URL is invalid, because it contains control characters
    */
    deprecated
    constructor(string name,
            string desc,
            string url,
            bool monitor,
            *hash opts,
            hash urlh,
            *string safe_url,
            *bool enabled) {
        # remove "enabled" option from option hash, if any
        *bool opt_enabled = remove opts.enabled;
        hash<auto> attributes = {
            "monitor": monitor,
            "enabled": enabled ?? opt_enabled ?? True,
        };
        constructorInit({
            "name": name,
            "desc": desc,
            "url": url,
            "opts": opts,
        }, attributes);
    }

    #! creates the AbstractConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes; see below
        @param options connection options
        @param logger any logger interface to use for the connection

        @par Attributes
        Attributes are special flags for given connection - stored as public
        attributes of the object instance.
        For now there are:
        - \c enabled (bool), default \c True
        - \c monitor (bool), default \c True
        - \c locked (bool), default \c False
        - \c tags (hash), no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
        @throw CONNECTION-URL-ERROR URL is invalid, because it contains control characters

        @note use constructor(hash<auto> config, *hash<auto> attr instead
    */
    deprecated constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {}, *LoggerInterface logger) : LoggerWrapper(logger) {
        constructorInit({
            "name": name,
            "desc": description,
            "url": url,
            "opts": options,
        }, attributes);
    }

    #! creates the AbstractConnection object
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
        @throw CONNECTION-URL-ERROR URL is invalid, because it contains control characters
    */
    constructor(hash<auto> config, *hash<auto> attr) {
        constructorInit(config, attr);
    }

    #! Returns the connection name
    /** @return the connection name

        @since %ConnectionProvider 1.7
    */
    string getName() {
        return name;
    }

    #! Returns the connection display name
    /** @return the connection display name

        @since %ConnectionProvider 2.0
    */
    string getDisplayName() {
        return display_name;
    }

    #! Returns the connection short description
    /** @return the connection short description

        @since %ConnectionProvider 2.0
    */
    string getShortDescription() {
        return short_desc;
    }

    #! Returns the connection description
    /** @return the connection description

        @since %ConnectionProvider 1.7
    */
    string getDescription() {
        return desc;
    }

    #! Returns the connection URL
    /** @return the connection URL

        @since %ConnectionProvider 1.7
    */
    string getUrl() {
        return url;
    }

    #! Returns the connection safe URL (without password info)
    /** @return the connection safe URL (without password info)

        @since %ConnectionProvider 1.7
    */
    string getSafeUrl() {
        return safe_url;
    }

    #! Returns True if the connection requires OAuth2 authorization before it can be used
    /** @return @ref True if the connection requires OAuth2 authorization, @ref False if already authorized, or the
        connection does not support authorization

        This base class method returns @ref False by default

        @since %ConnectionProvider 2.0
    */
    bool needsAuth() {
        return False;
    }

    #! Returns the data provider application name this connection serves, if any
    /** @return the data provider application name this connection serves, if any

        @since %ConnectionProvider 2.0
    */
    *string getAppName() {
        # intentionally empty
    }

    #! returns a hash with the results of the ping operation
    /** @param throw_exception throw an exception if a connection error occurs

        @return a @ref PingInfo hash
    */
    hash<PingInfo> ping(bool throw_exception) {
        return ping(throw_exception ? PF_THROW_EXCEPTION : 0);
    }

    #! returns a hash with the results of the ping operation
    /** @param ping_flags see @ref PingFlags for more information

        @return a @ref PingInfo hash

        @since %ConnectionProvider 2.0
    */
    hash<PingInfo> ping(int ping_flags = 0) {
        hash<PingInfo> rv(self{"name", "desc", "opts"} + {
            "ping_flags": ping_flags,
        });
        rv.url = safe_url;

        last_check = now_us();

        if (ping_flags & PF_THROW_EXCEPTION) {
            return pingIntern(rv);
        }

        try {
            return pingIntern(rv);
        } catch (hash<ExceptionInfo> ex) {
            date delta = now_us() - last_check;
            handlePingFailed(delta, ex);
            return rv += {
                "ok": False,
                "time": delta,
                "info": sprintf("%s: %s", ex.err, ex.desc),
                "ex": check_exception_serializable(ex),
            };
        }
    }

    #! Starts customization for the object
    /** @bool restart Restart any customization in progress

        @throw CUSTOMIZATION-UNSUPPORTED this exception is thrown if the connection does not support customization
        @throw CUSTOMIZATION-IN-PROGRESS this exception is thrown if customization is already in progress and
        \a restart is not @ref True
    */
    startCustomization(*bool restart) {
        if (customization_status == CCS_IN_PROGRESS) {
            throw "CUSTOMIZATION-IN-PROGRESS", "";
        }
        startCustomizationImpl();
    }

    #! performs the ping by executing get(); returns a @ref PingInfo hash
    private hash<PingInfo> pingIntern(hash<PingInfo> rv) {
        date start = now_us();
        if (!(rv.ping_flags & PF_AUTOMATIC_PING) || !disable_automatic_pings) {
            pingImpl();
        }
        date delta = now_us() - start;
        handlePingSuccess(delta);
        return rv += {
            "ok": True,
            "time": delta,
            "info": "OK",
        };
    }

    #! returns a @ref ConnectionInfo hash of information about the connection
    /** @param with_password determines if the password is returned or not

        @return a @ref ConnectionInfo hash of information about the connection

        @deprecated use getInfo(*hash<auto> opts) instead
    */
    hash<ConnectionInfo> getInfo(bool with_password = False) {
        return getInfo({
            "with_passwords": with_password,
        });
    }

    #! returns a @ref ConnectionInfo hash of information about the connection
    /** @param opts connection information options; options are connection-specific; the only universal option is
        \c with_passwords: if @ref True, then passwords are returned in the info hash

        @return a @ref ConnectionInfo hash of information about the connection

        @note Some of the information in this hash may be non-serializable objects, for a method that returns only
        serializable information about the connection, see @ref getInfoAsData()
    */
    hash<ConnectionInfo> getInfo(*hash<auto> opts) {
        hash<ConnectionInfo> h = cast<hash<ConnectionInfo>>(self{
            "name", "display_name", "short_desc", "desc", "url", "monitor", "last_check", "updated", "status", "up",
            "loopback", "enabled", "locked", "supports_customization", "customization_status",
            "customization_timestamp", "debug_data", "tags", "children_can_support_apis",
            "children_can_support_records", "children_can_support_observers", "children_can_support_messages",
            "children_can_support_transactions",
        } + {
            "type": getType(),
            "url_hash": urlh,
            "has_provider": hasDataProvider(),
            "features": getFeatures(),
            "dataprovider_subtypes": getDataProviderSubtypes(),
            "needs_auth": needsAuth(),
        });
        h.opts = orig_opts;
        if (!opts.with_passwords) {
            h.url = safe_url;
            delete h.url_hash.password;
            # remove all sensitive option values as well
            map remove h.opts{$1}, keys getConnectionSchemeInfo().sensitive_options;
        }
        # sort options by key
        if (h.opts) {
            h.opts = h.opts{sort(keys h.opts)};
        }
        return h;
    }

    #! Returns a @ref ConnectionDataInfo hash of information about the connection
    /** @param opts connection information options; options are connection-specific; the only universal option is
        \c with_passwords: if @ref True, then passwords are returned in the info hash

        @return a @ref ConnectionDataInfo hash of information about the connection

        @note All the information in this hash is data; some info in the output of @ref getInfo() may be objects

        @since ConnectionProvider 2.0
    */
    hash<ConnectionDataInfo> getInfoAsData(*hash<auto> opts) {
        hash<ConnectionInfo> h = getInfo(opts);
        hash<ConnectionDataInfo> rv = cast<hash<ConnectionDataInfo>>(h - "dataprovider_subtypes");
        foreach hash<auto> i in (h.dataprovider_subtypes.pairIterator()) {
            foreach hash<auto> j in (i.value.pairIterator()) {
                rv.dataprovider_subtypes{i.key}{j.key}.type = map $1.getName(), j.value.type;
            }
        }
        return rv;
    }

    #! Returns a list of connection-defined features
    /** @since ConnectionProvider 1.10
    */
    *hash<string, bool> getFeatures() {
        return getFeaturesImpl();
    }

    #! Returns connection info plus any extended information returned by the connection
    /** The default implementation in the base class returns the same value as getInfo(*hash<auto>)

        @param opts connection information options; options are connection-specific; the only universal option is
        \c with_passwords: if @ref True, then passwords are returned in the info hash

        @return Connection info plus any extended information returned by the connection

        @note Some of the information in this hash may be non-serializable objects, for a method that returns only
        serializable information about the connection, see @ref getExtendedInfoAsData()

        @since ConnectionProvider 1.5
    */
    hash<auto> getExtendedInfo(*hash<auto> opts) {
        # ensure hash<auto> for the return type
        hash<auto> h += getInfo(opts);
        return h;
    }

    #! Returns connection info plus any extended information returned by the connection
    /** The default implementation in the base class returns the same value as getInfoAsData(*hash<auto>)

        @param opts connection information options; options are connection-specific; the only universal option is
        \c with_passwords: if @ref True, then passwords are returned in the info hash

        @return Connection info plus any extended information returned by the connection

        @note All the information in this hash is data; some info in the output of @ref getExtendedInfo() may be objects

        @since ConnectionProvider 2.0
    */
    hash<auto> getExtendedInfoAsData(*hash<auto> opts) {
        # ensure hash<auto> for the return type
        hash<auto> h += getInfoAsData(opts);
        return h;
    }

    #! returns a @ref ConfigInfo hash of static configuration information about the connection
    /** @returns a @ref ConfigInfo hash of static configuration information about the connection
    */
    hash<ConfigInfo> getConfigHash() {
        return cast<hash<ConfigInfo>>(self{"name", "desc", "url", "opts"});
    }

    #! Returns a default ConnectionSchemeInfo hash
    hash<ConnectionSchemeInfo> getConnectionSchemeInfo() {
        return getConnectionSchemeInfoImpl();
    }

    #! Override in subclasses to return a native ConnectionSchemeInfo hash
    hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        hash<string, hash<ConnectionOptionInfo>> options;
        *hash<auto> opt = getOptions();
        if (opt) {
            *hash<auto> def = getDefaultOptions();
            options = map {
                $1.key: <ConnectionOptionInfo>{
                    "display_name": $1.key,
                    "short_desc": DefaultDescription,
                    "type": "any",
                    "desc": DefaultDescription,
                    "default_value": def{$1.key},
                },
            }, opt.pairIterator();
        }
        string cname = self.className();
        string desc = sprintf("%s connection", cname);
        return <ConnectionSchemeInfo>{
            "display_name": sprintf("%s Connection", cname),
            "short_desc": desc,
            "desc": desc,
            "cls": Class::getClass(self),
            "options": options,
            "schemes": {
                getType(): True,
            },
        };
    }

    #! returns static / initialization options
    *hash<auto> getOptions() {
    }

    #! returns runtime options
    *hash<auto> getRuntimeOptions() {
    }

    #! returns default options
    *hash<auto> getDefaultOptions() {
    }

    #! returns options for saving the connection's configuration information
    *hash<auto> getRealOptions() {
        return orig_opts;
    }

    #! returns options for creating a new connection
    *hash<auto> getConnectionOptions(*hash<auto> rtopts) {
        return orig_opts + rtopts;
    }

    #! returns the underlying connection object
    /** calls @ref getImpl() to actually acquire the connection object
    */
    object get(bool connect = True, *hash<auto> rtopts) {
        if (!connect) {
            object o = getImpl(False, rtopts);
            if (post_processing) {
                post_processing(self, o);
            }
            return o;
        }

        on_exit
            last_check = now_us();

        # issue #4852: do not change the connection status based on whether or not a connection can be made

        try {
            object o = getImpl(True, rtopts);
            if (post_processing) {
                post_processing(self, o);
            }
            return o;
        } catch (hash<ExceptionInfo> ex) {
            status = sprintf("%s: %s", ex.err, ex.desc);
            rethrow;
        }
    }

    #! this method is called when parsing connection file definitions from text files when loading into the database
    /** reimplement this method in subclasses to parse the option hash as set from a text file if needed (for special
        option formats, etc)
    */
    parseTextOptions() {
    }

    #! returns all tags associated with the connection
    hash<auto> getTags() {
        return tags;
    }

    #! returns the value of the given tag or @ref nothing if not present
    auto getTag(string tag) {
        return tags{tag};
    }

    #! returns the value of the given tag or throws an exception if the tag is not set with a value
    /** @throw TAG-ERROR the given tag is not set on the connection
    */
    auto getTagEx(string tag) {
        auto rv = tags{tag};
        if (!exists rv) {
            throw "TAG-ERROR", sprintf("tag %y is not set on connection %y", tag, name);
        }
        return rv;
    }

    #! returns @ref True if the connection returns a data provider with the @ref getDataProvider() method
    /** @return @ref True if the connection returns a data provider with the @ref getDataProvider() method

        This base class method returns @ref False; to support the data provider API, override this method in child
        classes

        @see @ref getDataProvider()

        @since ConnectionProvider 1.3
    */
    bool hasDataProvider() {
        return False;
    }

    #! returns a data provider object for this connection, if supported
    /** @param constructor_options any additional constructor options for the data provider

        @return a data provider object for this connection, if supported

        @throw DATA-PROVIDER-ERROR this object does not support the data provider API

        This base class method throws a \c DATA-PROVIDER-ERROR exception by default; to support the data provider
        API, override this method in child classes

        @see hasDataProvider()

        @since
        - ConnectionProvider 1.3
        - ConnectionProvider 2.0 added the optional \c constructor_options argument
    */
    AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        throw "DATA-PROVIDER-ERROR", sprintf("%y connections do not support the data provider API", getType());
    }

    #! Returns a data provider object for the connection and subtype passed as an argument
    /** @param subtype the subtype of data provider supported, in case the connection can return multiple data providers
        @param constructor_options any additional constructor options required by the data provider

        @return a data provider object for the connection and subtype passed as an argument

        @throw DATA-PROVIDER-ERROR this object does not support the data provider API or the given subtype is not valid

        @see
        - hasDataProvider()
        - getDataProviderSubtypes()

        @since ConnectionProvider 2.0
    */
    AbstractDataProvider getDataProvider(string subtype, *hash<auto> constructor_options) {
        if (subtype == getType()) {
            return getDataProvider(constructor_options);
        }
        return getDataProviderImpl(subtype, constructor_options);
    }

    #! Returns a hash of data provider constructor option info, if applicable
    /** @return a hash of data provider constructor option info for the primary data provider, if applicable

        @since ConnectionProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getDataProviderConstructorOptions() {
        return getDataProviderConstructorOptionsImpl();
    }

    #! Returns a hash of supported data provider subtypes and constructor option info
    /** @return a hash of supported data provider subtypes and constructor option info; top-level keys are data
        provider subtypes supported by this connection, values are hashes of data provider constructor option
        information

        @since ConnectionProvider 2.0
    */
    *hash<string, *hash<string, hash<DataProviderOptionInfo>>> getDataProviderSubtypes() {
        return getDataProviderSubtypesImpl();
    }

    #! returns @ref True if the connection supports the %Qore @ref Qore::Socket "Socket-based" polling API
    /** @return @ref True if the connection supports the polling API where multiple connections can be polled from a
        single thread

        This base class method returns @ref False; to support the new polling API, override this method in child
        classes

        @see
        - @ref startConnectPoll()

        @since ConnectionProvider 1.7
    */
    bool supportsPollingApi() {
        return False;
    }

    #! Called to start the connection polling operation
    /** Only called if the connection supports the polling API

        @param an optional logger interface for any object used for polling

        @return a socket poll operation object that will allow the connection goal to be reached with polling

        @see supportsPollingApi()

        @since ConnectionProvider 1.7
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        throw "UNSUPPORTED", sprintf("connection %y (%s) does not support the polling API", name, safe_url);
    }

    #! Returns an unconnected object for a non-blocking poll operation
    /** @return an unconnected object for a non-blocking poll operation

        @since ConnectionProvider 1.7.1
    */
    object getPollImpl() {
        return getImpl(False);
    }

    #! Handles a successful ping operation on the connection
    handlePingSuccess(date delta) {
        up = True;
        status = "OK";
    }

    #! Handles a failed ping operation on the connection
    handlePingFailed(date delta, hash<ExceptionInfo> ex) {
        up = False;
        status = sprintf("%s: %s", ex.err, ex.desc);
    }

    #! Handles a failed ping operation on the connection
    handlePingFailed(date delta, string err) {
        up = False;
        status = err;
    }

    #! Returns a ConnectionOptionInfo hash from a DataProviderOptionInfo hash
    static hash<ConnectionOptionInfo> getConnectionOptionInfo(hash<DataProviderOptionInfo> dinfo) {
        hash<ConnectionOptionInfo> rv = cast<hash<ConnectionOptionInfo>>(dinfo{CommonConnectionKeys});
        rv.type = dinfo.type[0].getBaseTypeName();
        rv.type =~ s/<.*>//;
        return rv;
    }

    #! Returns a hash of ConnectionOptionInfo hashes from a hash of DataProviderOptionInfo hashes
    static hash<string, hash<ConnectionOptionInfo>> getConnectionOptionInfo(
                hash<string, hash<DataProviderOptionInfo>> dinfo) {
        return cast<hash<string, hash<ConnectionOptionInfo>>>(map {
            $1.key: AbstractConnection::getConnectionOptionInfo($1.value)
        }, dinfo.pairIterator());
    }

    #! Returns the real URL for a connection URL
    static string getRealUrl(string url, hash<ConnectionSchemeInfo> info, string type) {
        string scheme = (url =~ x/^([^:]+)/)[0];
        *hash<auto> rmap = map {$1.value: $1.key}, info.base_scheme_map.pairIterator();
        if (*string real_scheme = rmap{scheme}) {
            return regex_subst(url, "^" + scheme, real_scheme);
        }
        bool ssl;
        # remove type if possible
        if (url.equalPartial(type)) {
            int len = type.length();
            # set SSL if the scheme ends in an "s" after the expected scheme
            ssl = url[len..len+3] == "s://";
        } else {
            ssl = (scheme =~ /s$/);
        }
        string targ = "http" + (ssl ? "s" : "");
        return regex_subst(url, "^" + scheme, targ);
    }

    #! Starts customization for the object
    /** @throw CUSTOMIZATION-UNSUPPORTED this exception is thrown if the connection does not support customization
    */
    private startCustomizationImpl() {
        throw "CUSTOMIZATION-UNSUPPORTED", "This connection does not support customization";
    }

    #! returns a data provider object for the connection and subtype passed as an argument
    /** @param subtype the subtype of data provider supported, in case the connection can return multiple data providers
        @param constructor_options any additional constructor options required by the data provider

        @return a data provider object for the connection and subtype passed as an argument

        @throw DATA-PROVIDER-ERROR this object does not support the data provider API or the given subtype is not valid

        This base class method throws a \c DATA-PROVIDER-ERROR exception by default; to support the data provider
        API, override this method in child classes

        @see
        - hasDataProvider()
        - getDataProviderSubtypes()

        @since ConnectionProvider 2.0
    */
    private AbstractDataProvider getDataProviderImpl(string subtype, *hash<auto> constructor_options) {
        throw "DATA-PROVIDER-ERROR", hasDataProvider()
            ? sprintf("%y connections do not support data provider subtypes", getType())
            : sprintf("%y connections do not support the data provider API", getType());
    }

    #! Returns a hash of data provider constructor option info, if applicable
    /** @return a hash of data provider constructor option info for the primary data provider, if applicable

        @since ConnectionProvider 2.0
    */
    private *hash<string, hash<DataProviderOptionInfo>> getDataProviderConstructorOptionsImpl() {
        # the base class method returns NOTHING by default
    }

    #! Returns a hash of supported data provider subtypes
    /** @return a hash of supported data provider subtypes

        @since ConnectionProvider 2.0
    */
    private *hash<string, *hash<string, hash<DataProviderOptionInfo>>> getDataProviderSubtypesImpl() {
        # no subtypes supported by default in the base class
    }

    #! Returns options for each data provider subtype
    /**
        @param constructor_options any additional constructor options required by the data provider
    */
    private *hash<auto> getSubtypeOptions(*hash<string, hash<DataProviderOptionInfo>> subtype_options,
            *hash<auto> constructor_options) {
        *hash<auto> opts = getRealOptions();
        return map {$1.key: $1.value},
            (map {$1: constructor_options{$1} ?? opts{$1}}, keys subtype_options).pairIterator(),
            exists $1.value;
    }

    #! performs the internal ping
    /** By default this creates a new connection only; override in child classes to implement additional ping logic
    */
    private pingImpl() {
        get();
    }

    #! Parse the URL to a hash
    /** @param url a string with url
        @return hash untyped, depends on the parsing method

        The base implementation calls @ref Qore::parse_url().

        Reimplement \c parseUrl() if your connection uses different URL scheme.
        Like eg. Qore::Datasource connection string.
    */
    private hash<UrlInfo> parseUrl(string url) {
        return Qore::parse_url(url);
    }

    #! creates a "safe" URL string with password information removed
    /** @param urlh a parsed hash (as returned from parseUrl()
        @return string with safe URL

        This base/default implementation expects the \c urlh
        as returned by @ref Qore::parse_url())

        The public attribute \c safe_url is created from the \c url calling
        AbstractConnection::getSafeUrl(). Reimplement this method if your
        connection does not follow the standard URL format like:
        @code
scheme://user:pass@hostname:port/path
        @endcode
    */
    private string getSafeUrl(hash<auto> urlh) {
        string url = urlh.protocol + "://";
        bool has_user_or_pass;
        if (urlh.username) {
            url += urlh.username;
            has_user_or_pass = True;
        }
        if (urlh.password) {
            url += ":<masked>";
            if (!has_user_or_pass) {
                has_user_or_pass = True;
            }
        }
        if (has_user_or_pass) {
            url += "@";
        }
        url += urlh.host;
        if (urlh.port)
            url += ":" + urlh.port;
        if (urlh.path && urlh.path != "/")
            url += urlh.path;
        return url;
    }

    #! performs options validation in the constructor
    /** @param options a hash with options from the constructor
        @param check_required check that all required options are set

        @return validated option hash

        This method can be overloaded in connections where are options created dynamically for example

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    private hash<auto> validateOptions(hash<auto> options, bool check_required = True) {
        return ConnectionSchemeCache::validateConnectionOptions(name, getConnectionSchemeInfo(), options,
            check_required);
    }

    #! Common construtor implementation
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored?
        - enabled (optional bool): is the connection enabled?
        - locked (optional bool): is the connection locked?
        - debug_data (optional bool): debug data?
        - tags (optional hash): tags for the connection

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
        @throw CONNECTION-URL-ERROR URL is invalid, because it contains control characters
    */
    private:internal constructorInit(hash<auto> config, *hash<auto> attr) {
        # string name, string description, string url, hash<auto> attributes, hash<auto> options) {
        # metadata
        name = getString(config, "name", True);
        display_name = getString(config, "display_name") ?? AbstractDataField::getDisplayName(name);
        desc = getString(config, "desc") ?? AbstractDataProviderType::DefaultDesc;
        short_desc = getString(config, "short_desc") ?? AbstractDataField::getShortDescription(desc);

        # url handling
        url = getString(config, "url", True);
        # remove trailing newlines from the URL
        url =~ s/[\n\r]+$//;
        # check for control characters
        if (url =~ /[\x00-\x1f]/) {
            throw "CONNECTION-URL-ERROR", sprintf("Connection url %y is invalid, because it contains control "
                "characters", url);
        }
        urlh = parseUrl(self.url);
        safe_url = getSafeUrl(self.urlh);

        # attributes
        monitor = getBool(attr, "monitor") ?? True;
        enabled = getBool(attr, "enabled") ?? True;
        locked = getBool(attr, "locked") ?? False;
        debug_data = getBool(attr, "debug_data") ?? False;
        tags = getHash(attr, "tags") ?? {};

        # set logger, if any
        if (config.logger) {
            if (config.logger instanceof LoggerInterface) {
                setLogger(config.logger);
            } else {
                throw "CONNECTION-OPTION-ERROR", sprintf("Connection option \"logger\" is type %y; expecting a "
                    "\"LoggerInterface\" object", config.logger.fullType());
            }
        }

        opts = validateOptions(getHash(config, "opts") ?? {});
        orig_opts = opts;

        if (opts.disable_automatic_pings) {
            disable_automatic_pings = True;
        }

        setChildCapabilities();
        setRealUrl(url);
    }

    #! Called by the constructor to set the real URL
    /** @path the new URL

        sets the real HTTP URL for HTTP-based connections
    */
    private setRealUrl(*string url) {
        if (exists url) {
            real_url = url;
        }

    }

    #! Returns a hash of connection-defined features
    /** This method returns no value; override in child classes to return features

        @since ConnectionProvider 1.10
    */
    private *hash<string, bool> getFeaturesImpl() {
    }

    #! Override to set child data provider capabilities once per child class
    private setChildCapabilities() {
        # this method intentionally left empty
    }

    #! returns the string type name for the connection object
    abstract string getType();

    #! this method must return the connection object corresponding to the object's configuration
    /** @param connect if @ref True "True" then the connection should be returned connected (and if possible a
        round-trip "ping" of the server should be made)
        @param rtopts if the object acquisition requires any runtime options, then these are passed here

        @return the connection object
    */
    private abstract object getImpl(bool connect = True, *hash<auto> rtopts);
}
} # ConnectionProvider namespace
