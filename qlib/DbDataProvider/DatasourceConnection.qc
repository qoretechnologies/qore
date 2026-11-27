# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatasourceConnection class definition

/** DatasourceConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DbDataProvider {
#! Datasource connection class providing database connectivity using the @c db:// URL scheme
/** Provides connection support for databases using the @c db:// URL scheme.

    The URL format is:
    @code
    db://driver:user/pass\@db(charset)%host:port{opt=val,...}
    @endcode

    @since %DbDataProvider 2.3
*/
public class DatasourceConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    #! The connection's clients open a database (qore#7052); the class also checks its creator itself
    private final int getConnectionDomainsImpl() {
        return DOM_DATABASE;
    }

    public {
        #! Connection entry info
        const ConnectionScheme = ConnectionProvider::connection_scheme_presentation("DbDataProvider",
            "DatasourceConnection", "en", <ConnectionSchemeInfo>{
            "display_name": "Database Connection",
            "short_desc": "Connection to a database server",
            "desc": "Connection to a database server using a datasource connection string",
            "cls": Class::forName("DbDataProvider::DatasourceConnection"),
            "schemes": {
                "db": True,
            },
            "base_scheme_map": {
                "tcp": "db",
            },
            "options": ConnectionOptions,
        });

        #! Connection options
        const ConnectionOptions = {
            "min": <ConnectionOptionInfo>{
                "display_name": "Min Pool Size",
                "short_desc": "Minimum pool connections",
                "type": "int",
                "desc": "Minimum number of connections in the pool",
            },
            "max": <ConnectionOptionInfo>{
                "display_name": "Max Pool Size",
                "short_desc": "Maximum pool connections",
                "type": "int",
                "desc": "Maximum number of connections in the pool",
            },
        };
    }

    private {
        #! The parsed datasource URL hash (without the "db://" prefix)
        hash<auto> ds_urlh;
    }

    #! Creates the connection object from a URL
    /** @param name the name of the connection
        @param desc a description of the connection
        @param url the URL to connect to (must start with @c "db://")
        @param attributes connection attributes
        @param options connection options
        @throw PERMISSION-DENIED the calling code is restricted from the \c DATABASE domain
        (@ref Qore::PO_NO_DATABASE "PO_NO_DATABASE"); it uses the connections that its host creates, unless a sandbox
        that restricts the domain governs it (since DbDataProvider 2.5)
    */
    constructor(string name, string desc, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : AbstractConnectionWithInfo(name, desc, url, attributes, options) {
        checkCreator();
        commonSetup();
    }

    #! Creates the connection object from a hash
    /** @param config a hash with connection configuration
        @param attr optional connection attributes
        @throw PERMISSION-DENIED the calling code is restricted from the \c DATABASE domain
        (@ref Qore::PO_NO_DATABASE "PO_NO_DATABASE"); it uses the connections that its host creates, unless a sandbox
        that restricts the domain governs it (since DbDataProvider 2.5)
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
        checkCreator();
        commonSetup();
    }

    #! Returns the connection type
    string getType() {
        return "db";
    }

    #! Returns @ref True as the connection supports the data provider API
    bool hasDataProvider() {
        return True;
    }

    #! Returns a data provider for this connection
    /** @param constructor_options optional constructor options for the data provider
        @return a DbDataProvider object
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        # the data provider is created from the connection's members and its constructor options: for restricted code
        # that was lent the connection, from its creator's members, and with options that cannot change the target
        # (qore#7327)
        getLentDataProviderSource(constructor_options);
        return new DbDataProvider(cast<Datasource>(get()));
    }

    #! Returns the data provider application name
    *string getAppName() {
        return AppName;
    }

    #! Returns the parsed datasource URL hash
    /** @return the parsed datasource URL hash without the @c "db://" prefix
    */
    hash<auto> getDatasourceHash() {
        return ds_urlh;
    }

    #! Returns logo info for the connection's database driver
    /** @return a hash with \c logo, \c logo_file_name, and \c logo_mime_type keys
    */
    hash<auto> getLogoInfo() {
        return getDriverLogo(ds_urlh.type);
    }

    #! Returns connection info
    /** Overridden so that without the \c with_passwords option, @c url_hash has no credentials, as the safe URL: the
        password is removed, the user name is masked as @c "****", and the value of each driver option is masked as
        @c "****" unless all of the conditions of @ref datasource_mask_options hold: the driver is loaded, registers
        the option with option flags without declaring it sensitive, the option name names no credential, and the
        value is a plain string, number or boolean that a datasource string can show (qore#7017, qore#7070); the
        database name is shown under the rule of @ref datasource_mask_options, which masks the database name of a
        loaded driver that declares @ref Qore::SQL::DBI_CAP_SENSITIVE_DBNAME "DBI_CAP_SENSITIVE_DBNAME" (qore#7275)
    */
    hash<ConnectionInfo> getInfo(*hash<auto> opts) {
        hash<ConnectionInfo> h = AbstractConnectionWithInfo::getInfo(opts);
        if (!opts.with_passwords) {
            h.url_hash = mask_datasource_hash(urlh, {"show_db": True, "show_options": True}) + {"protocol": "db"};
        }
        return h;
    }

    #! Returns the options of the connection without the values that can hold credentials
    /** The options of the connection scheme (the pool sizes) are shown; the value of every other option, a driver
        option that the URL or the options give, is masked as @c "****" unless all of the conditions of
        @ref datasource_mask_options hold: the driver is loaded, registers the option with option flags without
        declaring it sensitive, the option name names no credential, and the value is a plain string, number or
        boolean that a datasource string can show (see
        @ref Qore::SQL::mask_datasource_options() "mask_datasource_options()"), as an option of a driver can hold a
        credential (qore#7017, qore#7070)
    */
    private *hash<auto> getSafeOptions(*hash<auto> opts) {
        opts = AbstractConnectionWithInfo::getSafeOptions(opts);
        *hash<auto> driver_opts = opts - keys ConnectionScheme.options;
        return driver_opts ? opts + mask_datasource_options(ds_urlh.type ?? urlh.type ?? "", driver_opts) : opts;
    }

    #! Returns the ConnectionSchemeInfo hash
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }

    #! Returns the connection object
    /** @param connect if True, opens the datasource connection
        @param rtopts runtime options
        @return a Datasource object
    */
    private Datasource getImpl(bool connect = True, *hash<auto> rtopts) {
        Datasource ds(ds_urlh);
        # the datasource is identified by the name of the connection, which serialized data keeps instead of its
        # credentials (qore#7016)
        ds.setConnectionName(name);
        if (connect) {
            ds.open();
        }
        return ds;
    }

    #! Performs the internal ping
    private pingImpl() {
        # for restricted code that was lent the connection, the client is created from a private copy with its
        # creator's target (qore#7327)
        Datasource ds = cast<DatasourceConnection>(getLentTargetSource()).getImpl();
        ds.getServerVersion();
    }

    #! Parses the URL to a hash
    /** @param url the URL to parse (with @c "db://" prefix)
        @return a hash with the datasource components plus @c protocol: @c "db"
    */
    private hash<auto> parseUrl(string url) {
        # strip the "db://" prefix
        string ds_str = url;
        if (ds_str =~ /^db:\/\//) {
            ds_str = ds_str.substr(5);
        }
        if (!ds_str.val()) {
            throw "DATASOURCE-PARSE-ERROR", "empty datasource string after 'db://' prefix";
        }
        hash<auto> urlh = parse_datasource(datasource: ds_str);
        # a datasource needs a driver; the error is the one that the safe URL raised when it was made with
        # make_datasource_string() (the safe URL is now masked and has no error of its own; qore#7017)
        if (!urlh.type.val()) {
            throw "MAKE-DATASOURCE-STRING-ERROR", "the datasource in the 'db://' URL has no driver";
        }
        urlh.protocol = "db";
        return urlh;
    }

    #! Creates a "safe" URL string with no credentials
    /** The URL is shown in user interfaces and logs: it shows the driver, the database, the character encoding, the
        host and the port, and the names of the options; the user name and the password are masked as @c "****", and
        the value of each option is masked as @c "****" unless all of the conditions of @ref datasource_mask_options
        hold: the driver is loaded, registers the option with option flags without declaring it sensitive, the option
        name names no credential, and the value is a plain string, number or boolean that a datasource string can
        show, as an option of a driver can hold a credential (see
        @ref Qore::SQL::mask_datasource(hash<auto>, *hash<auto>) "mask_datasource()"; qore#7017, qore#7070)

        @param urlh a parsed URL hash (as returned from parseUrl())

        @return a safe URL string with the @c "db://" prefix and no credentials

        @note the \c safe_url member is made when the connection is created: if the driver is not loaded then, the
        value of each driver option stays masked in it, also after the driver has been loaded, and its database name is
        shown or masked by the character rule of @ref datasource_mask_options alone, as the driver's
        @ref Qore::SQL::DBI_CAP_SENSITIVE_DBNAME "DBI_CAP_SENSITIVE_DBNAME" is not known then; the safe URL that the
        connection shows (getSafeUrl(), the \c url of getInfo(), getInfoAsData(), getExtendedInfo() and ping()) is made
        when it is shown, so that it follows the drivers loaded then (see getShownUrl(); qore#7275)
    */
    private string getSafeUrl(hash<auto> urlh) {
        return "db://" + mask_datasource(urlh, {"show_db": True, "show_options": True});
    }

    #! Returns the safe URL that the connection shows, made when it is shown
    /** The safe URL depends on the drivers that are loaded when it is made (the values of the driver options that a
        loaded driver declares not sensitive, and the database name of a loaded driver that declares
        @ref Qore::SQL::DBI_CAP_SENSITIVE_DBNAME "DBI_CAP_SENSITIVE_DBNAME"), so it is made each time that it is shown,
        and the \c url of getInfo() agrees with its \c url_hash (qore#7275)

        @since DbDataProvider 2.5
    */
    private string getShownUrl() {
        return getSafeUrl(urlh);
    }

    #! Validates connection options
    /** Returns options as-is since DB connection options are free-form and driver-dependent
        @param options the options to validate
        @param check_required whether to check for required options (ignored)
        @return the validated options
    */
    private hash<auto> validateOptions(hash<auto> options, bool check_required = True) {
        return options;
    }

    #! Refuses code restricted from the DATABASE domain as the code that gives the connection its URL
    /** The connection opens a datasource from its URL for whoever uses it, so the code that creates it, and so gives
        the URL, decides; code restricted from the \c DATABASE domain uses the connections that its host creates
        (qore#6385).  It is \c private:internal, so a subclass cannot replace it, and it is called by the
        constructors and by the deserialization, which gives the URL as well.

        @throw PERMISSION-DENIED the calling code is restricted from the \c DATABASE domain
    */
    private:internal checkCreator() {
        if (SandboxManager::isCallerRestricted(DOM_DATABASE)) {
            throw "PERMISSION-DENIED", "code that is restricted from the DATABASE domain cannot create a database "
                "connection from a URL, a configuration or serialized data; use a connection that the host provides";
        }
    }

    #! Assigns the members of the class after deserialization: serialized data gives the connection its URL as well
    private:internal deserializeMembers(hash<auto> members) {
        checkCreator();
        self += members;
    }

    #! Common setup logic called from both constructors
    private commonSetup() {
        children_can_support_transactions = True;
        children_can_support_records = True;

        # the caller's options may be a narrowed literal (e.g. hash<string, string> for {"min": "1", "max": "4"}),
        # and adding URL options or converting the pool sizes to integers below would throw, so the options are
        # processed in an "auto!" local and stored afterwards
        *hash<auto!> o = opts;

        # unpack driver options into top-level options
        if (o.driver.typeCode() == NT_HASH) {
            hash<auto!> driver_opts = remove o.driver;
            driver_opts += o;
            o = driver_opts;
        }

        # move URL options to opts
        if (urlh.options) {
            o += urlh.options;
        }

        # validate min/max pool options: both must be present or both are stripped
        if (exists o.min || exists o.max) {
            if (!exists o.min || !exists o.max) {
                # silently strip incomplete min/max pair
                o -= ("min", "max");
            } else {
                # ensure it's always integer for safe comparisons
                o.min = int(o.min);
                o.max = int(o.max);

                if (o.min < 0) {
                    throw "DATASOURCE-CONNECTION-ERROR", sprintf("connection %y: 'min' pool option must be >= 0; "
                        "got: %y", name, o.min);
                }
                if (o.max < 1) {
                    throw "DATASOURCE-CONNECTION-ERROR", sprintf("connection %y: 'max' pool option must be > 0; "
                        "got: %y", name, o.max);
                }
                if (o.max < o.min) {
                    throw "DATASOURCE-CONNECTION-ERROR", sprintf("connection %y: 'max' (%d) must be >= 'min' (%d)",
                        name, o.max, o.min);
                }
            }
        }

        opts = o;
        orig_opts = opts;

        # build the datasource URL hash (without protocol)
        ds_urlh = urlh - "protocol";
        *hash<auto> extra_opts = opts - ("min", "max");
        if (extra_opts) {
            ds_urlh = ds_urlh + {"options": (ds_urlh.options ?? {}) + extra_opts};
        }
        # the options are normalized after the base class took its snapshot of them (qore#7327)
        takeLentTargetSnapshot();
    }
}
}
