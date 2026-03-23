# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Table class definition

/** Table.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! represents a database table; this class embeds an AbstractTable object that is created automatically in the constructor based on the database driver for the AbstractDatasource object providing the database connection
/** Driver-specific modules that provide the AbstractTable implementation embedded in this class are loaded on demand based on the driver's name.
    The driver-specific module's name is generated based on the db-driver's name with the first letter capitalized then with \c "SqlUtil" appended.

    @note SqlUtil::Table does not provide any functionality except it is able to create
            real low level AbstractTable inherited object. All functionality is provided
            by embedded AbstractTable object.

    For example:
    - \c "oracle": <a href="../../OracleSqlUtil/html/index.html">OracleSqlUtil</a>
    - \c "pgsql": <a href="../../PgsqlSqlUtil/html/index.html">PgsqlSqlUtil</a>
    - \c "mysql": <a href="../../MysqlSqlUtil/html/index.html">MysqlSqlUtil</a>

    etc.

    @note Objects of Table class can hold AbstractTable pointing to a view in Oracle.
*/
public class Table {
    private {
        #! the embedded AbstractTable object that actually provides the functionality for this class
        AbstractTable t;
    }

    #! creates the Table object
    /** @par Example:
        @code{.py}
Table table(ds, "table");
        @endcode

        @param ds the AbstractDatasource object used to provide the DB connection
        @param name the name of the table
        @param opts a hash of options for the table creation string; see @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional driver-specific options

        @throw TABLE-DRIVER-ERROR no database-specific module can be loaded
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(AbstractDatasource ds, string name, *hash<auto> opts) {
        t = AbstractTable::getTable(ds, name, opts);
    }

    #! creates the Table object
    /** @par Example:
        @code{.py}
Table table("pgsql:user/pass@db%host", "table");
        @endcode

        @param ds a datasource description string in the format that can be parsed by parse_datasource()
        @param name the name of the table
        @param opts a hash of options for the table creation string; see @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional driver-specific options

        @throw TABLE-DRIVER-ERROR no database-specific module can be loaded
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(string ds, string name, *hash<auto> opts) {
        t = AbstractTable::getTable(ds, name, opts);
    }

    #! creates the Table object
    /** @par Example:
        @code{.py}
Table table("pgsql:user/pass@db%host", "table");
        @endcode

        @param ds a hash giving parameters for the new datasource with the following possible keys (the \c "type" key is mandatory, also usable with the output of the parse_datasource() function):
        - \c type: (@ref string_or_nothing_type "*string") The name of the database driver to use; this key is mandatory; if not present, an exception will be raised. See @ref sql_constants for builtin constants for DBI drivers shipped with Qore, or see the DBI driver documentation to use an add-on driver (this string should be the name of the driver to be loaded)
        - \c user: (@ref string_or_nothing_type "*string") The user name for the new connection
        - \c pass: (@ref string_or_nothing_type "*string") The password for the new connection
        - \c db: (@ref string_or_nothing_type "*string") The database name for the new connection
        - \c charset: (@ref string_or_nothing_type "*string") The database-specific name of the character encoding to use for the new connection. Also see Qore::SQL::Datasource::setDBCharset() for a method that allows this parameter to be set after the constructor. If no value is passed for this parameter, then the database character encoding corresponding to the default character encoding for the %Qore process will be used instead.
        - \c host: (@ref string_or_nothing_type "*string") The host name for the new connection
        - \c port: (@ref softint_type "softint") The port number for the new connection
        - \c options: (@ref hash_or_nothing_type "*hash") Any options for the new connection
        @param name the name of the table
        @param opts a hash of options for the table creation string; see @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional driver-specific options

        @throw TABLE-DRIVER-ERROR no database-specific module can be loaded
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(hash<auto> ds, string name, *hash<auto> opts) {
        t = AbstractTable::getTable(ds, name, opts);
    }

    #! creates the object from a table description hash
    /** @param ds the AbstractDatasource for the connection to the database
        @param desc a @ref table_desc_hash "table description hash" describing the table
        @param name the name of the table
        @param opts an optional hash of options for the table creation string; see @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw DESCRIPTION-ERROR invalid or unsupported description hash value passed
    */
    constructor(AbstractDatasource ds, hash<auto> desc, string name, *hash<auto> opts) {
        t = AbstractTable::getTable(ds, name);
        t.setupTable(desc, opts);
    }

    #! returns the AbstractTable object contained by this object
    AbstractTable getTable() {
        return t;
    }

    #! executes a method on the contained AbstractTable object
    /** See @ref SqlUtil::AbstractTable API for reference
        */
    auto methodGate(string meth) {
        return call_object_method_args(t, meth, argv);
    }

} # class Table
}
