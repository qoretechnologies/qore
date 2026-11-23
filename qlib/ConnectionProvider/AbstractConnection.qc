# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AbstractConnection class definition

/*  AbstractConnection.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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
public const int PF_THROW_EXCEPTION = (1 << 0);

#! Ping flag: automatic / scheduled ping
/** This should be set internally when calling ping() and can be used by connection classes to selectively disable
    automatic pings, for example, for metered or rate-limited connections
*/
public const int PF_AUTOMATIC_PING =  (1 << 1);
#/@}

/** @defgroup ConnectionCustomizationStatuses Connection Customization Statuses
*/
#/@{
#! Customization status: unsupported
public const string CCS_UNSUPPORTED = "unsupported";

#! Customization status: uncustomized
/** The connection supports customization, but customization has not yet been started
*/
public const string CCS_UNCUSTOMIZED = "uncustomized";

#! Customization status: in-progress
/** The connection supports customization, and customization is currently in progress
*/
public const string CCS_IN_PROGRESS = "in-progress";

#! Customization status: customized
/** The connection supports customization, and customization has been completed
*/
public const string CCS_CUSTOMIZED = "customized";
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
        /** \c "not checked" until the connection is checked by a ping or a connection is made with it; then \c "OK"
            or the error string of the last failed check
        */
        string status = "not checked";

        #! connection status flag: @ref True "True" only when the connection has been proven to be up
        /** A connection is not up until it is proven to be up: this flag is @ref False "False" for a connection that
            has never been checked (then @ref status is \c "not checked" and @ref last_check is @ref nothing, so a
            reader can tell a connection that was never checked from one that is down).

            It is set to @ref True "True" by a successful ping (@ref handlePingSuccess(), called by @ref ping() and by
            connection monitoring) and by a successful connection made with @ref get() with \a connect
            @ref True "True", unless the last ping failed, and to @ref False "False" by a failed ping
            (@ref handlePingFailed()), also one that throws its error.  It is not changed by:
            - an object acquired with \a connect @ref False "False" or with @ref getAsync(), which does not prove that
              the connection can be made (async objects connect on demand): a connection that is used only through
              such objects, including the data providers that use them (the REST connections' data providers, for
              example), stays not up until it is pinged
              (<a href="https://github.com/qoretechnologies/qore/issues/7274">issue 7274</a> tracks marking it up
              on the first successful request of an async client)
            - a connection that cannot be made with @ref get() or @ref getAsync(), which sets @ref status to its error
              only (issue #4852)
            - an automatic ping (@ref PF_AUTOMATIC_PING) skipped because the connection disables automatic pings
            - a successful connection made with @ref get() after a failed ping: a connection that a ping found down
              comes up with a successful ping, whose transition a monitor handles

            What a ping checks depends on the connection class (its \c pingImpl() method): by default it makes a connection
            with @ref get(), and connection classes check more, such as a request to the server.  A REST connection
            without the \c ping_method option, for example, checks its configuration (a connection that is not yet
            authorized fails) and that the server can be reached (TCP and TLS), but not its credentials or its
            authorization against the server, which only a ping with \c ping_method or a request checks; up then
            means reachable and configured, not authenticated.

            A connection object restored by deserialization or copied keeps the state of the object it was made from,
            except that a connection serialized with the status \c "not checked" is restored as not up (one
            serialized before %ConnectionProvider 3.1 has \c up @ref True "True" with that status).  A connection
            serialized before %ConnectionProvider 3.1 that was never pinged but whose connection failed with
            @ref get() has \c up @ref True "True" with the error as its status, which cannot be told apart from a
            connection that was up, and it is restored as up.  The state is
            changed and read under a lock, so @ref up and @ref status are consistent in @ref getInfo().

            @since %ConnectionProvider 3.1 a connection that was never checked is not up (it was up before)
        */
        bool up = False;

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

            @deprecated use setPostProcessing(), whose code sandboxed code cannot read: any code can read this public
            member, also the code of sandboxed tenants, which would get the host's code (qore#6749); code set with
            setPostProcessing() is used instead of the code set here
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
        const string CF_LOGGER = "logger";

        #! Connection feature: Update Options support
        /** Returned as a connection feature indicating that the connection object inherits
            @ref UpdateOptionsInterface and therefore supports setting a closure to update connection options in
            external systems (in blocking I/O calls)

            @see @ref UpdateOptionsInterface
        */
        const string CF_UPDATE_OPTIONS = "update-options";

        #! Connection feature: file location support
        /** Indicates that the connection can be used in \c "conn://" file locations with the
            \c FileLocationHandler module

            @see
            - @ref supportsFileLocations()
            - @ref getFileLocationRedirect()
            - @ref getFileLocationHandler()

            @since ConnectionProvider 3.1
        */
        const string CF_FILE_LOCATIONS = "file-locations";

        #! Default option description
        const string DefaultDescription = "n/a; update connection to use the `AbstractConnectionWithInfo` class instead";

        #! Connection info keys mapped 1:1 to DataProviderOptionInfo keys
        const list<string> CommonConnectionKeys = (
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

        @note use constructor(\c hash&lt;auto&gt; config, \c *hash&lt;auto&gt; attr) instead

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
        @throw CONNECTION-URL-ERROR URL is invalid, because it contains control characters

        @throw PERMISSION-DENIED the calling code is restricted from the functional domain of the connection's
        clients (see getConnectionDomainsImpl()); a connection is a capability of the code that creates it, also
        when it is deserialized (qore#7052)
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

        @note use constructor(\c hash&lt;auto&gt; config, \c *hash&lt;auto&gt; attr) instead

        @throw PERMISSION-DENIED the calling code is restricted from the functional domain of the connection's
        clients (see getConnectionDomainsImpl()); a connection is a capability of the code that creates it, also
        when it is deserialized (qore#7052)
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

    private:internal {
        #! Refuses sandboxed callers a connection of a module with uncontrolled external I/O
        /** Created with the connection, also when it is deserialized, so that it runs with the connection's
            provenance; created while serving a sandboxed caller, it would run under that caller's sandbox for every
            caller
        */
        transient DataProvider::DataProviderSandboxCheck sandbox_check();

        #! Serializes the changes of the connection's state (@ref up and @ref status) and their reads (issue #7236)
        transient Mutex state_lock();

        #! @ref True "True" while the last ping failed: then only a successful ping marks the connection up again
        /** A connection made with get() marks only a connection that no failed ping marked down up, so that a
            connection that a monitor saw go down comes up through a ping and its transition handling (issue #7236)
        */
        bool down_by_ping;

        #! The functional domains that the code that created the connection is restricted from (qore#6981)
        /** Set when the connection is created, also when it is deserialized: code restricted from the domains that
            the connection's clients use cannot create the connection (qore#7052; see checkCreation() and
            getConnectionDomainsImpl())
        */
        transient int creator_restrictions = AbstractConnection::getCreatorRestrictions();

        #! The proof that the code that created the connection may create an HTTP client connection manager (qore#7262)
        /** Taken when the connection is created, also when it is deserialized, and bound to the connection's URL then;
            get() and getAsync() grant it to the next HTTP client created in their thread (see
            HttpClientIo::HttpClientCreatorProofGrant), so a connection that trusted code created and lent creates
            clients for the code that uses it that are the trusted code's, which reach only the connection's target.
            It is never handed to the connection's class
        */
        transient *HttpClientIo::HttpClientCreatorProof creator_proof;
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
        @throw PERMISSION-DENIED the calling code is restricted from the functional domain of the connection's
        clients (see getConnectionDomainsImpl()); a connection is a capability of the code that creates it, also
        when it is deserialized (qore#7052)
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

    #! Refuses a sandboxed caller a connection of a module that registers an app with uncontrolled external I/O
    /** A connection of a module that registers an app with
        @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware" (see the DataProvider module's
        documentation of apps with external I/O) does that I/O for its callers, so a sandboxed caller
        is refused it, also when trusted code created the connection: get(), getAsync(), ping(),
        getFileLocationHandler() and getFileLocationRedirect() call this method first.  The connection's class
        hierarchy decides, as DataProvider::DataProviderSandboxCheck describes, so a subclass defined elsewhere is
        refused like the class that it inherits.

        A connection subclass calls this method in any other public method that does I/O.  Code that serves a
        sandboxed caller on a thread that no sandbox governs (ex: a request of a sandboxed user handled by a server
        thread) calls it with @p sandboxed set to @ref True.

        @param sandboxed whether the caller is sandboxed; if not given, Qore::SandboxManager::isRestricted() decides

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and the connection's module registers an app with
        network or filesystem I/O that the sandbox does not control and that is not declared sandbox-aware

        @par Example:
        @code{.py}
# a sandboxed user's request, served on a server thread
conn.checkSandboxAccess(True);
        @endcode

        @since %ConnectionProvider 3.1
    */
    checkSandboxAccess(*bool sandboxed) {
        if (!(sandboxed ?? SandboxManager::isRestricted())) {
            return;
        }
        if (sandbox_check.needsCheck(True)) {
            sandbox_check.check("connection", name, (Class::getClass(self),), True);
        }
    }

    #! Returns a redirect to an existing file location scheme for the given path, if supported
    /** @param path the path relative to the connection's own base location
        @param write @ref True if the location will be written to, @ref False if it will be read

        @return a @ref FileLocationRedirect hash, or @ref nothing if this connection does not support file locations
        by redirect

        Connections that map cleanly onto a scheme already supported by the \c FileLocationHandler module (the local
        filesystem, FTP, SFTP, plain HTTP) implement this method rather than implementing file I/O themselves; the
        \c "conn://" location handler re-enters normal dispatch for the returned scheme, so that handler's blocking,
        streaming, and polling implementations are all reused unchanged.

        Connections carrying state that cannot be expressed in a URL (OAuth2 tokens with automatic refresh,
        SPNEGO configuration, certificate data supplied inline, per-connection headers) implement
        @ref getFileLocationHandler() instead.

        @note The returned location may contain password information; implementations must never place it in an
        exception message or log entry

        @since ConnectionProvider 3.1

        @see
        - @ref supportsFileLocations()
        - @ref getFileLocationHandler()
    */
    *hash<FileLocationRedirect> getFileLocationRedirect(string path, bool write) {
        # the common path is one call: only a sandboxed caller is checked
        if (SandboxManager::isRestricted()) {
            checkSandboxAccess(True);
        }
        return getFileLocationRedirectImpl(path, write);
    }

    #! Returns @ref True if this connection provides a native file location handler
    /** @return @ref True if this connection provides a native file location handler

        @since ConnectionProvider 3.1

        @see getFileLocationHandler()
    */
    bool supportsFileLocations() {
        return supportsFileLocationsImpl();
    }

    #! Returns a native file location handler object for this connection
    /** @return an object inheriting \c FileLocationHandler::AbstractFileLocationHandler

        A connection that reads its configured TLS files remains subject to the active sandbox's filesystem policy.

        @throw FILE-LOCATION-UNSUPPORTED this connection does not provide a native file location handler

        @note The return type is declared as @ref object_type "object" rather than as the concrete handler class
        deliberately.  Declaring the real type would introduce a hard dependency from this module on the
        \c FileLocationHandler module; \c FileLocationHandler is a leaf utility whose only dependency is \c Util and
        which sits below this module in the dependency graph (\c RestClient requires both, and
        @ref ConnectionProvider::HttpConnection "HttpConnection" documents its \c ssl_cert_path and \c ssl_key_path
        options as being resolved through it).  Adding the edge would pull \c DataProvider, \c AsyncSocketIo,
        \c reflection, \c ProviderIndexUtil, and \c Logger into every consumer of a file location, and would turn any
        future reverse dependency into a load cycle.  Do not "correct" this to a concrete type.

        @since ConnectionProvider 3.1

        @see
        - @ref supportsFileLocations()
        - @ref getFileLocationRedirect()
    */
    object getFileLocationHandler() {
        # the common path is one call: only a sandboxed caller is checked
        if (SandboxManager::isRestricted()) {
            checkSandboxAccess(True);
        }
        return getFileLocationHandlerImpl();
    }

    #! Sets or clears the default post-processing code of the objects that every connection creates
    /** The code is called with the connection object and the object created by getImpl() or getAsyncImpl() each time
        get() or getAsync() of any connection returns an object, for every caller.

        @param callback the code to call, or @ref nothing "NOTHING" to clear it; it takes the connection object
        and the object created as arguments

        @throw SANDBOX-REGISTRATION-ERROR the calling code is sandboxed or acts for sandboxed code: the code is
        process-wide and runs for every caller

        @note code set here is used instead of the code of the deprecated @ref post_processing member, which any code
        can read

        @since ConnectionProvider 3.1
    */
    static setPostProcessing(*code callback) [dom=PROCESS_CONTROL] {
        # process-wide: it runs for every caller, of other tenants and of trusted code (qore#6749)
        SandboxManager::checkProcessWideRegistration("set the default post-processing code of connections");
        ConnectionPostProcessing::callback = callback;
    }

    #! Returns the default post-processing code of the objects that every connection creates
    /** @return the code set with setPostProcessing(), else the code of the deprecated @ref post_processing member, or
        @ref nothing "NOTHING" if neither is set or if the calling code is sandboxed or acts for sandboxed code, which
        does not get the host's code (see @ref sandbox_process_wide_registries)

        @since ConnectionProvider 3.1
    */
    static *code getPostProcessing() [dom=PROCESS_CONTROL] {
        # the code is process-wide and configured by the host: not handed to sandboxed code (qore#6749)
        if (!SandboxManager::isProcessWideRegistrationAllowed()) {
            return;
        }
        return ConnectionPostProcessing::callback ?? post_processing;
    }

    #! Resolves a file location path against a connection base path, rejecting any result outside the base
    /** @param conn_name the connection name; used in error messages
        @param base the connection's base path; use \c "/" for no confinement
        @param path the path from the location string, relative to \a base

        @return the resolved absolute path, always strictly below \a base

        A leading slash in \a path does not make it absolute; file location paths are always relative to the
        connection's own base location.  \c ".." segments are resolved before the containment check, so a path that
        would escape the base is rejected rather than followed.

        @throw LOCATION-ERROR empty path; path resolves outside the connection base

        @note The exception message names the offending relative path only, never the resolved absolute path, so
        that error output cannot be used to probe the layout outside the base

        @since ConnectionProvider 3.1
    */
    static string getConfinedFileLocationPath(string conn_name, string base, string path) {
        # a leading slash does not make the path absolute here; it is always relative to the connection base
        path =~ s/^\/+//;
        if (!path.val()) {
            throw "LOCATION-ERROR", sprintf("connection %y: file location is missing a file path", conn_name);
        }

        string base_norm = normalize_dir_unix(base.val() ? base : "/", "/");
        string rv = normalize_dir_unix(base_norm + "/" + path, "/");

        # the result must be strictly below the base; naming the base directory itself is not a file location
        string prefix = base_norm == "/" ? "/" : base_norm + "/";
        if (rv.substr(0, prefix.length()) != prefix) {
            throw "LOCATION-ERROR", sprintf("connection %y: file location path %y resolves outside the "
                "connection's base directory", conn_name, path);
        }
        return rv;
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

        A successful ping marks the connection up (@ref handlePingSuccess()), and a failed ping marks it down
        (@ref handlePingFailed()), also when @ref PF_THROW_EXCEPTION is set and the error is thrown; see @ref up.

        An automatic ping (@ref PF_AUTOMATIC_PING) of a connection whose automatic pings are disabled (the
        \c disable_automatic_pings option) checks nothing: it leaves @ref up, @ref status and @ref last_check as they
        are and returns \c ok @ref True "True" with \c ping_info <tt>{"skipped": True}</tt>

        @since
        - %ConnectionProvider 2.0
        - %ConnectionProvider 3.1 a failed ping marks the connection down also when its error is thrown, and a
          skipped automatic ping does not mark it up
    */
    hash<PingInfo> ping(int ping_flags = 0) {
        # before the ping: a refused caller does not change the status of the connection for its other users
        # the common path is one call: only a sandboxed caller is checked
        if (SandboxManager::isRestricted()) {
            checkSandboxAccess(True);
        }
        hash<PingInfo> rv(self{"name", "desc", "opts"} + {
            "ping_flags": ping_flags,
        });
        # Ping results are diagnostics, including when authentication fails. Apply
        # the same sensitive-option policy as getInfo() before returning either result.
        rv.opts = getSafeOptions(rv.opts);
        rv.url = safe_url;

        # issue #7236: an automatic ping of a connection whose automatic pings are disabled checks nothing, so it
        # leaves the state of the connection (up, status and last_check) as it is
        if ((ping_flags & PF_AUTOMATIC_PING) && disable_automatic_pings) {
            return rv += {
                "ok": True,
                "time": 0s,
                "info": "automatic pings are disabled; the connection was not checked",
                "ping_info": {"skipped": True},
            };
        }

        date start = now_us();
        last_check = start;

        try {
            return pingIntern(rv);
        } catch (hash<ExceptionInfo> ex) {
            # the start of the ping, not last_check, which a connection made by the ping sets as well
            date delta = now_us() - start;
            # issue #7236: a failed ping marks the connection down, also when the error is thrown; a connection made
            # by the ping before it failed has marked it up
            handlePingFailed(delta, ex);
            if (ping_flags & PF_THROW_EXCEPTION) {
                rethrow;
            }
            return rv += {
                "ok": False,
                "time": delta,
                "info": sprintf("%s: %s", ex.err, ex.desc),
                "ex": check_exception_serializable(ex),
            };
        }
    }

    #! Called after a new connection object is created from an existing connection
    /** @param old the old connection object
    */
    persistInfo(AbstractConnection old) {
        # intentionally empty
    }

    #! Starts customization for the object
    /** @param restart restart any customization in progress

        @throw CUSTOMIZATION-UNSUPPORTED this exception is thrown if the connection does not support customization
        @throw CUSTOMIZATION-IN-PROGRESS this exception is thrown if customization is already in progress and
        \a restart is not @ref True
    */
    startCustomization(*bool restart) {
        if ((customization_status == CCS_IN_PROGRESS) && !restart) {
            throw "CUSTOMIZATION-IN-PROGRESS", "Customization in progress; set the restart flag to restart "
                "connection customization";
        }
        startCustomizationImpl();
    }

    #! performs the ping by executing get(); returns a @ref PingInfo hash
    private hash<PingInfo> pingIntern(hash<PingInfo> rv) {
        date start = now_us();
        pingImpl();
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

        @deprecated use getInfo(\c *hash&lt;auto&gt; opts) instead
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
        # issue #7236: up and status are read together
        hash<auto> state;
        {
            AutoLock al(state_lock);
            state = self{"status", "up"};
        }
        hash<ConnectionInfo> h = cast<hash<ConnectionInfo>>(state + self{
            "name", "display_name", "short_desc", "desc", "url", "monitor", "last_check", "updated",
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
        if ((*string app = getAppName()).val()) {
            h.app = app;
        }
        h.opts = orig_opts;
        if (!opts.with_passwords) {
            h.url = safe_url;
            delete h.url_hash.password;
            # remove all sensitive option values as well
            h.opts = getSafeOptions(h.opts);
        }
        # sort options by key
        if (h.opts) {
            h.opts = h.opts{sort(keys h.opts)};
        }
        return h;
    }

    #! Returns the options of the connection without the values of its sensitive options
    /** Used for the information (getInfo() without the \c with_passwords option) and the ping results of the
        connection: the base implementation removes the options that the connection scheme declares sensitive; a
        connection whose options can hold credentials that its scheme does not declare reimplements it, and calls the
        base implementation to remove the declared ones (qore#7017, qore#7070)

        @param opts the options of the connection

        @return the options to show

        @since ConnectionProvider 3.1
    */
    private *hash<auto> getSafeOptions(*hash<auto> opts) {
        map remove opts{$1.key}, getConnectionSchemeInfo().options.pairIterator(), $1.value.sensitive;
        return opts;
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
    /** The default implementation in the base class returns the same value as getInfo(\c *hash&lt;auto&gt;)

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
    /** The default implementation in the base class returns the same value as getInfoAsData(\c *hash&lt;auto&gt;)

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

        With \a connect @ref True "True", a connection that is made marks the connection up (@ref up is set to
        @ref True "True" and @ref status to \c "OK") unless the last ping failed, and one that cannot be made sets
        @ref status to its error without marking the connection down; see @ref up

        @since %ConnectionProvider 3.1 a connection that is made marks the connection up
    */
    object get(bool connect = True, *hash<auto> rtopts) {
        # the common path is one call: only a sandboxed caller is checked
        if (SandboxManager::isRestricted()) {
            checkSandboxAccess(True);
        }
        # the HTTP client that the connection creates for restricted code that was lent the connection is the
        # creator's, bound to its target (qore#7262); a caller that is not restricted takes a proof of its own, so no
        # restricted code that runs while it calls (a callback, for example) can take the connection's
        *HttpClientIo::HttpClientCreatorProofGrant grant;
        if (SandboxManager::isCallerRestricted(DOM_NETWORK)) {
            # a client that the connection's code creates from its members, which the restricted code can change,
            # connects only to the target that the connection's creator gave (qore#7262)
            checkLentClientTarget();
            if (creator_proof) {
                grant = creator_proof.grantNext();
            }
        }
        if (!connect) {
            object o = getImpl(False, rtopts);
            postProcess(o);
            return o;
        }

        on_exit
            last_check = now_us();

        # issue #4852: a connection that cannot be made does not mark the connection down; only a failed ping does

        try {
            object o = getImpl(True, rtopts);
            postProcess(o);
            # issue #7236: a connection made with the connection proves that it is up
            handleConnected();
            return o;
        } catch (hash<ExceptionInfo> ex) {
            setErrorStatus(ex);
            rethrow;
        }
    }

    #! returns an async-I/O-capable connection object
    /** Calls @ref getAsyncImpl() to acquire a connection object that uses non-blocking,
        multiplexed I/O (e.g., HTTP/2, HTTP/3, or HTTP/1.1 via \c HttpClientIo).

        @param connect if @ref True "True" then the connection should be returned connected
        @param rtopts runtime options for connection acquisition

        @return the async-I/O-capable connection object

        @throw CONNECTION-ASYNC-UNSUPPORTED if the connection type does not support async I/O
        (the base class implementation throws this; subclasses override @ref getAsyncImpl()
        to provide the async variant)
        @throw PERMISSION-DENIED the async client is, or creates, an \c HttpClientIo connection manager (the async
        clients of HTTP and REST connections, for example), and the calling code is restricted from the \c NETWORK
        functional domain: the manager is created for the calling code, also with a connection that unrestricted code
        created (see the \c HttpClientIo module's restricted callers section)

        @since ConnectionProvider 2.2

        @see getAsyncImpl()
    */
    object getAsync(bool connect = True, *hash<auto> rtopts) {
        # the common path is one call: only a sandboxed caller is checked
        if (SandboxManager::isRestricted()) {
            checkSandboxAccess(True);
        }
        # the HTTP client that the connection creates for restricted code that was lent the connection is the
        # creator's, bound to its target (qore#7262); a caller that is not restricted takes a proof of its own, so no
        # restricted code that runs while it calls (a callback, for example) can take the connection's
        *HttpClientIo::HttpClientCreatorProofGrant grant =
            creator_proof && SandboxManager::isCallerRestricted(DOM_NETWORK) ? creator_proof.grantNext() : NOTHING;
        if (!connect) {
            object o = getAsyncImpl(False, rtopts);
            postProcess(o);
            return o;
        }

        try {
            object o = getAsyncImpl(True, rtopts);
            postProcess(o);
            # issue #7236: an async object does not prove that the connection is up, as async objects connect on
            # demand (getAsyncImpl() of the HTTP, REST and FTP connections ignores "connect"), so it is no check of
            # the connection and changes neither its state nor last_check
            return o;
        } catch (hash<ExceptionInfo> ex) {
            last_check = now_us();
            setErrorStatus(ex);
            rethrow;
        }
    }

    #! Calls the default post-processing code, if any, with the object created (see setPostProcessing())
    private:internal postProcess(object o) {
        # read here, as the code that the host set is used for every caller
        *code pp = ConnectionPostProcessing::callback ?? post_processing;
        if (pp) {
            pp(self, o);
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

    #! returns @ref True if the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True if the connection returns a data provider with the \c getDataProvider() method

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
        - \c startPollConnect()

        @since ConnectionProvider 1.7
    */
    bool supportsPollingApi() {
        return False;
    }

    #! Called to start the connection polling operation
    /** Only called if the connection supports the polling API

        @param logger an optional logger interface for any object used for polling

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
        # the client is handed to the caller as get()'s is: it is checked for code restricted from NETWORK (qore#7262)
        if (SandboxManager::isCallerRestricted(DOM_NETWORK)) {
            checkLentClientTarget();
        }
        return getImpl(False);
    }

    #! Returns @ref True if the connection supports the async request API
    /** This base class method returns @ref False; to support the async request API, override this method in child
        classes

        @see
        - \c startPollRequest()

        @since ConnectionProvider 3.0
    */
    bool supportsAsyncRequestApi() {
        return False;
    }

    #! Starts a non-blocking HTTP request on the connection
    /** Only called if the connection supports the async request API

        @param method the HTTP method (GET, POST, PUT, DELETE, PATCH, etc.)
        @param path the request path
        @param body the request body
        @param headers optional request headers
        @param logger an optional logger interface

        @return a poll operation for the async request

        @see supportsAsyncRequestApi()

        @since ConnectionProvider 3.0
    */
    Qore::AbstractPollOperation startPollRequest(string method, string path, auto body,
            *hash<auto> headers, *Logger::LoggerInterface logger) {
        throw "UNSUPPORTED", sprintf("connection %y (%s) does not support the async request API",
            name, safe_url);
    }

    #! Handles a successful ping operation on the connection
    /** Marks the connection up: sets @ref up to @ref True "True" and @ref status to \c "OK"

        @param delta the duration of the ping
    */
    handlePingSuccess(date delta) {
        setState(True, "OK", False);
    }

    #! Handles a failed ping operation on the connection
    /** Marks the connection down: sets @ref up to @ref False "False" and @ref status to the error

        @param delta the duration of the ping
        @param ex the exception that the ping raised
    */
    handlePingFailed(date delta, hash<ExceptionInfo> ex) {
        setState(False, sprintf("%s: %s", ex.err, ex.desc), True);
    }

    #! Handles a failed ping operation on the connection
    /** Marks the connection down: sets @ref up to @ref False "False" and @ref status to \a err

        @param delta the duration of the ping
        @param err the error string
    */
    handlePingFailed(date delta, string err) {
        setState(False, err, True);
    }

    #! Returns a ConnectionOptionInfo hash from a DataProviderOptionInfo hash
    static hash<ConnectionOptionInfo> getConnectionOptionInfo(hash<DataProviderOptionInfo> dinfo) {
        string type_str = dinfo.type[0].getBaseTypeName();
        type_str =~ s/<.*>//;
        return <ConnectionOptionInfo>{
            "display_name": dinfo.display_name,
            "short_desc": dinfo.short_desc,
            "desc": dinfo.desc,
            "sensitive": dinfo.sensitive,
            "allowed_values": dinfo.allowed_values,
            "allowed_values_creatable": dinfo.allowed_values_creatable,
            "default_value": dinfo.default_value,
            "example_value": dinfo.example_value,
            "required": dinfo.required,
            "readonly": dinfo.readonly,
            "disabled": dinfo.disabled,
            "preselected": dinfo.preselected,
            "has_dependents": dinfo.has_dependents,
            "depends_on": dinfo.depends_on,
            "on_change": dinfo.on_change,
            "type": type_str,
        };
    }

    #! Returns a hash of ConnectionOptionInfo hashes from a hash of DataProviderOptionInfo hashes
    static hash<string, hash<ConnectionOptionInfo>> getConnectionOptionInfo(
                hash<string, hash<DataProviderOptionInfo>> dinfo) {
        hash<string, hash<ConnectionOptionInfo>> rv;
        map rv{$1.key} = AbstractConnection::getConnectionOptionInfo($1.value),
            dinfo.pairIterator();
        return rv;
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

    #! Called by get() for code restricted from the NETWORK domain before the connection's client is created
    /** A connection class whose get() creates a client that connects outside the connection manager of HttpClientIo
        (a synchronous client) checks with checkLentNetworkTarget() the target and the proxy that the client is
        created for from the connection's members, which restricted code that was lent the connection can change;
        this implementation checks nothing (qore#7262)

        @throw PERMISSION-DENIED the target or the proxy is not the one that the connection's creator gave

        @since %ConnectionProvider 3.1
    */
    private checkLentClientTarget() {
    }

    #! Refuses code restricted from the NETWORK domain a network connection that the connection's creator proof does not allow
    /** A connection class calls this method before it connects a client that it created outside @ref get() and
        @ref getAsync() for the calling code (a synchronous client of a ping, for example), giving the target that the
        client connects to.  Code that is not restricted from the \c NETWORK domain is not checked.  Restricted code
        that was lent the connection may connect only to the connection's own target, as with the clients that
        @ref get() and @ref getAsync() create for it (qore#7262): the origin of \a url (TLS or not, host, port) and the
        proxy must be the ones that the proof that the connection took from the code that created it is bound to; a
        connection without a proof, or another target, is refused before any connection is attempted.

        @param url the URL that the client connects to (an \c http or \c https URL)
        @param proxy_url the URL of the client's proxy, if any

        @throw PERMISSION-DENIED the calling code is restricted from the \c NETWORK domain, and the connection's creator
        proof does not allow the target or the proxy

        @since %ConnectionProvider 3.1
    */
    final private checkLentNetworkTarget(string url, *string proxy_url) {
        if (!SandboxManager::isCallerRestricted(DOM_NETWORK)) {
            return;
        }
        # the origin (TLS or not, canonical host, port; the schemes that are mapped to HTTP, such as ws and rest, as
        # their clients map them) and the proxy are derived as for the proof itself; an invalid URL allows nothing
        if (!creator_proof || !creator_proof.allowsUrl(url, proxy_url)) {
            throw "PERMISSION-DENIED", sprintf("connection %y: the calling code is restricted from the NETWORK "
                "functional domain and may connect only to the target of a connection that unrestricted code created "
                "and lent to it", name);
        }
    }

    #! Marks the connection up after a connection was made successfully with get(), unless a failed ping marked it down
    /** Sets @ref up to @ref True "True" and @ref status to \c "OK", as a successful ping does, but without calling
        @ref handlePingSuccess(), which handles pings only.  A connection that the last ping found down is left down:
        the transition to up is the business of the pings, which a monitor handles (only a ping changes the status of
        a connection that was checked by a ping, as for issue #4852)

        @since %ConnectionProvider 3.1
    */
    private:internal handleConnected() {
        AutoLock al(state_lock);
        if (!down_by_ping) {
            up = True;
            status = "OK";
        }
    }

    #! Sets the state of the connection atomically
    /** @param up the new value of @ref up
        @param status the new value of @ref status
        @param down_by_ping if the state is set by a failed ping

        @since %ConnectionProvider 3.1
    */
    private:internal setState(bool up, string status, bool down_by_ping) {
        AutoLock al(state_lock);
        self.up = up;
        self.status = status;
        self.down_by_ping = down_by_ping;
    }

    #! Sets the status of the connection after an error, without changing @ref up
    private:internal setErrorStatus(hash<ExceptionInfo> ex) {
        AutoLock al(state_lock);
        status = sprintf("%s: %s", ex.err, ex.desc);
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
        @param subtype_options subtype option descriptions
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
    # returns hash<auto>: a connection with another URL scheme returns its own hash (DatasourceConnection returns the
    # hash of parse_datasource()), which is stored in urlh (issue #5838)
    private hash<auto> parseUrl(string url) {
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
        # code restricted from the domains of the connection cannot create it (qore#7052)
        checkCreation(config.name);
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
                "characters", get_masked_url(url));
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
        # the proof of the code that creates the connection, bound to the URL that its clients use (its real URL: the
        # HTTP URL of an HTTP-based connection, whatever its scheme) and its proxy (qore#7262)
        creator_proof = HttpClientIo::HttpClientCreatorProof::tryGet(real_url ?? url, {"proxy": opts.proxy});
    }

    #! Called by the constructor to set the real URL
    /** @param url the new URL

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

    #! Returns a redirect to an existing file location scheme for the given path, if supported
    /** The default implementation returns no value; override in child classes that map onto a scheme already
        supported by the \c FileLocationHandler module

        @param path the path relative to the connection's own base location
        @param write @ref True if the location will be written to, @ref False if it will be read

        @return @ref nothing in the base class

        @since ConnectionProvider 3.1
    */
    private *hash<FileLocationRedirect> getFileLocationRedirectImpl(string path, bool write) {
    }

    #! Returns @ref True if this connection provides a native file location handler
    /** The default implementation returns @ref False

        @return @ref False in the base class

        @since ConnectionProvider 3.1
    */
    private bool supportsFileLocationsImpl() {
        return False;
    }

    #! Returns a native file location handler object for this connection
    /** The default implementation throws an exception; override in child classes that provide a native handler

        @throw FILE-LOCATION-UNSUPPORTED this connection does not provide a native file location handler

        @since ConnectionProvider 3.1
    */
    private object getFileLocationHandlerImpl() {
        throw "FILE-LOCATION-UNSUPPORTED", sprintf("connection %y (type %y) does not provide a native file "
            "location handler", name, getType());
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

    #! Returns the functional domains that the connection's clients use (qore#6981)
    /** Code restricted from any of them cannot create the connection; the connections to a network host return
        @ref Qore::DOM_NETWORK "DOM_NETWORK", those of the file system @ref Qore::DOM_FILESYSTEM "DOM_FILESYSTEM",
        those of a database @ref Qore::DOM_DATABASE "DOM_DATABASE", and a connection whose clients use no functional
        domain returns 0.

        The default implementation returns all of the domains that a connection's clients can use (\c NETWORK,
        \c FILESYSTEM and \c DATABASE), so code restricted from any of them cannot create a connection of a class
        that does not declare its domains; unrestricted code is not affected.  Every connection class should override
        this method, declared \c final, to return the domains that its clients use.

        @since %ConnectionProvider 3.1
    */
    private int getConnectionDomainsImpl() {
        return DOM_NETWORK | DOM_FILESYSTEM | DOM_DATABASE;
    }

    #! Refuses a connection that code restricted from the connection's domains creates or deserializes (qore#7052)
    /** A connection is a capability of the code that creates it: code restricted from the domains that the
        connection's clients use cannot create one, by any constructor, also of a subclass, nor by deserialization,
        so no client of it can be obtained; a connection that unrestricted code created and lends is not refused
    */
    private:internal checkCreation(auto conn_name) {
        int dom = creator_restrictions & getConnectionDomainsImpl();
        if (dom) {
            throw "PERMISSION-DENIED", sprintf("code restricted from the %s functional domain cannot create connection "
                "%y; use a connection that the host provides", AbstractConnection::getDomainNames(dom), conn_name);
        }
    }

    #! Assigns the members of the class after deserialization (qore#7052)
    /** Deserialization creates a connection too, so it is refused to code restricted from the connection's domains;
        the transient members are assigned here, as no automatic member initialization is performed when this method
        is defined
    */
    private:internal deserializeMembers(hash<auto> members) {
        transient_info = {};
        sandbox_check = new DataProvider::DataProviderSandboxCheck();
        state_lock = new Mutex();
        creator_restrictions = AbstractConnection::getCreatorRestrictions();
        creator_proof = HttpClientIo::HttpClientCreatorProof::tryGet(members.real_url ?? members.url,
            {"proxy": members.opts.proxy});
        checkCreation(members.name);
        # the transient and internal members are the deserializing process's own, never the serialized data's
        self += members - TransientMembers;
        # issue #7236: a connection that was never checked is not up; a connection serialized before
        # ConnectionProvider 3.1 has up True with the status "not checked"
        if (status == "not checked") {
            up = False;
        }
    }

    #! The members that deserializeMembers() assigns itself
    const TransientMembers = ("transient_info", "sandbox_check", "state_lock", "creator_restrictions",
        "creator_proof");

    #! The functional domains that a connection's clients can use (see getConnectionDomainsImpl())
    const ConnectionDomains = (DOM_NETWORK, DOM_FILESYSTEM, DOM_DATABASE);

    #! Returns the names of the given connection domains, for messages
    private:internal static string getDomainNames(int dom) {
        list<string> rv = ();
        if (dom & DOM_NETWORK) {
            push rv, "NETWORK";
        }
        if (dom & DOM_FILESYSTEM) {
            push rv, "FILESYSTEM";
        }
        if (dom & DOM_DATABASE) {
            push rv, "DATABASE";
        }
        return rv.join(" and ");
    }

    #! Returns the functional domains of a connection that the calling code is restricted from (qore#6981)
    private:internal static int getCreatorRestrictions() {
        int rv = 0;
        foreach int dom in (ConnectionDomains) {
            if (SandboxManager::isCallerRestricted(dom)) {
                rv |= dom;
            }
        }
        return rv;
    }

    #! returns an async-I/O-capable connection object
    /** The default implementation throws \c CONNECTION-ASYNC-UNSUPPORTED.
        Subclasses that support async I/O (e.g., REST connections) override this
        method to return an async-capable variant of the connection object.

        @param connect if @ref True "True" then the connection should be returned connected
        @param rtopts runtime options for connection acquisition

        @return the async-I/O-capable connection object

        @throw CONNECTION-ASYNC-UNSUPPORTED if the connection type does not support async I/O

        @since ConnectionProvider 2.2

        @see getAsync()
    */
    private object getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        throw "CONNECTION-ASYNC-UNSUPPORTED",
            sprintf("connection type %y does not support async I/O", getType());
    }
}

#! The default post-processing code of connections (not public: only the module's own code uses it)
/** Set by trusted code with AbstractConnection::setPostProcessing(); AbstractConnection::getPostProcessing() does not
    hand it to sandboxed code (qore#6749)
*/
class ConnectionPostProcessing {
    public {
        #! the code, if any
        static *code callback;
    }
}
} # ConnectionProvider namespace
