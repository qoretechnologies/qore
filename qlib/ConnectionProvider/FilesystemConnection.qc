# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore FilesystemConnection class definition

/*  FilesystemConnection.qc Copyright 2016 - 2024 Qore Technologies, s.r.o.

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

#! the ConnectionProvider namespace. All classes used in the ConnectionProvider module should be inside this namespace
public namespace ConnectionProvider {
#! class for filesystem connections; returns an object of type @ref Qore::Dir
/** this can be used to monitor filesystem free space or to ensure a network filesystem is mounted, for example

    supports the following option:
    - \c "readonly": does not mark the connection as down if the directory is read-only; use for example if the
      connection should point to a read-only base directory; interface directories should normally be writable

    @note the path will be subject to environment variable substitutions
*/
public class FilesystemConnection inherits AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Filesystem Connection",
            "short_desc": "A connection to the local filesystem",
            "desc": "A connection to the local filesystem",
            "cls": Class::forName("FilesystemConnection"),
            "schemes": {
                "file": True,
                "dir": True,
            },
            "options": {
                "readonly": <ConnectionOptionInfo>{
                    "display_name": "Read-Only",
                    "short_desc": "Sets the read-only flag on the connection",
                    "type": "bool",
                    "desc": "Does not mark the connection as down if the filesystem location is read only; use for "
                        "example if the connection should point to a read-only base directory; interface directories "
                        "should normally be writable",
                    "default_value": False,
                },
            },
        };

        #! Data provider subtypes supported
        const DataProviderSubtypeMap = {
            "filepoller": {
                "module": "FilePoller",
                "class": "FileDelayedPollerDataProvider",
                "option_class": "FilePollerDataProviderBase",
                "options": "ConstructorOptions",
            },
        };
    }

    private {
        #! Data provider subtype info
        static hash<string, *hash<string, hash<DataProviderOptionInfo>>> subtype_info;

        #! Data provider subtype lock
        static Mutex subtype_lck();
    }

    #! creates the FilesystemConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options
        @param logger optional logger interface for the connection

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {}, hash<auto> options = {},
            *Logger logger)
            : AbstractConnectionWithInfo(name, description, url, attributes, options, logger) {
        setupFsOpts();
    }

    #! creates the FilesystemConnection object
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
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
        setupFsOpts();
    }

    #! returns \c "file"
    string getType() {
        return "file";
    }

    #! returns @ref True
    /** @return @ref True
    */
    bool hasDataProvider() {
        return True;
    }

    #! Returns a data provider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return a data provider object for this connection
    */
    AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        load_module("FileDataProvider");
        return create_object("FileDataProvider", {
            "root": urlh.path ?? "/",
            "readonly": False,
        } + constructor_options);
    }

    #! Common constructor initialization
    private setupFsOpts() {
        # the URL hash is parsed with parse_url() which will put a string in the
        # "host" key if it does not look like an absolute path, in which case "host"
        # needs to be prepended back to the path
        if (urlh.host)
            urlh.path = (remove urlh.host) + urlh.path;

        urlh.path = substitute_env_vars(urlh.path);

        if (!absolute_path(urlh.path)) {
%ifdef Windows
            # here we have to guess where the "root" is
            urlh.path = "c:\\" + urlh.path;
%else
            urlh.path = "/" + urlh.path;
%endif
        }

        # sanitize / normalize the path
        urlh.path = normalize_dir(urlh.path);
    }

    #! Verifies that subtype info has been loaded from external modules
    /** @since ConnectionProvider 2.0
    */
    private checkSubtypeInfo() {
        if (!subtype_info) {
            loadSubtypeInfo();
        }
    }

    #! Returns a hash of supported data provider subtypes
    /** @return a hash of supported data provider subtypes

        @since ConnectionProvider 2.0
    */
    private *hash<string, *hash<string, hash<DataProviderOptionInfo>>> getDataProviderSubtypesImpl() {
        if (subtype_info) {
            return subtype_info;
        }
        loadSubtypeInfo();
        return subtype_info;
    }

    #! Loads subtype info from external modules
    private loadSubtypeInfo() {
        subtype_lck.lock();
        on_exit subtype_lck.unlock();
        # check again inside the lock
        if (subtype_info) {
            return;
        }
        foreach hash<auto> i in (DataProviderSubtypeMap.pairIterator()) {
            load_module(i.value."module");
            Class cls = Class::forName(i.value."option_class" ?? i.value."class");
            subtype_info{i.key} = cls.getConstant(i.value."options").getValue();
        }
    }

    #! returns a data provider object for connection subtype passed as an argument
    /** @param subtype the subtype of data provider supported, in case the connection can return multiple data
        providers
        @param constructor_options any additional constructor options required by the data provider

        @return a data provider object for the connection and subtype passed as an argument

        @throw DATA-PROVIDER-ERROR the given subtype is not valid
    */
    private AbstractDataProvider getDataProviderImpl(string subtype, *hash<auto> constructor_options) {
        *hash<auto> conf = DataProviderSubtypeMap{subtype};
        if (!conf) {
            throw "DATA-PROVIDER-ERROR", sprintf("file connections do not support data provider subtype %y; "
                "supported subtypes: %y", subtype, keys getDataProviderSubtypes());
        }
        if (opts.readonly && subtype =~ /write/i) {
            throw "DATA-PROVIDER-ERROR", sprintf("cannot return a %s data provider for readonly file connections",
                conf."class");
        }
        checkSubtypeInfo();
        constructor_options = {} + constructor_options;
        if (subtype_info{subtype}.path && !constructor_options.path) {
            constructor_options.path = urlh.path;
        }
        if (subtype_info{subtype}.readonly && !exists constructor_options.readonly) {
            constructor_options.readonly = True;
        }
        hash<auto> opts = getSubtypeOptions(subtype_info{subtype}, constructor_options);
        return create_object(conf."class", opts);
    }

    #! Returns a hash of data provider constructor option info, if applicable
    /** @return a hash of data provider constructor option info for the primary data provider, if applicable
    */
    private *hash<string, hash<DataProviderOptionInfo>> getDataProviderConstructorOptionsImpl() {
        load_module("FilePoller");
        Class cls = Class::forName("FilePollerDataProviderBase");
        return {
            "filepoller": cls.getConstant("ConstructorOptions").getValue(),
        };
    }

    #! returns a @ref Qore::Dir object
    /** @param connect if @ref True "True", then the target directory is checked for existence and if it's writable,
        if either check fails, an exception is thrown
        @param rtopts this connection type does not accept any runtime option, so this parameter is ignored

        @return a @ref Qore::Dir object
    */
    private Dir getImpl(bool connect = True, *hash rtopts) {
        Dir dir();
        if (urlh.path.val()) {
            dir.chdir(urlh.path);
        }
        if (connect) {
            if (!opts.readonly && (!is_writeable(urlh.path) || !dir.exists()))
                throw "CONNECTION-ERROR", sprintf("connection %y (type %y url %y) points to non-existent, "
                    "inaccessible, or read-only directory %y", name, urlh.protocol, url, urlh.path);
        }
        return dir;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
} # ConnectionProvider namespace
