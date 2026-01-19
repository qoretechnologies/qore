# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore FtpConnection class definition

/*  FtpConnection.qc Copyright 2016 - 2024 Qore Technologies, s.r.o.

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

#! the ConnectionProvider namespace. All classes used in the ConnectionProvider module should be inside this namespace
public namespace ConnectionProvider {
#! class for FTP connections; returns @ref Qore::FtpClient objects
/** supports the following runtime options in getImpl():
    - \c "path": overrides the path component in the URL at runtime
    - \c "path_add": appends the given string to the path component of the URL at runtime
*/
public class FtpConnection inherits AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "FTP Connection",
            "short_desc": "A connection to an FTP server",
            "desc": "A connection to an FTP server with the `ftp` or `ftps` (secure, not to be confused with sftp) "
                "protocols",
            "cls": Class::forName("FtpConnection"),
            "schemes": {
                "ftps": True,
                "ftp": True,
            },
            "options": HttpConnection::ConnectionScheme.options + {
                "password": <ConnectionOptionInfo>{
                    "display_name": "Password",
                    "short_desc": "Sets the password for authentication",
                    "type": "string",
                    "desc": "The password for authentication; only used if no username or password is set in the URL "
                        "and if the `username` option is also used",
                    "sensitive": True,
                },
                "username": <ConnectionOptionInfo>{
                    "display_name": "Username",
                    "short_desc": "Sets the username for authentication",
                    "type": "string",
                    "desc": "The username for authentication; only used if no username or password is set in the URL "
                        "and if the `password` option is also used",
                },
            },
        };

        #! Data provider subtypes supported
        const DataProviderSubtypeMap = {
            "ftppoller": {
                "module": "FtpPoller",
                "class": "FtpDelayedPollerDataProvider",
                "option_class": "FtpPollerDataProviderBase",
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

    #! creates the FtpConnection object
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
            *Logger logger) : AbstractConnectionWithInfo(name, description, url, attributes, options, logger) {
    }

    #! creates the FtpConnection object
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
    }

    #! returns \c "ftp"
    string getType() {
        return "ftp";
    }

    #! returns runtime options
    /** return a hash with the following keys reflecting support for the corresponding runtime options in getImpl():
        - \c "path": overrides the path component in the URL at runtime
        - \c "path_add": appends the given string to the path component of the URL at runtime
    */
    *hash<auto> getRuntimeOptions() {
        return {
            "path": True,
            "path_add": True,
        };
    }

    #! returns an FtpClientDataProvider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return an FtpClientDataProvider object for this connection

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        # to avoid circular dependencies, this object loads the FtpDataProvider and creates the data provider
        # object dynamically
        load_module("FtpClientDataProvider");
        return create_object("FtpClientDataProvider", get());
    }

    #! returns @ref True, as the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the \c getDataProvider() method

        @see \c getDataProvider()
    */
    bool hasDataProvider() {
        return True;
    }

    #! returns a data provider object for the connection and subtype passed as an argument
    /** @param subtype the subtype of data provider supported, in case the connection can return multiple data
        providers
        @param constructor_options any additional constructor options required by the data provider

        @return a data provider object for the connection and subtype passed as an argument

        @throw DATA-PROVIDER-ERROR this object does not support the data provider API or the given subtype is not
        valid

        @see
        - hasDataProvider()
        - getDataProviderSubtypes()

        @since ConnectionProvider 2.0
    */
    private AbstractDataProvider getDataProviderImpl(string subtype, *hash<auto> constructor_options) {
        *hash<auto> conf = DataProviderSubtypeMap{subtype};
        if (!conf) {
            throw "DATA-PROVIDER-ERROR", sprintf("ftp connections do not support data provider subtype %y; supported "
                "subtypes: %y", subtype, keys getDataProviderSubtypes());
        }
        checkSubtypeInfo();
        hash<auto> opts = {
            "url": url,
        } + getSubtypeOptions(subtype_info{subtype}, constructor_options);
        return create_object(conf."class", opts);
    }

    #! Sets child data provider capabilities
    private setChildCapabilities() {
        children_can_support_apis = True;
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

    #! Returns an @ref Qore::FtpClient "FtpClient" object
    /** @param connect if @ref True "True" then a connection is made any if there is any path in the URL a \c CWD
        operation is performed to change to the target directory
        @param rtopts this connection type does not accept any runtime option, so this parameter is ignored
    */
    private FtpClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # set runtime path
        *string rtpath = rtopts.path ?* urlh.path;
        if (rtopts.path_add.val()) {
            if (rtpath !~ /\/$/ && rtopts.path_add !~ /^\//) {
                rtpath += "/";
            }
            rtpath += rtopts.path_add;
        }

        string url = self.url;
        if (opts.username.val() && opts.password.val() && (!urlh.username.val() || !urlh.password.val())) {
            url = sprintf("%s://%s:%s@%s", urlh.protocol, opts.username, opts.password, urlh.host);
            if (urlh.port) {
                url += sprintf(":%d", urlh.port);
            }
        }

        # create the object and perform post processing if necessary
        FtpClient fc(url);
        if (connect) {
            fc.connect();
            if (rtpath.val()) {
                fc.cwd(rtpath);
                fc.path = rtpath;
            }
        }
        return fc;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
} # ConnectionProvider namespace
