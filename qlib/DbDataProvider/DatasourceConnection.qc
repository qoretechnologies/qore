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

%modern

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
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
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
        };

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

    private:internal {
        #! The parsed datasource URL hash (without the "db://" prefix)
        hash<auto> ds_urlh;
    }

    #! Creates the connection object from a URL
    /** @param name the name of the connection
        @param desc a description of the connection
        @param url the URL to connect to (must start with @c "db://")
        @param attributes connection attributes
        @param options connection options
    */
    constructor(string name, string desc, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : AbstractConnectionWithInfo(name, desc, url, attributes, options) {
        commonSetup();
    }

    #! Creates the connection object from a hash
    /** @param config a hash with connection configuration
        @param attr optional connection attributes
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
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

    #! Returns logo info for the connection's database driver
    /** @return a hash with \c logo, \c logo_file_name, and \c logo_mime_type keys
    */
    hash<auto> getLogoInfo() {
        return getDriverLogo(ds_urlh.type);
    }

    #! Returns the connection type
    string getConnectionType() {
        return "db";
    }

    #! Returns connection info
    /** Overridden to strip the @c pass key from @c url_hash (datasource URLs use @c pass instead of @c password)
    */
    hash<ConnectionInfo> getInfo(*hash<auto> opts) {
        hash<ConnectionInfo> h = AbstractConnectionWithInfo::getInfo(opts);
        if (!opts.with_passwords) {
            delete h.url_hash.pass;
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
        hash<auto> urlh = parse_datasource(ds_str);
        urlh.protocol = "db";
        return urlh;
    }

    #! Creates a "safe" URL string with password information removed
    /** @param urlh a parsed URL hash (as returned from parseUrl())
        @return a safe URL string with the @c "db://" prefix and no password
    */
    private string getSafeUrl(hash<auto> urlh) {
        return "db://" + make_datasource_string(urlh - "pass");
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

    #! Common setup logic called from both constructors
    private commonSetup() {
        children_can_support_transactions = True;
        children_can_support_records = True;

        # unpack driver options into top-level options
        if (opts.driver.typeCode() == NT_HASH) {
            # build a new hash to avoid type narrowing issues
            hash<auto> driver_opts = remove opts.driver;
            driver_opts += opts;
            opts = driver_opts;
        }

        # move URL options to opts
        if (urlh.options) {
            opts += urlh.options;
        }

        # validate min/max pool options
        if (exists opts.min || exists opts.max) {
            if (!exists opts.min || !exists opts.max) {
                throw "CONNECTION-OPTION-ERROR", sprintf("connection %y: both 'min' and 'max' pool options must "
                    "be provided together; got min: %y, max: %y", name, opts.min, opts.max);
            }
            if (opts.min < 0) {
                throw "CONNECTION-OPTION-ERROR", sprintf("connection %y: 'min' pool option must be >= 0; "
                    "got: %y", name, opts.min);
            }
            if (opts.max < 1) {
                throw "CONNECTION-OPTION-ERROR", sprintf("connection %y: 'max' pool option must be > 0; "
                    "got: %y", name, opts.max);
            }
            if (opts.max < opts.min) {
                throw "CONNECTION-OPTION-ERROR", sprintf("connection %y: 'max' (%d) must be >= 'min' (%d)",
                    name, opts.max, opts.min);
            }
        }

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
