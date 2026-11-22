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
    /** Overridden to strip the @c pass key from @c url_hash (datasource URLs use @c pass instead of @c password), to
        mask the values of the driver options in @c url_hash that cannot be shown (see
        @ref Qore::SQL::mask_datasource_options() "mask_datasource_options()"), and to mask the database name in
        @c url_hash if it can hold credentials or the driver is not loaded (see
        @ref Qore::SQL::mask_datasource_dbname() "mask_datasource_dbname()"); since DbDataProvider 2.5
    */
    hash<ConnectionInfo> getInfo(*hash<auto> opts) {
        hash<ConnectionInfo> h = AbstractConnectionWithInfo::getInfo(opts);
        if (!opts.with_passwords) {
            delete h.url_hash.pass;
            if (h.url_hash.options) {
                h.url_hash.options = maskDriverOptions(h.url_hash.type, h.url_hash.options);
            }
            if (exists h.url_hash.db) {
                h.url_hash.db = mask_datasource_dbname(h.url_hash.type ?? "", h.url_hash.db);
            }
        }
        return h;
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
        if (connect) {
            ds.open();
        }
        return ds;
    }

    #! Performs the internal ping
    private pingImpl() {
        Datasource ds = getImpl();
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
        urlh.protocol = "db";
        return urlh;
    }

    #! Creates a "safe" URL string with password information removed
    /** @param urlh a parsed URL hash (as returned from parseUrl())
        @return a safe URL string with the @c "db://" prefix and no password; the value of each driver option is
        masked as \c "****" unless the driver is loaded and declares the option without declaring it sensitive (see
        @ref Qore::SQL::mask_datasource_options() "mask_datasource_options()"), and the database name is masked as
        \c "****" unless the driver is loaded and does not declare that its database name can hold credentials (see
        @ref Qore::SQL::mask_datasource_dbname() "mask_datasource_dbname()"); since DbDataProvider 2.5

        @note the safe URL is made when the connection is created: if the driver is not loaded then, the database name
        and the value of each driver option stay masked in it, also after the driver has been loaded
    */
    private string getSafeUrl(hash<auto> urlh) {
        # the values of the driver options that can hold a credential are masked (qore#7070)
        if (urlh.options) {
            urlh.options = maskDriverOptions(urlh.type, urlh.options);
        }
        # and so is a database name that can hold credentials (qore#7275)
        if (exists urlh.db) {
            urlh.db = mask_datasource_dbname(urlh.type ?? "", urlh.db);
        }
        return "db://" + make_datasource_string(urlh - "pass");
    }

    #! Returns the options of the connection that can be shown, with the values of the driver options masked
    /** The pool options \c min and \c max are shown; the value of each driver option is masked as \c "****" unless
        the driver is loaded and declares the option without declaring it sensitive (see
        @ref Qore::SQL::mask_datasource_options() "mask_datasource_options()")

        @since DbDataProvider 2.5
    */
    private *hash<auto> getSafeOptions(*hash<auto> opts) {
        opts = AbstractConnectionWithInfo::getSafeOptions(opts);
        return opts ? maskDriverOptions(ds_urlh.type ?? urlh.type, opts) : opts;
    }

    #! Returns the options with the value of each driver option that cannot be shown masked; the pool options are kept
    private:internal *hash<auto> maskDriverOptions(*string type, *hash<auto> o) {
        *hash<auto> driver_opts = o - ("min", "max");
        return driver_opts ? o + mask_datasource_options(type ?? "", driver_opts) : o;
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
    }
}
}
